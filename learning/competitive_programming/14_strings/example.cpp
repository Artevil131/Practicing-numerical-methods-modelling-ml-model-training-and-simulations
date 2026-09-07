// Chapter 14 — String algorithms: reference library.
//
// Every structure of lesson.md as a self-contained snippet, each cross-checked in main()
// against a brute force on random strings. This file is TRAINING MATERIAL: after reading
// the lesson, close it and re-type each snippet from memory, then diff against this file.
//
// Build:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// Sections (search for "// ==="):
//   1. polynomial hashing mod 2^61-1 (substring hash, LCP by binary search, distinct substrings)
//   2. Z-function
//   3. prefix function / KMP (occurrences, periods & borders, automaton)
//   4. trie (array-based) + "Word Combinations" DP
//   5. Aho–Corasick (count occurrences of many patterns; DP avoiding patterns)
//   6. suffix array O(n log n) + Kasai LCP (distinct substrings, longest repeat, pattern search)
//   7. suffix automaton (distinct substrings, occurrence counts, LCS, k-th substring)
//   8. Manacher (odd/even palindrome radii)
//   9. palindromic tree (eertree)
//  10. Lyndon factorization / Duval (minimal rotation)
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
using namespace std;
using ll = long long;
using ull = unsigned long long;

static const ll MOD = 1'000'000'007LL;
static mt19937_64 rng(20260905);
static ll rnd(ll lo, ll hi) { return uniform_int_distribution<ll>(lo, hi)(rng); }
static string rand_string(int n, int alpha) {
    string s;
    for (int i = 0; i < n; i++) s += char('a' + rnd(0, alpha - 1));
    return s;
}

// =====================================================================================
// 1. Polynomial hashing mod p = 2^61 - 1 with a random base
// =====================================================================================
// h(s) = sum s[i] * B^(n-1-i) mod p. Prefix hashes give any substring hash in O(1):
// hash(l, r) = pre[r] - pre[l] * B^(r-l). p is a Mersenne prime: mulmod needs no division,
// and the collision probability per comparison is ~ n / 2^61 — anti-hash tests cannot be
// prepared against a base chosen at runtime.
struct Hash61 {
    static constexpr ull P = (1ULL << 61) - 1;
    static ull mulmod(ull a, ull b) {
        __uint128_t c = (__uint128_t)a * b;
        ull r = (ull)(c & P) + (ull)(c >> 61);
        r = (r & P) + (r >> 61);
        return r >= P ? r - P : r;
    }
    static ull base() {  // one random base per program run
        static ull B = uniform_int_distribution<ull>(256, P - 2)(rng);
        return B;
    }
    vector<ull> pre, pw;
    explicit Hash61(const string& s) : pre(s.size() + 1, 0), pw(s.size() + 1, 1) {
        ull B = base();
        for (size_t i = 0; i < s.size(); i++) {
            pre[i + 1] = (mulmod(pre[i], B) + (unsigned char)s[i]) % P;
            pw[i + 1] = mulmod(pw[i], B);
        }
    }
    ull get(int l, int r) const {  // hash of s[l, r)
        ull v = pre[r] + P - mulmod(pre[l], pw[r - l]);
        return v >= P ? v - P : v;
    }
};

// LCP of suffixes i and j by binary search on hashes: O(log n).
int lcp_hash(const Hash61& h, int n, int i, int j) {
    int lo = 0, hi = n - max(i, j);
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (h.get(i, i + mid) == h.get(j, j + mid)) lo = mid; else hi = mid - 1;
    }
    return lo;
}

// =====================================================================================
// 2. Z-function: z[i] = length of the longest common prefix of s and s[i..]
// =====================================================================================
// Invariant: [l, r) is the rightmost "Z-box" found so far (s[l..r) == s[0..r-l)).
// For i < r we already know s[i..r) == s[i-l..r-l), so z[i] >= min(r-i, z[i-l]);
// then extend by direct comparison. Each comparison that succeeds moves r -> O(n).
vector<int> z_function(const string& s) {
    int n = (int)s.size();
    vector<int> z(n, 0);
    for (int i = 1, l = 0, r = 0; i < n; i++) {
        if (i < r) z[i] = min(r - i, z[i - l]);
        while (i + z[i] < n && s[z[i]] == s[i + z[i]]) z[i]++;
        if (i + z[i] > r) { l = i; r = i + z[i]; }
    }
    if (n) z[0] = n;
    return z;
}

// =====================================================================================
// 3. Prefix function / KMP
// =====================================================================================
// pi[i] = length of the longest proper border of s[0..i] (prefix == suffix, != whole).
// Candidate lengths for pi[i] are pi[i-1]+1, pi[pi[i-1]-1]+1, ... (each border of a
// border is a border). Amortised O(n): pi grows by at most 1 per step, each while
// iteration decreases it.
vector<int> prefix_function(const string& s) {
    int n = (int)s.size();
    vector<int> pi(n, 0);
    for (int i = 1; i < n; i++) {
        int k = pi[i - 1];
        while (k > 0 && s[i] != s[k]) k = pi[k - 1];
        if (s[i] == s[k]) k++;
        pi[i] = k;
    }
    return pi;
}

// All occurrence start positions of pattern p in text t (overlaps allowed), O(|p|+|t|).
vector<int> kmp_find(const string& t, const string& p) {
    vector<int> pi = prefix_function(p), res;
    int m = (int)p.size();
    if (m == 0) return res;
    for (int i = 0, k = 0; i < (int)t.size(); i++) {
        while (k > 0 && (k == m || t[i] != p[k])) k = pi[k - 1];
        if (t[i] == p[k]) k++;
        if (k == m) res.push_back(i - m + 1);
    }
    return res;
}

// Borders of s in increasing length: pi[n-1], pi[pi[n-1]-1], ...  Period q of s means
// s[i] == s[i+q]; q is a period iff n-q is a border, so smallest period = n - pi[n-1].
vector<int> borders(const string& s) {
    vector<int> pi = prefix_function(s), res;
    for (int k = s.empty() ? 0 : pi.back(); k > 0; k = pi[k - 1]) res.push_back(k);
    reverse(res.begin(), res.end());
    return res;
}

// KMP automaton: aut[state][c] = matched length after reading c in state "state".
vector<vector<int>> kmp_automaton(const string& p, int alpha) {
    int m = (int)p.size();
    vector<int> pi = prefix_function(p);
    vector<vector<int>> aut(m + 1, vector<int>(alpha, 0));
    for (int s = 0; s <= m; s++)
        for (int c = 0; c < alpha; c++) {
            if (s < m && p[s] - 'a' == c) aut[s][c] = s + 1;
            else aut[s][c] = s == 0 ? 0 : aut[pi[s - 1]][c];
        }
    return aut;
}

// =====================================================================================
// 4. Trie (array-based) + Word Combinations DP
// =====================================================================================
struct Trie {
    static const int K = 26;
    vector<array<int, K>> nxt;  // -1 = no child
    vector<int> cnt;            // number of words ending here
    Trie() { new_node(); }
    int new_node() { array<int, K> a; a.fill(-1); nxt.push_back(a); cnt.push_back(0); return (int)nxt.size() - 1; }
    void insert(const string& w) {
        int v = 0;
        for (char ch : w) {
            int c = ch - 'a';
            if (nxt[v][c] == -1) { int u = new_node(); nxt[v][c] = u; }
            v = nxt[v][c];
        }
        cnt[v]++;
    }
};

// Number of ways to write s as a concatenation of dictionary words (mod).
// dp[i] = ways for prefix s[0..i); push forward along the trie from i. O(n * maxlen).
ll word_combinations(const string& s, const vector<string>& dict) {
    Trie tr;
    for (auto& w : dict) tr.insert(w);
    int n = (int)s.size();
    vector<ll> dp(n + 1, 0);
    dp[0] = 1;
    for (int i = 0; i < n; i++) {
        if (!dp[i]) continue;
        int v = 0;
        for (int j = i; j < n; j++) {
            v = tr.nxt[v][s[j] - 'a'];
            if (v == -1) break;
            if (tr.cnt[v]) dp[j + 1] = (dp[j + 1] + dp[i] * tr.cnt[v]) % MOD;
        }
    }
    return dp[n];
}

// =====================================================================================
// 5. Aho–Corasick
// =====================================================================================
// Trie of patterns + suffix links (link[v] = longest proper suffix of v that is a trie
// node) + full goto function (automaton). exitl[v] = nearest node on the link chain that
// ends a pattern (for enumerating all matches at a position). Build O(total * alpha).
struct AhoCorasick {
    static const int K = 26;
    vector<array<int, K>> go;
    vector<int> link, exitl, term;  // term[v] = index of a pattern ending exactly at v (-1)
    vector<int> order;              // BFS order (for propagating counts to suffixes)
    AhoCorasick() { add_node(); }
    int add_node() { array<int, K> a; a.fill(-1); go.push_back(a); link.push_back(0); exitl.push_back(-1); term.push_back(-1); return (int)go.size() - 1; }
    int insert(const string& w, int id) {
        int v = 0;
        for (char ch : w) {
            int c = ch - 'a';
            if (go[v][c] == -1) { int u = add_node(); go[v][c] = u; }
            v = go[v][c];
        }
        term[v] = id;
        return v;
    }
    void build() {
        queue<int> q;
        for (int c = 0; c < K; c++) {
            if (go[0][c] == -1) go[0][c] = 0;
            else { link[go[0][c]] = 0; q.push(go[0][c]); }
        }
        while (!q.empty()) {
            int v = q.front(); q.pop();
            order.push_back(v);
            exitl[v] = term[link[v]] != -1 ? link[v] : exitl[link[v]];
            for (int c = 0; c < K; c++) {
                int u = go[v][c];
                if (u == -1) go[v][c] = go[link[v]][c];  // missing edge: follow the link
                else { link[u] = go[link[v]][c]; q.push(u); }
            }
        }
    }
    // Occurrences of every pattern in t: visit count per node, then push counts up the
    // suffix links in reverse BFS order (a node's occurrences are also occurrences of
    // every suffix-link ancestor). O(|t| + nodes).
    vector<ll> count_all(const string& t, int npat, const vector<int>& endNode) {
        vector<ll> cnt(go.size(), 0);
        int v = 0;
        for (char ch : t) { v = go[v][ch - 'a']; cnt[v]++; }
        for (int i = (int)order.size() - 1; i >= 0; i--) cnt[link[order[i]]] += cnt[order[i]];
        vector<ll> res(npat);
        for (int i = 0; i < npat; i++) res[i] = cnt[endNode[i]];
        return res;
    }
};

// Number of strings of length n over the first `alpha` letters containing NO pattern.
// A node is forbidden if it or any suffix-link ancestor ends a pattern.
ll count_avoiding(int n, int alpha, const vector<string>& pats) {
    AhoCorasick ac;
    for (size_t i = 0; i < pats.size(); i++) ac.insert(pats[i], (int)i);
    ac.build();
    int V = (int)ac.go.size();
    vector<char> bad(V, 0);
    bad[0] = ac.term[0] != -1;
    for (int v : ac.order) bad[v] = ac.term[v] != -1 || bad[ac.link[v]];
    vector<ll> dp(V, 0), nd(V);
    if (!bad[0]) dp[0] = 1;
    for (int i = 0; i < n; i++) {
        fill(nd.begin(), nd.end(), 0);
        for (int v = 0; v < V; v++) if (dp[v])
            for (int c = 0; c < alpha; c++) {
                int u = ac.go[v][c];
                if (!bad[u]) nd[u] = (nd[u] + dp[v]) % MOD;
            }
        dp = nd;
    }
    ll total = 0;
    for (ll x : dp) total = (total + x) % MOD;
    return total;
}

// =====================================================================================
// 6. Suffix array in O(n log n) (cyclic shifts, counting sort) + Kasai LCP
// =====================================================================================
// Round h sorts cyclic shifts by their first 2^h characters: a shift is a pair
// (class of first half, class of second half); sort by second half (already sorted
// order shifted by -2^h), then stable counting sort by first half. A sentinel smaller
// than every character makes cyclic shifts order = suffix order.
vector<int> suffix_array(const string& str) {
    string s = str + '\x01';  // input must not contain '\x00' or '\x01'
    int n = (int)s.size(), classes = 256;
    vector<int> p(n), c(n), cnt(max(classes, n), 0), pn(n), cn(n);
    for (char ch : s) cnt[(unsigned char)ch]++;
    for (int i = 1; i < classes; i++) cnt[i] += cnt[i - 1];
    for (int i = n - 1; i >= 0; i--) p[--cnt[(unsigned char)s[i]]] = i;
    c[p[0]] = 0; classes = 1;
    for (int i = 1; i < n; i++) { if (s[p[i]] != s[p[i - 1]]) classes++; c[p[i]] = classes - 1; }
    for (int k = 1; k < n; k <<= 1) {
        for (int i = 0; i < n; i++) { pn[i] = p[i] - k; if (pn[i] < 0) pn[i] += n; }
        fill(cnt.begin(), cnt.begin() + classes, 0);
        for (int i = 0; i < n; i++) cnt[c[pn[i]]]++;
        for (int i = 1; i < classes; i++) cnt[i] += cnt[i - 1];
        for (int i = n - 1; i >= 0; i--) p[--cnt[c[pn[i]]]] = pn[i];
        cn[p[0]] = 0; classes = 1;
        for (int i = 1; i < n; i++) {
            pair<int, int> cur{c[p[i]], c[(p[i] + k) % n]}, prv{c[p[i - 1]], c[(p[i - 1] + k) % n]};
            if (cur != prv) classes++;
            cn[p[i]] = classes - 1;
        }
        c.swap(cn);
        if (classes == n) break;  // all distinct: sorted
    }
    p.erase(p.begin());  // drop the sentinel suffix
    return p;
}

// Kasai: lcp[i] = LCP(suffix sa[i], suffix sa[i+1]). Key fact: if suffix i has LCP h
// with its SA-neighbour, suffix i+1 has LCP >= h-1 with ITS neighbour, so the total
// number of character comparisons is O(n).
vector<int> kasai(const string& s, const vector<int>& sa) {
    int n = (int)s.size();
    vector<int> rk(n), lcp(max(n - 1, 0), 0);
    for (int i = 0; i < n; i++) rk[sa[i]] = i;
    for (int i = 0, k = 0; i < n; i++) {
        if (rk[i] == n - 1) { k = 0; continue; }
        int j = sa[rk[i] + 1];
        while (i + k < n && j + k < n && s[i + k] == s[j + k]) k++;
        lcp[rk[i]] = k;
        if (k) k--;
    }
    return lcp;
}

// Number of occurrences of p in s via binary search on the suffix array: O(|p| log n).
int sa_count(const string& s, const vector<int>& sa, const string& p) {
    auto cmp = [&](int suf, const string& q) {  // suffix < q ?
        return s.compare(suf, q.size(), q) < 0;
    };
    int n = (int)s.size();
    int lo = int(lower_bound(sa.begin(), sa.end(), p, cmp) - sa.begin());
    int hi = lo;
    while (hi < n && s.compare(sa[hi], p.size(), p) == 0) hi++;
    return hi - lo;
}

// =====================================================================================
// 7. Suffix automaton
// =====================================================================================
// States = endpos-equivalence classes of substrings. Each state v represents substrings
// of lengths (len[link[v]], len[v]]. Adding a character creates at most 2 states, and
// the total work is O(n) because each step of the two while-loops shortens a chain that
// only grows by O(1) per extension (amortised argument in lesson.md).
struct SuffixAutomaton {
    static const int K = 26;
    struct State { int len, link; array<int, K> nxt; ll cnt; };
    vector<State> st;
    int last = 0;
    SuffixAutomaton() { State root{0, -1, {}, 0}; root.nxt.fill(-1); st.push_back(root); }
    void extend(int c) {
        int cur = (int)st.size();
        State ns{st[last].len + 1, -1, {}, 1};
        ns.nxt.fill(-1);
        st.push_back(ns);
        int p = last;
        while (p != -1 && st[p].nxt[c] == -1) { st[p].nxt[c] = cur; p = st[p].link; }
        if (p == -1) st[cur].link = 0;
        else {
            int q = st[p].nxt[c];
            if (st[p].len + 1 == st[q].len) st[cur].link = q;
            else {
                int clone = (int)st.size();
                State cl = st[q];
                cl.len = st[p].len + 1;
                cl.cnt = 0;  // clone is not an end position itself
                st.push_back(cl);
                while (p != -1 && st[p].nxt[c] == q) { st[p].nxt[c] = clone; p = st[p].link; }
                st[q].link = st[cur].link = clone;
            }
        }
        last = cur;
    }
    explicit SuffixAutomaton(const string& s) : SuffixAutomaton() { for (char ch : s) extend(ch - 'a'); finish(); }
    vector<int> byLenDesc;
    void finish() {  // occurrence counts: propagate cnt to suffix links in decreasing len
        int V = (int)st.size();
        byLenDesc.resize(V);
        iota(byLenDesc.begin(), byLenDesc.end(), 0);
        sort(byLenDesc.begin(), byLenDesc.end(), [&](int a, int b) { return st[a].len > st[b].len; });
        for (int v : byLenDesc) if (st[v].link != -1) st[st[v].link].cnt += st[v].cnt;
    }
    ll distinct_substrings() const {
        ll r = 0;
        for (size_t v = 1; v < st.size(); v++) r += st[v].len - st[st[v].link].len;
        return r;
    }
    ll occurrences(const string& p) const {  // number of occurrences of p
        int v = 0;
        for (char ch : p) { v = st[v].nxt[ch - 'a']; if (v == -1) return 0; }
        return st[v].cnt;
    }
    int longest_common_substring(const string& t) const {
        int v = 0, l = 0, best = 0;
        for (char ch : t) {
            int c = ch - 'a';
            while (v != 0 && st[v].nxt[c] == -1) { v = st[v].link; l = st[v].len; }
            if (st[v].nxt[c] != -1) { v = st[v].nxt[c]; l++; }
            best = max(best, l);
        }
        return best;
    }
    // k-th (1-based) distinct substring in lexicographic order. paths[v] = number of
    // distinct substrings that start by walking out of v (non-empty paths from v).
    string kth_substring(ll k) const {
        int V = (int)st.size();
        vector<ll> paths(V, 0);
        // paths[v] = 1 (stop here) + sum over children; so paths[u] = number of distinct
        // substrings whose walk passes through edge (v -> u), i.e. that start with that letter.
        for (int v : byLenDesc) { paths[v] = 1; for (int c = 0; c < K; c++) if (st[v].nxt[c] != -1) paths[v] += paths[st[v].nxt[c]]; }
        string res;
        int v = 0;
        while (k > 0) {
            for (int c = 0; c < K; c++) {
                int u = st[v].nxt[c];
                if (u == -1) continue;
                if (k <= paths[u]) { res += char('a' + c); k--; v = u; break; }
                k -= paths[u];
            }
        }
        return res;
    }
};

// =====================================================================================
// 8. Manacher
// =====================================================================================
// d1[i] = number of odd palindromes centred at i  (radius r -> palindrome [i-r+1, i+r-1])
// d2[i] = number of even palindromes centred between i-1 and i ([i-r, i+r-1]).
// Mirror trick: inside the rightmost known palindrome [l, r], the answer at i is at least
// min(d[mirror of i], distance to r); extend from there. Each extension moves r -> O(n).
vector<int> manacher_odd(const string& s) {
    int n = (int)s.size();
    vector<int> d1(n, 0);
    for (int i = 0, l = 0, r = -1; i < n; i++) {
        int k = i > r ? 1 : min(d1[l + r - i], r - i + 1);
        while (0 <= i - k && i + k < n && s[i - k] == s[i + k]) k++;
        d1[i] = k--;
        if (i + k > r) { l = i - k; r = i + k; }
    }
    return d1;
}
vector<int> manacher_even(const string& s) {
    int n = (int)s.size();
    vector<int> d2(n, 0);
    for (int i = 0, l = 0, r = -1; i < n; i++) {
        int k = i > r ? 0 : min(d2[l + r - i + 1], r - i + 1);
        while (0 <= i - k - 1 && i + k < n && s[i - k - 1] == s[i + k]) k++;
        d2[i] = k--;
        if (i + k > r) { l = i - k - 1; r = i + k; }
    }
    return d2;
}

// =====================================================================================
// 9. Palindromic tree (eertree)
// =====================================================================================
// One node per distinct palindromic substring (at most n). Node 0: imaginary root of
// length -1, node 1: empty string. nxt[v][c] = palindrome c+v+c. link[v] = longest proper
// palindromic suffix. Adding s[i]: from the longest palindromic suffix of s[0..i), follow
// links until s[i - len - 1] == s[i]. Amortised O(n) (the "last" pointer depth argument).
struct Eertree {
    static const int K = 26;
    vector<array<int, K>> nxt;
    vector<int> len, link, cnt;  // cnt[v] = occurrences ending at each position (before propagation)
    string s;
    int last = 1;
    Eertree() {
        array<int, K> z; z.fill(0);
        nxt.assign(2, z);
        len = {-1, 0}; link = {0, 0}; cnt = {0, 0};
    }
    int get_link(int v, int i) const {
        while (i - len[v] - 1 < 0 || s[i - len[v] - 1] != s[i]) v = link[v];
        return v;
    }
    void add(char ch) {
        s += ch;
        int i = (int)s.size() - 1, c = ch - 'a';
        int v = get_link(last, i);
        if (!nxt[v][c]) {
            int nw = (int)len.size();
            int nlen = len[v] + 2;
            int lk = nlen == 1 ? 1 : nxt[get_link(link[v], i)][c];
            array<int, K> z; z.fill(0);
            nxt.push_back(z); len.push_back(nlen); link.push_back(lk); cnt.push_back(0);
            nxt[v][c] = nw;
        }
        last = nxt[v][c];
        cnt[last]++;
    }
    int distinct_palindromes() const { return (int)len.size() - 2; }
    // total number of palindromic substrings (with multiplicity): propagate cnt along links
    ll total_palindromes() {
        ll total = 0;
        vector<ll> c(cnt.begin(), cnt.end());
        for (int v = (int)len.size() - 1; v >= 2; v--) { total += c[v]; c[link[v]] += c[v]; }
        return total;
    }
};

// =====================================================================================
// 10. Lyndon factorization (Duval) and minimal rotation
// =====================================================================================
// s = w1 w2 ... wk with each wi a Lyndon word (strictly smaller than all its proper
// suffixes) and w1 >= w2 >= ... >= wk. Duval scans with (i, j, k): s[i..j) is a
// "pre-Lyndon" word (a Lyndon word repeated, plus a prefix); s[k] is compared to s[j].
vector<string> duval(const string& s) {
    int n = (int)s.size(), i = 0;
    vector<string> fac;
    while (i < n) {
        int j = i + 1, k = i;
        while (j < n && s[k] <= s[j]) { if (s[k] < s[j]) k = i; else k++; j++; }
        while (i <= k) { fac.push_back(s.substr(i, j - k)); i += j - k; }
    }
    return fac;
}

// Start index of the lexicographically smallest rotation: run Duval on s+s and take the
// start of the Lyndon factor that begins in [0, n) and reaches furthest.
int min_rotation(const string& s) {
    string t = s + s;
    int n = (int)t.size(), i = 0, ans = 0;
    while (i < n / 2) {
        ans = i;
        int j = i + 1, k = i;
        while (j < n && t[k] <= t[j]) { if (t[k] < t[j]) k = i; else k++; j++; }
        while (i <= k) i += j - k;
    }
    return ans;
}

// =====================================================================================
// main: tests
// =====================================================================================
int main() {
    // ---- 1. hashing
    for (int it = 0; it < 100; it++) {
        int n = (int)rnd(1, 40);
        string s = rand_string(n, 2);
        Hash61 h(s);
        for (int q = 0; q < 50; q++) {
            int l1 = (int)rnd(0, n - 1), l2 = (int)rnd(0, n - 1);
            int len = (int)rnd(0, n - max(l1, l2));
            assert((h.get(l1, l1 + len) == h.get(l2, l2 + len)) == (s.compare(l1, len, s, l2, len) == 0));
            int naive = 0;
            while (l1 + naive < n && l2 + naive < n && s[l1 + naive] == s[l2 + naive]) naive++;
            assert(lcp_hash(h, n, l1, l2) == naive);
        }
        set<ull> hs;
        set<string> ss;
        for (int l = 0; l < n; l++) for (int r = l + 1; r <= n; r++) { hs.insert(h.get(l, r)); ss.insert(s.substr(l, r - l)); }
        assert(hs.size() == ss.size());
    }
    // ---- 2. Z-function
    for (int it = 0; it < 200; it++) {
        string s = rand_string((int)rnd(0, 30), 2);
        auto z = z_function(s);
        for (int i = 0; i < (int)s.size(); i++) {
            int k = 0;
            while (i + k < (int)s.size() && s[k] == s[i + k]) k++;
            assert(z[i] == k);
        }
    }
    // ---- 3. KMP
    for (int it = 0; it < 200; it++) {
        string t = rand_string((int)rnd(0, 40), 2), p = rand_string((int)rnd(1, 4), 2);
        auto occ = kmp_find(t, p);
        vector<int> want;
        for (size_t pos = t.find(p); pos != string::npos; pos = t.find(p, pos + 1)) want.push_back((int)pos);
        assert(occ == want);
        // borders / periods
        string s = rand_string((int)rnd(1, 20), 2);
        auto b = borders(s);
        vector<int> wantB;
        for (int k = 1; k < (int)s.size(); k++) if (s.compare(0, k, s, s.size() - k, k) == 0) wantB.push_back(k);
        assert(b == wantB);
        int n = (int)s.size();
        int period = n - (b.empty() ? 0 : b.back());
        for (int i = 0; i + period < n; i++) assert(s[i] == s[i + period]);
        // automaton agrees with the online matcher
        auto aut = kmp_automaton(p, 2);
        int st = 0; vector<int> occ2;
        for (int i = 0; i < (int)t.size(); i++) { st = aut[st][t[i] - 'a']; if (st == (int)p.size()) occ2.push_back(i - (int)p.size() + 1); }
        assert(occ2 == want);
    }
    // ---- 4. trie / Word Combinations
    for (int it = 0; it < 100; it++) {
        string s = rand_string((int)rnd(1, 10), 2);
        int k = (int)rnd(1, 5);
        vector<string> dict;
        for (int i = 0; i < k; i++) dict.push_back(rand_string((int)rnd(1, 3), 2));
        function<ll(int)> rec = [&](int i) -> ll {
            if (i == (int)s.size()) return 1;
            ll r = 0;
            for (auto& w : dict) if (s.compare(i, w.size(), w) == 0 && i + w.size() <= s.size()) r += rec(i + (int)w.size());
            return r;
        };
        assert(word_combinations(s, dict) == rec(0) % MOD);
    }
    // ---- 5. Aho–Corasick
    for (int it = 0; it < 100; it++) {
        string t = rand_string((int)rnd(0, 40), 2);
        int k = (int)rnd(1, 5);
        vector<string> pats;
        for (int i = 0; i < k; i++) pats.push_back(rand_string((int)rnd(1, 4), 2));
        AhoCorasick ac;
        vector<int> endNode;
        for (int i = 0; i < k; i++) endNode.push_back(ac.insert(pats[i], i));
        ac.build();
        auto got = ac.count_all(t, k, endNode);
        for (int i = 0; i < k; i++) {
            ll want = 0;
            for (size_t pos = t.find(pats[i]); pos != string::npos; pos = t.find(pats[i], pos + 1)) want++;
            assert(got[i] == want);
        }
        int n = (int)rnd(0, 8);
        ll brute = 0;
        for (int code = 0; code < (1 << n); code++) {
            string s;
            for (int i = 0; i < n; i++) s += char('a' + (code >> i & 1));
            bool ok = true;
            for (auto& p : pats) if (s.find(p) != string::npos) ok = false;
            brute += ok;
        }
        assert(count_avoiding(n, 2, pats) == brute % MOD);
    }
    // ---- 6. suffix array + Kasai
    for (int it = 0; it < 150; it++) {
        int n = (int)rnd(1, 40);
        string s = rand_string(n, it % 3 == 0 ? 1 : 2);
        auto sa = suffix_array(s);
        vector<int> want(n);
        iota(want.begin(), want.end(), 0);
        sort(want.begin(), want.end(), [&](int a, int b) { return s.compare(a, string::npos, s, b, string::npos) < 0; });
        assert(sa == want);
        auto lcp = kasai(s, sa);
        ll distinct = (ll)n * (n + 1) / 2;
        int longestRepeat = 0;
        for (int i = 0; i + 1 < n; i++) {
            int k = 0;
            while (sa[i] + k < n && sa[i + 1] + k < n && s[sa[i] + k] == s[sa[i + 1] + k]) k++;
            assert(lcp[i] == k);
            distinct -= k;
            longestRepeat = max(longestRepeat, k);
        }
        set<string> ss;
        for (int l = 0; l < n; l++) for (int r = l + 1; r <= n; r++) ss.insert(s.substr(l, r - l));
        assert(distinct == (ll)ss.size());
        int bruteRepeat = 0;
        for (auto& sub : ss) if (s.find(sub, s.find(sub) + 1) != string::npos) bruteRepeat = max(bruteRepeat, (int)sub.size());
        assert(longestRepeat == bruteRepeat);
        string p = rand_string((int)rnd(1, 3), 2);
        int cntWant = 0;
        for (size_t pos = s.find(p); pos != string::npos; pos = s.find(p, pos + 1)) cntWant++;
        assert(sa_count(s, sa, p) == cntWant);
        // ---- 7. suffix automaton on the same string
        SuffixAutomaton sam(s);
        assert(sam.distinct_substrings() == (ll)ss.size());
        assert(sam.occurrences(p) == cntWant);
        string t = rand_string((int)rnd(0, 20), 2);
        int lcsWant = 0;
        for (auto& sub : ss) if (t.find(sub) != string::npos) lcsWant = max(lcsWant, (int)sub.size());
        assert(sam.longest_common_substring(t) == lcsWant);
        ll kk = rnd(1, (ll)ss.size());
        auto itk = ss.begin();
        advance(itk, kk - 1);
        assert(sam.kth_substring(kk) == *itk);
        // ---- 8. Manacher on the same string
        auto d1 = manacher_odd(s), d2 = manacher_even(s);
        ll palCount = 0;
        int longestPal = 0;
        for (int i = 0; i < n; i++) { palCount += d1[i] + d2[i]; longestPal = max({longestPal, 2 * d1[i] - 1, 2 * d2[i]}); }
        ll palBrute = 0;
        int longestBrute = 0;
        set<string> palSet;
        for (int l = 0; l < n; l++) for (int r = l; r < n; r++) {
            bool pal = true;
            for (int a = l, b = r; a < b; a++, b--) if (s[a] != s[b]) pal = false;
            if (pal) { palBrute++; longestBrute = max(longestBrute, r - l + 1); palSet.insert(s.substr(l, r - l + 1)); }
        }
        assert(palCount == palBrute && longestPal == longestBrute);
        // ---- 9. eertree
        Eertree et;
        for (char ch : s) et.add(ch);
        assert(et.distinct_palindromes() == (int)palSet.size());
        assert(et.total_palindromes() == palBrute);
        // ---- 10. Duval / minimal rotation
        auto fac = duval(s);
        string cat;
        for (size_t i = 0; i < fac.size(); i++) {
            cat += fac[i];
            for (size_t j = 1; j < fac[i].size(); j++) assert(fac[i] < fac[i].substr(j));  // Lyndon
            if (i) assert(fac[i - 1] >= fac[i]);
        }
        assert(cat == s);
        int rot = min_rotation(s);
        string best = s;
        for (int i = 0; i < n; i++) best = min(best, s.substr(i) + s.substr(0, i));
        assert(s.substr(rot) + s.substr(0, rot) == best);
    }
    puts("all chapter 14 tests passed");
    return 0;
}
