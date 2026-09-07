// Chapter 15 — Geometry: the reference library.
//
// Compile:  c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo
//
// TRAINING RULE: this file is your snippet library. Read it once, then close it and
// re-type every function from memory. Repeat until you can write P/cross/orient,
// segment intersection, point-in-polygon, convex hull, closest pair and the rectangle
// union sweep without looking. In a contest you will have ~5 minutes per primitive.
//
// Design rules used throughout:
//   * Integer geometry first. Points are `long long`; every predicate (orientation,
//     on-segment, intersection test, point-in-polygon, hull) is EXACT — no eps.
//   * `long double` only where a real number is unavoidable (intersection point, distance,
//     circles, half-plane intersection). Those functions are grouped at the end.
//   * Overflow bound: |coord| <= 1e9  =>  |dx|,|dy| <= 2e9  =>  |dx*dy| <= 4e18
//     and a cross product (difference of two such products) is <= 8e18 < 9.22e18 = LLONG_MAX.
//     Anything larger (e.g. cross*cross, coords up to 2e9) needs __int128.
//
// main() asserts every primitive on hand-checked inputs and cross-checks the O(n log n)
// algorithms against brute force on random data.

#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <deque>
#include <numeric>
#include <random>
#include <set>
#include <tuple>
#include <vector>
using namespace std;
typedef long long ll;
typedef long double ld;

// ============================================================================
// 1. Points, vectors, cross/dot, orientation
// ============================================================================
struct P {
    ll x, y;
    P(ll x = 0, ll y = 0) : x(x), y(y) {}
    P operator+(P o) const { return P(x + o.x, y + o.y); }
    P operator-(P o) const { return P(x - o.x, y - o.y); }
    P operator*(ll k) const { return P(x * k, y * k); }
    bool operator<(P o) const { return x != o.x ? x < o.x : y < o.y; }
    bool operator==(P o) const { return x == o.x && y == o.y; }
    bool operator!=(P o) const { return !(*this == o); }
};
ll cross(P a, P b) { return a.x * b.y - a.y * b.x; }        // signed area of parallelogram (a,b)
ll dot(P a, P b) { return a.x * b.x + a.y * b.y; }
ll cross(P o, P a, P b) { return cross(a - o, b - o); }       // twice signed area of triangle oab
ll norm2(P a) { return dot(a, a); }                          // squared length — stays integer
int sgn(ll v) { return (v > 0) - (v < 0); }
// +1: c is to the LEFT of directed line a->b (a,b,c counter-clockwise)
// -1: RIGHT (clockwise);  0: collinear.  Exact for |coord| <= 1e9.
int orient(P a, P b, P c) { return sgn(cross(a, b, c)); }

// ============================================================================
// 2. Segments and lines (exact)
// ============================================================================
// p lies on closed segment [a,b]  (works when a==b: then p must equal a)
bool onSeg(P p, P a, P b) { return orient(a, b, p) == 0 && dot(p - a, p - b) <= 0; }

// Do closed segments [a,b] and [c,d] share at least one point?
// Handles: proper crossing, touching at an endpoint, collinear overlap, degenerate
// (point) segments.  The four onSeg checks cover every improper case; after them the only
// remaining way to intersect is a proper crossing, detected by strict sign changes.
bool segInter(P a, P b, P c, P d) {
    if (onSeg(a, c, d) || onSeg(b, c, d) || onSeg(c, a, b) || onSeg(d, a, b)) return true;
    return orient(a, b, c) * orient(a, b, d) < 0 && orient(c, d, a) * orient(c, d, b) < 0;
}

// Lattice points on the closed segment [a,b]  (gcd trick)
ll latticeOnSeg(P a, P b) { return std::gcd(llabs(a.x - b.x), llabs(a.y - b.y)) + 1; }   // std::gcd is C++17 (<numeric>)

// Intersection of LINES ab and cd as an exact rational:  point = a + (b-a) * num/den.
// Returns false if parallel (den == 0).  Caller may convert to long double or keep the
// fraction for exact comparisons (compare num1*den2 vs num2*den1 with __int128).
bool lineInterFrac(P a, P b, P c, P d, ll& num, ll& den) {
    den = cross(b - a, d - c);
    if (den == 0) return false;
    num = cross(c - a, d - c);
    if (den < 0) { den = -den; num = -num; }
    return true;
}

// ============================================================================
// 3. Polygons (exact)
// ============================================================================
// Twice the signed area (shoelace). >0 for counter-clockwise order.
ll area2(const vector<P>& p) {
    ll s = 0; int n = (int)p.size();
    for (int i = 0; i < n; i++) s += cross(p[i], p[(i + 1) % n]);
    return s;
}

// Point in SIMPLE polygon (any orientation), ray casting. 0 = outside, 1 = boundary, 2 = inside.
// Invariant: a horizontal ray to the right from q crosses an edge (a,b) iff exactly one
// endpoint is strictly above q.y (half-open rule => a vertex is counted once), and the
// crossing is to the right of q iff the sign of cross(b-a, q-a) matches the edge direction.
int inPoly(const vector<P>& p, P q) {
    int n = (int)p.size(); bool in = false;
    for (int i = 0; i < n; i++) {
        P a = p[i], b = p[(i + 1) % n];
        if (onSeg(q, a, b)) return 1;
        if ((a.y > q.y) != (b.y > q.y) && ((cross(b - a, q - a) > 0) == (b.y > a.y))) in = !in;
    }
    return in ? 2 : 0;
}

// Winding number (Sunday's algorithm). q must not be on the boundary.
// Inside a simple polygon <=> winding != 0.  Also useful for self-intersecting polygons.
int winding(const vector<P>& p, P q) {
    int n = (int)p.size(), w = 0;
    for (int i = 0; i < n; i++) {
        P a = p[i], b = p[(i + 1) % n];
        if (a.y <= q.y) { if (b.y > q.y && cross(b - a, q - a) > 0) w++; }
        else            { if (b.y <= q.y && cross(b - a, q - a) < 0) w--; }
    }
    return w;
}

// Point in STRICTLY CONVEX polygon given in ccw order, O(log n). Returns 0/1/2 as above.
// Fan from p[0]: binary search the wedge (p0, p[i], p[i+1]) containing q, then one test.
int inConvex(const vector<P>& p, P q) {
    int n = (int)p.size();
    if (orient(p[0], p[1], q) < 0 || orient(p[0], p[n - 1], q) > 0) return 0;
    int lo = 1, hi = n - 1;                         // largest lo with orient(p0,p[lo],q) >= 0
    while (hi - lo > 1) { int m = (lo + hi) / 2; (orient(p[0], p[m], q) >= 0 ? lo : hi) = m; }
    int s = orient(p[lo], p[lo + 1], q);
    if (s < 0) return 0;
    if (s == 0) return 1;
    if (lo == 1 && orient(p[0], p[1], q) == 0) return 1;
    if (lo == n - 2 && orient(p[0], p[n - 1], q) == 0) return 1;
    return 2;
}

// Pick's theorem:  A = I + B/2 - 1   =>  I = (2A - B + 2) / 2.  Returns {interior, boundary}.
pair<ll, ll> pickCount(const vector<P>& p) {
    int n = (int)p.size(); ll B = 0;
    for (int i = 0; i < n; i++) B += latticeOnSeg(p[i], p[(i + 1) % n]) - 1;  // each vertex once
    ll A2 = llabs(area2(p));
    return {(A2 - B + 2) / 2, B};
}

// ============================================================================
// 4. Convex hull — Andrew's monotone chain, O(n log n)
// ============================================================================
// Returns hull in ccw order starting from the lowest-x (then lowest-y) point.
// keepCollinear=false: strictly convex (collinear boundary points dropped) — the default you
// want for calipers / inConvex.  keepCollinear=true: every boundary lattice point kept
// (needed e.g. when counting hull points).  n<=2 handled; duplicates removed.
vector<P> hull(vector<P> p, bool keepCollinear = false) {
    sort(p.begin(), p.end());
    p.erase(unique(p.begin(), p.end()), p.end());
    int n = (int)p.size();
    if (n <= 2) return p;
    bool allCol = true;
    for (int i = 2; i < n && allCol; i++) allCol = orient(p[0], p[1], p[i]) == 0;
    if (allCol) { if (keepCollinear) return p; return {p[0], p[n - 1]}; }
    vector<P> h(2 * n); int k = 0;
    auto bad = [&](P a, P b, P c) { ll cr = cross(a, b, c); return keepCollinear ? cr < 0 : cr <= 0; };
    for (int i = 0; i < n; i++) {                              // lower hull, left to right
        while (k >= 2 && bad(h[k - 2], h[k - 1], p[i])) k--;
        h[k++] = p[i];
    }
    for (int i = n - 2, t = k + 1; i >= 0; i--) {              // upper hull, right to left
        while (k >= t && bad(h[k - 2], h[k - 1], p[i])) k--;
        h[k++] = p[i];
    }
    h.resize(k - 1);                                           // last point == first point
    return h;
}

// ============================================================================
// 5. Rotating calipers on a strictly convex ccw polygon
// ============================================================================
// Squared diameter (farthest pair). Two pointers: for edge i, advance j while the next
// vertex is farther from edge i's line; candidates are (i,j) and (i+1,j).  O(n).
ll diameter2(const vector<P>& h) {
    int n = (int)h.size();
    if (n <= 1) return 0;
    if (n == 2) return norm2(h[1] - h[0]);
    ll best = 0; int j = 1;
    for (int i = 0; i < n; i++) {
        P e = h[(i + 1) % n] - h[i];
        while (cross(e, h[(j + 1) % n] - h[j]) > 0) j = (j + 1) % n;
        best = max({best, norm2(h[i] - h[j]), norm2(h[(i + 1) % n] - h[j])});
    }
    return best;
}

// Minimum width (smallest distance between two parallel supporting lines) and minimum-area
// enclosing rectangle. Theorem: an optimal rectangle has a side flush with a hull edge, so
// try every edge i with three pointers: j = farthest from the edge (max cross),
// r = farthest along the edge (max dot), l = farthest against it (min dot).
// All three rotate monotonically with i.  Returns {minWidth, minRectArea}.  n >= 3.
pair<ld, ld> minWidthAndRect(const vector<P>& h) {
    int n = (int)h.size();
    ld bestW = 1e30L, bestA = 1e30L;
    int j = 1, r = 1, l = 0;
    for (int i = 0; i < n; i++) {
        P o = h[i], e = h[(i + 1) % n] - o;
        while (cross(e, h[(j + 1) % n] - o) >= cross(e, h[j] - o) && (j + 1) % n != i) j = (j + 1) % n;
        while (dot(e, h[(r + 1) % n] - o) >= dot(e, h[r] - o) && (r + 1) % n != i) r = (r + 1) % n;
        if (i == 0) l = r;
        while (dot(e, h[(l + 1) % n] - o) <= dot(e, h[l] - o) && (l + 1) % n != (i + 1) % n) l = (l + 1) % n;
        ld len = sqrtl((ld)norm2(e));
        ld height = (ld)cross(e, h[j] - o) / len;
        ld width = (ld)(dot(e, h[r] - o) - dot(e, h[l] - o)) / len;
        bestW = min(bestW, height);
        bestA = min(bestA, height * width);
    }
    return {bestW, bestA};
}

// ============================================================================
// 6. Closest pair of points
// ============================================================================
// (a) Divide & conquer, O(n log n): split by x, recurse, merge by y, check the strip.
//     Strip argument: in a d x d box on one side there are at most 4 points at pairwise
//     distance >= d, so each strip point is compared with O(1) successors in y order.
ll closestDC(vector<P>& a, int lo, int hi) {   // a sorted by x on entry; sorted by y on exit
    if (hi - lo <= 3) {
        ll best = LLONG_MAX;
        for (int i = lo; i < hi; i++) for (int j = i + 1; j < hi; j++) best = min(best, norm2(a[i] - a[j]));
        sort(a.begin() + lo, a.begin() + hi, [](P u, P v) { return u.y < v.y; });
        return best;
    }
    int mid = (lo + hi) / 2; ll mx = a[mid].x;
    ll best = min(closestDC(a, lo, mid), closestDC(a, mid, hi));
    inplace_merge(a.begin() + lo, a.begin() + mid, a.begin() + hi, [](P u, P v) { return u.y < v.y; });
    vector<P> strip;
    for (int i = lo; i < hi; i++) if ((a[i].x - mx) * (a[i].x - mx) < best) strip.push_back(a[i]);
    for (int i = 0; i < (int)strip.size(); i++)
        for (int j = i + 1; j < (int)strip.size() && (strip[j].y - strip[i].y) * (strip[j].y - strip[i].y) < best; j++)
            best = min(best, norm2(strip[i] - strip[j]));
    return best;
}
ll closestPairDC(vector<P> a) { sort(a.begin(), a.end()); return closestDC(a, 0, (int)a.size()); }

// (b) Sweep line with std::set, O(n log n): sweep by x, keep the points with x within d of the
//     sweep in a set ordered by y; only points with |dy| < d can improve d.
ll closestPairSweep(vector<P> a) {
    sort(a.begin(), a.end());
    set<pair<ll, ll>> s;                          // (y, x)
    ll best = LLONG_MAX; int left = 0;
    for (int i = 0; i < (int)a.size(); i++) {
        ll d = best == LLONG_MAX ? LLONG_MAX / 4 : (ll)sqrtl((ld)best) + 1;
        while (a[i].x - a[left].x >= d) { s.erase({a[left].y, a[left].x}); left++; }
        auto it = s.lower_bound({a[i].y - d, LLONG_MIN});
        for (; it != s.end() && it->first <= a[i].y + d; ++it)
            best = min(best, norm2(a[i] - P(it->second, it->first)));
        s.insert({a[i].y, a[i].x});
    }
    return best;
}

// ============================================================================
// 7. Sweep line: union area of axis-aligned rectangles (segment tree), O(n log n)
// ============================================================================
// Segment tree over compressed x-intervals: cnt[node] = how many rectangles fully cover the
// node's range (not pushed down), len[node] = covered length inside the node's range.
// Sweep in y: at each y-event add/remove the rectangle's x-interval; area += len(root) * dy.
struct CoverTree {
    int n; vector<int> cnt; vector<ll> len; vector<ll> xs;   // xs: sorted unique x coords
    CoverTree(const vector<ll>& xs_) : xs(xs_) { n = (int)xs.size() - 1; cnt.assign(4 * n, 0); len.assign(4 * n, 0); }
    void upd(int node, int l, int r, int ql, int qr, int v) {   // elementary intervals [l,r)
        if (qr <= l || r <= ql) return;
        if (ql <= l && r <= qr) cnt[node] += v;
        else { int m = (l + r) / 2; upd(2 * node, l, m, ql, qr, v); upd(2 * node + 1, m, r, ql, qr, v); }
        if (cnt[node] > 0) len[node] = xs[r] - xs[l];
        else len[node] = (r - l == 1) ? 0 : len[2 * node] + len[2 * node + 1];
    }
    void add(int ql, int qr, int v) { if (ql < qr) upd(1, 0, n, ql, qr, v); }
    ll covered() const { return len[1]; }
};
struct Rect { ll x1, y1, x2, y2; };   // x1<x2, y1<y2
ll unionArea(const vector<Rect>& rs) {
    if (rs.empty()) return 0;
    vector<ll> xs;
    for (auto& r : rs) { xs.push_back(r.x1); xs.push_back(r.x2); }
    sort(xs.begin(), xs.end()); xs.erase(unique(xs.begin(), xs.end()), xs.end());
    auto idx = [&](ll x) { return int(lower_bound(xs.begin(), xs.end(), x) - xs.begin()); };
    vector<tuple<ll, int, int, int>> ev;              // (y, +1/-1, xl, xr)
    for (auto& r : rs) { ev.emplace_back(r.y1, +1, idx(r.x1), idx(r.x2)); ev.emplace_back(r.y2, -1, idx(r.x1), idx(r.x2)); }
    sort(ev.begin(), ev.end());
    CoverTree t(xs); ll area = 0; ll prevY = get<0>(ev[0]);
    for (auto& [y, v, l, r] : ev) { area += t.covered() * (y - prevY); prevY = y; t.add(l, r, v); }
    return area;
}

// ============================================================================
// 8. Minkowski sum of two convex polygons (exact), O(n + m)
// ============================================================================
// Edges of A+B are the edges of A and B merged by angle. Start both at the lowest-then-
// leftmost vertex so that edge angles run 0..2pi in the same order for both.
vector<P> minkowski(vector<P> a, vector<P> b) {   // both strictly convex, ccw, size >= 1
    auto reorder = [](vector<P>& v) {
        int k = 0;
        for (int i = 1; i < (int)v.size(); i++) if (v[i].y < v[k].y || (v[i].y == v[k].y && v[i].x < v[k].x)) k = i;
        rotate(v.begin(), v.begin() + k, v.end());
    };
    reorder(a); reorder(b);
    int n = (int)a.size(), m = (int)b.size();
    a.push_back(a[0]); a.push_back(a[1 % n]); b.push_back(b[0]); b.push_back(b[1 % m]);
    vector<P> r; int i = 0, j = 0;
    while (i < n || j < m) {
        r.push_back(a[i] + b[j]);
        ll c = cross(a[i + 1] - a[i], b[j + 1] - b[j]);
        if (c >= 0 && i < n) i++;
        if (c <= 0 && j < m) j++;
    }
    return r;
}

// ============================================================================
// 9. Angular sort without atan2 (exact)
// ============================================================================
// half(): 0 for angles in [0, pi), 1 for [pi, 2pi).  Then compare within a half by cross.
int half(P p) { return (p.y > 0 || (p.y == 0 && p.x > 0)) ? 0 : 1; }
bool angLess(P a, P b) { int ha = half(a), hb = half(b); return ha != hb ? ha < hb : cross(a, b) > 0; }

// ============================================================================
// 10. Real-valued geometry (long double). Only where unavoidable.
// ============================================================================
const ld EPS = 1e-9L;
int sgnd(ld v) { return (v > EPS) - (v < -EPS); }
struct PD {
    ld x, y;
    PD(ld x = 0, ld y = 0) : x(x), y(y) {}
    PD(P p) : x((ld)p.x), y((ld)p.y) {}
    PD operator+(PD o) const { return PD(x + o.x, y + o.y); }
    PD operator-(PD o) const { return PD(x - o.x, y - o.y); }
    PD operator*(ld k) const { return PD(x * k, y * k); }
    PD operator/(ld k) const { return PD(x / k, y / k); }
};
ld crossd(PD a, PD b) { return a.x * b.y - a.y * b.x; }
ld dotd(PD a, PD b) { return a.x * b.x + a.y * b.y; }
ld absd(PD a) { return sqrtl(dotd(a, a)); }
PD perp(PD a) { return PD(-a.y, a.x); }                 // rotate +90 degrees
PD rot(PD a, ld ang) { return PD(a.x * cosl(ang) - a.y * sinl(ang), a.x * sinl(ang) + a.y * cosl(ang)); }

// Intersection point of lines ab and cd (assumes not parallel).
PD lineInter(PD a, PD b, PD c, PD d) {
    ld t = crossd(c - a, d - c) / crossd(b - a, d - c);
    return a + (b - a) * t;
}
ld distPointLine(PD p, PD a, PD b) { return fabsl(crossd(b - a, p - a)) / absd(b - a); }
ld distPointSeg(PD p, PD a, PD b) {
    if (dotd(p - a, b - a) <= 0) return absd(p - a);      // before a
    if (dotd(p - b, a - b) <= 0) return absd(p - b);      // past b
    return distPointLine(p, a, b);
}
ld area2d(const vector<PD>& p) {
    ld s = 0; int n = (int)p.size();
    for (int i = 0; i < n; i++) s += crossd(p[i], p[(i + 1) % n]);
    return s;
}

// Cut polygon by directed line a->b, keep the LEFT side (cross >= 0). O(n). Convex in => convex out.
vector<PD> cutPolygon(const vector<PD>& poly, PD a, PD b) {
    vector<PD> res; int n = (int)poly.size();
    for (int i = 0; i < n; i++) {
        PD p = poly[i], q = poly[(i + 1) % n];
        int sp = sgnd(crossd(b - a, p - a)), sq = sgnd(crossd(b - a, q - a));
        if (sp >= 0) res.push_back(p);
        if (sp * sq < 0) res.push_back(lineInter(p, q, a, b));
    }
    return res;
}

// Half-plane intersection. Each half-plane is "left of directed line a->b" with INTEGER a,b so
// the angular sort is exact; intersection points are long double.  Sort by angle, sweep with a
// deque, pop back/front lines whose defining vertex falls outside the new half-plane, then trim
// the wrap-around.  A bounding box is added so the result is always bounded.  O(n log n).
struct HP { P a, b; P d() const { return b - a; } bool out(PD q) const { return crossd(PD(d()), q - PD(a)) < -EPS; } };
vector<PD> halfPlaneInter(vector<HP> hs, ll BOX = 1e9) {   // BOX <= 1e9 keeps the exact sort overflow-free
    hs.push_back({P(-BOX, -BOX), P(BOX, -BOX)}); hs.push_back({P(BOX, -BOX), P(BOX, BOX)});
    hs.push_back({P(BOX, BOX), P(-BOX, BOX)});   hs.push_back({P(-BOX, BOX), P(-BOX, -BOX)});
    sort(hs.begin(), hs.end(), [](const HP& u, const HP& v) {
        if (half(u.d()) != half(v.d())) return half(u.d()) < half(v.d());
        ll c = cross(u.d(), v.d());
        if (c != 0) return c > 0;
        return cross(u.d(), v.a - u.a) < 0;      // same direction: v.a outside u  =>  u is more restrictive, u first
    });
    // drop duplicates of the same direction (keep the first == most restrictive)
    vector<HP> h;
    for (auto& l : hs) if (h.empty() || half(h.back().d()) != half(l.d()) || cross(h.back().d(), l.d()) != 0) h.push_back(l);
    auto inter = [](const HP& u, const HP& v) { return lineInter(PD(u.a), PD(u.b), PD(v.a), PD(v.b)); };
    deque<HP> dq;
    for (auto& l : h) {
        while (dq.size() >= 2 && l.out(inter(dq[dq.size() - 2], dq.back()))) dq.pop_back();
        while (dq.size() >= 2 && l.out(inter(dq[0], dq[1]))) dq.pop_front();
        if (!dq.empty() && cross(dq.back().d(), l.d()) == 0) {     // antiparallel neighbour
            if (dot(dq.back().d(), l.d()) < 0 && l.out(PD(dq.back().a))) return {};
        }
        dq.push_back(l);
    }
    while (dq.size() >= 3 && dq[0].out(inter(dq[dq.size() - 2], dq.back()))) dq.pop_back();
    while (dq.size() >= 3 && dq.back().out(inter(dq[0], dq[1]))) dq.pop_front();
    if (dq.size() < 3) return {};
    vector<PD> res;
    for (int i = 0; i < (int)dq.size(); i++) res.push_back(inter(dq[i], dq[(i + 1) % dq.size()]));
    return res;
}

// Circles.
struct Circle { PD c; ld r; };
// circle-line: 0, 1 (tangent) or 2 points
vector<PD> circleLine(Circle C, PD a, PD b) {
    PD d = b - a; ld t = dotd(C.c - a, d) / dotd(d, d); PD f = a + d * t;   // foot of perpendicular
    ld h2 = C.r * C.r - dotd(f - C.c, f - C.c);
    if (h2 < -EPS) return {};
    if (h2 < EPS) return {f};
    PD off = d * (sqrtl(h2) / absd(d));
    return {f - off, f + off};
}
// circle-circle: returns 0, 1, 2 points; identical circles => 0 (infinitely many, caller decides)
vector<PD> circleCircle(Circle A, Circle B) {
    ld d = absd(B.c - A.c);
    if (d < EPS) return {};
    if (d > A.r + B.r + EPS || d < fabsl(A.r - B.r) - EPS) return {};
    ld x = (d * d + A.r * A.r - B.r * B.r) / (2 * d);        // distance from A.c along the center line
    ld h2 = A.r * A.r - x * x; if (h2 < 0) h2 = 0;
    PD u = (B.c - A.c) / d, m = A.c + u * x, v = perp(u) * sqrtl(h2);
    if (h2 < EPS) return {m};
    return {m - v, m + v};
}
// tangent points from external point p to circle C (p strictly outside)
vector<PD> tangentPoints(Circle C, PD p) {
    ld d2 = dotd(p - C.c, p - C.c);
    return circleCircle(C, {p, sqrtl(d2 - C.r * C.r)});
}
// circumscribed circle of three non-collinear points
Circle circumcircle(PD a, PD b, PD c) {
    PD ab = b - a, ac = c - a;
    ld d = 2 * crossd(ab, ac);
    ld ux = (ac.y * dotd(ab, ab) - ab.y * dotd(ac, ac)) / d;
    ld uy = (ab.x * dotd(ac, ac) - ac.x * dotd(ab, ab)) / d;
    PD ctr = a + PD(ux, uy);
    return {ctr, absd(ctr - a)};
}

// 3D basics: cross product gives a normal; scalar triple product = 6 * signed tetra volume.
struct P3 { ll x, y, z; P3 operator-(P3 o) const { return {x - o.x, y - o.y, z - o.z}; } };
P3 cross3(P3 a, P3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
ll dot3(P3 a, P3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
ll volume6(P3 a, P3 b, P3 c, P3 d) { return dot3(cross3(b - a, c - a), d - a); }

// ============================================================================
// Tests
// ============================================================================
static mt19937_64 rng(12345);
ll rnd(ll lo, ll hi) { return uniform_int_distribution<ll>(lo, hi)(rng); }

vector<P> bruteHull(const vector<P>& pts) {   // O(n^3): edge (i,j) is a hull edge iff all others are left of it
    vector<P> p = pts; sort(p.begin(), p.end()); p.erase(unique(p.begin(), p.end()), p.end());
    int n = (int)p.size(); set<P> vs;
    if (n <= 2) return p;
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (i != j) {
        bool ok = true;
        for (int k = 0; k < n && ok; k++) if (k != i && k != j) {
            ll c = cross(p[i], p[j], p[k]);
            if (c < 0 || (c == 0 && !onSeg(p[k], p[i], p[j]))) ok = false;
        }
        if (ok) { vs.insert(p[i]); vs.insert(p[j]); }
    }
    return vector<P>(vs.begin(), vs.end());
}

int main() {
    // ---- orientation / cross / dot ----
    assert(orient(P(0, 0), P(4, 0), P(1, 1)) == 1);        // left  (ccw)
    assert(orient(P(0, 0), P(4, 0), P(1, -1)) == -1);      // right (cw)
    assert(orient(P(0, 0), P(4, 0), P(9, 0)) == 0);        // collinear
    assert(cross(P(1e9, 1e9), P(-1e9, -1e9)) == 0);        // no overflow at the coordinate bound
    assert(cross(P(-1e9, -1e9), P(1e9, 1e9), P(1e9, -1e9)) == -4000000000000000000LL);  // |8e18| bound territory
    assert(dot(P(3, 4), P(3, 4)) == 25 && norm2(P(3, 4)) == 25);

    // ---- onSeg / segInter ----
    assert(onSeg(P(2, 2), P(0, 0), P(4, 4)) && !onSeg(P(5, 5), P(0, 0), P(4, 4)) && !onSeg(P(2, 3), P(0, 0), P(4, 4)));
    assert(onSeg(P(1, 1), P(1, 1), P(1, 1)) && !onSeg(P(1, 2), P(1, 1), P(1, 1)));
    assert(segInter(P(0, 0), P(4, 4), P(0, 4), P(4, 0)));          // proper crossing
    assert(segInter(P(0, 0), P(4, 4), P(2, 2), P(9, -3)));         // touching at interior point
    assert(segInter(P(0, 0), P(4, 4), P(4, 4), P(9, 0)));          // shared endpoint
    assert(segInter(P(0, 0), P(4, 4), P(2, 2), P(8, 8)));          // collinear overlap
    assert(!segInter(P(0, 0), P(4, 4), P(5, 5), P(8, 8)));         // collinear disjoint
    assert(!segInter(P(0, 0), P(4, 4), P(0, 1), P(3, 4)));         // parallel disjoint
    assert(!segInter(P(0, 0), P(4, 0), P(1, 1), P(3, 1)));         // parallel, not collinear
    assert(segInter(P(1, 1), P(1, 1), P(0, 0), P(2, 2)));          // point on segment
    assert(!segInter(P(1, 2), P(1, 2), P(0, 0), P(2, 2)));         // point off segment
    assert(!segInter(P(0, 0), P(4, 4), P(1, 3), P(2, 9)));         // lines cross but segments don't

    // ---- line intersection (fraction + double) ----
    { ll num, den; assert(lineInterFrac(P(0, 0), P(4, 4), P(0, 4), P(4, 0), num, den) && 4 * num == 2 * den);
      assert(!lineInterFrac(P(0, 0), P(1, 1), P(0, 1), P(1, 2), num, den));
      PD q = lineInter(PD(0, 0), PD(4, 4), PD(0, 4), PD(4, 0)); assert(fabsl(q.x - 2) < EPS && fabsl(q.y - 2) < EPS); }
    assert(latticeOnSeg(P(0, 0), P(6, 4)) == 3 && latticeOnSeg(P(1, 1), P(1, 1)) == 1 && latticeOnSeg(P(0, 0), P(5, 0)) == 6);

    // ---- distances ----
    assert(fabsl(distPointLine(PD(0, 3), PD(-1, 0), PD(1, 0)) - 3) < EPS);
    assert(fabsl(distPointSeg(PD(5, 3), PD(-1, 0), PD(1, 0)) - 5) < EPS);   // past endpoint b: dist to (1,0) = 5
    assert(fabsl(distPointSeg(PD(0, 3), PD(-1, 0), PD(1, 0)) - 3) < EPS);

    // ---- polygon area, point in polygon, Pick ----
    vector<P> sq = {P(0, 0), P(4, 0), P(4, 4), P(0, 4)};
    assert(area2(sq) == 32);
    { vector<P> cw(sq.rbegin(), sq.rend()); assert(area2(cw) == -32); }
    vector<P> tri = {P(0, 0), P(6, 0), P(0, 4)};
    assert(area2(tri) == 24);
    assert(inPoly(sq, P(2, 2)) == 2 && inPoly(sq, P(4, 2)) == 1 && inPoly(sq, P(4, 4)) == 1 && inPoly(sq, P(5, 2)) == 0);
    assert(inPoly(sq, P(-1, 4)) == 0 && inPoly(sq, P(-1, 0)) == 0);   // ray through vertices
    vector<P> concave = {P(0, 0), P(6, 0), P(6, 6), P(3, 2), P(0, 6)};   // notch from the top
    assert(inPoly(concave, P(3, 4)) == 0 && inPoly(concave, P(1, 3)) == 2 && inPoly(concave, P(3, 2)) == 1 && inPoly(concave, P(5, 3)) == 2);
    assert(winding(concave, P(3, 4)) == 0 && winding(concave, P(1, 3)) == 1);
    { vector<P> cw(concave.rbegin(), concave.rend()); assert(winding(cw, P(1, 3)) == -1 && inPoly(cw, P(1, 3)) == 2); }
    assert(inConvex(sq, P(2, 2)) == 2 && inConvex(sq, P(4, 2)) == 1 && inConvex(sq, P(0, 2)) == 1 && inConvex(sq, P(2, 0)) == 1);
    assert(inConvex(sq, P(5, 2)) == 0 && inConvex(sq, P(-1, 0)) == 0 && inConvex(sq, P(5, 0)) == 0 && inConvex(sq, P(0, 0)) == 1);
    { auto [I, B] = pickCount(sq); assert(I == 9 && B == 16); }
    { auto [I, B] = pickCount(tri); assert(B == 12 && I == 12 - 6 + 1); }   // A=12: I = 12 - 6 + 1 = 7
    // Pick vs brute lattice count on random lattice polygons (convex, via hull)
    for (int it = 0; it < 200; it++) {
        vector<P> pts; for (int i = 0; i < 8; i++) pts.push_back(P(rnd(-6, 6), rnd(-6, 6)));
        vector<P> h = hull(pts);
        if (h.size() < 3) continue;
        auto [I, B] = pickCount(h);
        ll bi = 0, bb = 0;
        for (ll x = -7; x <= 7; x++) for (ll y = -7; y <= 7; y++) { int r = inPoly(h, P(x, y)); bi += r == 2; bb += r == 1; }
        assert(I == bi && B == bb);
        for (ll x = -7; x <= 7; x++) for (ll y = -7; y <= 7; y++) {
            assert(inConvex(h, P(x, y)) == inPoly(h, P(x, y)));
            if (inPoly(h, P(x, y)) != 1) assert((winding(h, P(x, y)) != 0) == (inPoly(h, P(x, y)) == 2));
        }
    }

    // ---- convex hull vs brute O(n^3) ----
    { vector<P> h = hull({P(0, 0), P(2, 0), P(1, 0), P(1, 1), P(2, 2), P(0, 2), P(1, 2)});
      assert(h.size() == 4 && area2(h) == 8);
      vector<P> hk = hull({P(0, 0), P(2, 0), P(1, 0), P(1, 1), P(2, 2), P(0, 2), P(1, 2)}, true);
      assert(hk.size() == 6);
      assert(hull({P(0, 0), P(3, 3), P(1, 1), P(2, 2)}).size() == 2);
      assert(hull({P(0, 0), P(3, 3), P(1, 1), P(2, 2)}, true).size() == 4);
      assert(hull({P(5, 5), P(5, 5)}).size() == 1); }
    for (int it = 0; it < 300; it++) {
        int n = (int)rnd(1, 12); vector<P> pts;
        for (int i = 0; i < n; i++) pts.push_back(P(rnd(-5, 5), rnd(-5, 5)));
        vector<P> h = hull(pts), b = bruteHull(pts);
        vector<P> hs = h; sort(hs.begin(), hs.end());
        assert(hs == b);
        for (int i = 0; i < (int)h.size() && h.size() >= 3; i++) assert(orient(h[i], h[(i + 1) % h.size()], h[(i + 2) % h.size()]) > 0);
        for (P q : pts) assert(h.size() < 3 || inPoly(h, q) != 0);
    }

    // ---- rotating calipers vs brute ----
    for (int it = 0; it < 200; it++) {
        int n = (int)rnd(3, 15); vector<P> pts;
        for (int i = 0; i < n; i++) pts.push_back(P(rnd(-20, 20), rnd(-20, 20)));
        vector<P> h = hull(pts);
        if (h.size() < 3) continue;
        ll bd = 0;
        for (P a : pts) for (P b : pts) bd = max(bd, norm2(a - b));
        assert(diameter2(h) == bd);
        ld bw = 1e30L, ba = 1e30L; int m = (int)h.size();
        for (int i = 0; i < m; i++) {
            P o = h[i], e = h[(i + 1) % m] - o; ll hi = 0, dmax = LLONG_MIN, dmin = LLONG_MAX;
            for (P q : h) { hi = max(hi, cross(e, q - o)); dmax = max(dmax, dot(e, q - o)); dmin = min(dmin, dot(e, q - o)); }
            ld len = sqrtl((ld)norm2(e));
            bw = min(bw, (ld)hi / len); ba = min(ba, (ld)hi / len * ((ld)(dmax - dmin) / len));
        }
        auto [w, a] = minWidthAndRect(h);
        assert(fabsl(w - bw) < 1e-6L && fabsl(a - ba) < 1e-6L);
    }
    { auto [w, a] = minWidthAndRect(sq); assert(fabsl(w - 4) < EPS && fabsl(a - 16) < EPS); assert(diameter2(sq) == 32); }

    // ---- closest pair vs O(n^2) ----
    for (int it = 0; it < 200; it++) {
        int n = (int)rnd(2, 40); vector<P> pts; ll R = it % 2 ? 10 : 1000000000LL;
        for (int i = 0; i < n; i++) pts.push_back(P(rnd(-R, R), rnd(-R, R)));
        ll b = LLONG_MAX;
        for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) b = min(b, norm2(pts[i] - pts[j]));
        assert(closestPairDC(pts) == b && closestPairSweep(pts) == b);
    }

    // ---- rectangle union vs brute grid ----
    assert(unionArea({{0, 0, 2, 2}, {1, 1, 3, 3}}) == 7);
    assert(unionArea({{0, 0, 4, 4}, {1, 1, 2, 2}}) == 16);
    for (int it = 0; it < 200; it++) {
        int n = (int)rnd(1, 6); vector<Rect> rs;
        for (int i = 0; i < n; i++) { ll x1 = rnd(0, 9), x2 = rnd(0, 9), y1 = rnd(0, 9), y2 = rnd(0, 9);
            if (x1 == x2) x2++; if (y1 == y2) y2++; rs.push_back({min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2)}); }
        ll b = 0;
        for (int x = 0; x < 11; x++) for (int y = 0; y < 11; y++) {
            bool cov = false; for (auto& r : rs) if (r.x1 <= x && x < r.x2 && r.y1 <= y && y < r.y2) cov = true; b += cov; }
        assert(unionArea(rs) == b);
    }

    // ---- Minkowski sum vs hull of pairwise sums ----
    for (int it = 0; it < 200; it++) {
        vector<P> pa, pb;
        for (int i = 0; i < (int)rnd(1, 7); i++) pa.push_back(P(rnd(-9, 9), rnd(-9, 9)));
        for (int i = 0; i < (int)rnd(1, 7); i++) pb.push_back(P(rnd(-9, 9), rnd(-9, 9)));
        vector<P> a = hull(pa), b = hull(pb);
        if (a.size() < 3 || b.size() < 3) continue;
        vector<P> s = minkowski(a, b), all;
        for (P u : a) for (P v : b) all.push_back(u + v);
        vector<P> hs = hull(all), ss = hull(s);
        assert(hs == ss && s.size() == hs.size());
    }

    // ---- angular sort vs atan2 ----
    for (int it = 0; it < 100; it++) {
        vector<P> v; for (int i = 0; i < 20; i++) { P p(rnd(-9, 9), rnd(-9, 9)); if (p != P(0, 0)) v.push_back(p); }
        sort(v.begin(), v.end(), angLess);
        for (int i = 0; i + 1 < (int)v.size(); i++) {
            auto ang = [](P p) { ld a = atan2l((ld)p.y, (ld)p.x); return a < 0 ? a + 2 * acosl(-1) : a; };
            assert(ang(v[i]) <= ang(v[i + 1]) + 1e-12L);
        }
    }

    // ---- polygon cut, half-plane intersection ----
    { vector<PD> sqd = {PD(0, 0), PD(4, 0), PD(4, 4), PD(0, 4)};
      vector<PD> c = cutPolygon(sqd, PD(2, 0), PD(2, 4));         // keep left of x=2 (upward line) => x <= 2
      assert(fabsl(area2d(c) - 16) < EPS);
      c = cutPolygon(sqd, PD(0, 0), PD(4, 4));                   // keep y >= x
      assert(c.size() == 3 && fabsl(area2d(c) - 16) < EPS); }
    {   // two overlapping squares [0,4]^2 and [2,6]^2 -> [2,4]^2
        auto box = [](ll x1, ll y1, ll x2, ll y2) { return vector<HP>{{P(x1, y1), P(x2, y1)}, {P(x2, y1), P(x2, y2)}, {P(x2, y2), P(x1, y2)}, {P(x1, y2), P(x1, y1)}}; };
        vector<HP> hs = box(0, 0, 4, 4); for (auto h : box(2, 2, 6, 6)) hs.push_back(h);
        vector<PD> r = halfPlaneInter(hs);
        assert(r.size() == 4 && fabsl(area2d(r) - 8) < 1e-6L);
        // disjoint boxes -> empty
        hs = box(0, 0, 1, 1); for (auto h : box(2, 2, 3, 3)) hs.push_back(h);
        assert(halfPlaneInter(hs).empty());
        // triangle x>=0, y>=0, x+y<=6 -> area 18
        vector<HP> t = {{P(0, 0), P(1, 0)}, {P(6, 0), P(0, 6)}, {P(0, 1), P(0, 0)}};
        r = halfPlaneInter(t); assert(r.size() == 3 && fabsl(area2d(r) - 36) < 1e-6L);
        // random convex polygon intersections vs brute (clip one polygon by the other's edges)
        for (int it = 0; it < 100; it++) {
            vector<P> pa, pb;
            for (int i = 0; i < 8; i++) { pa.push_back(P(rnd(-9, 9), rnd(-9, 9))); pb.push_back(P(rnd(-9, 9), rnd(-9, 9))); }
            vector<P> a = hull(pa), b = hull(pb);
            if (a.size() < 3 || b.size() < 3) continue;
            vector<HP> h;
            for (int i = 0; i < (int)a.size(); i++) h.push_back({a[i], a[(i + 1) % a.size()]});
            for (int i = 0; i < (int)b.size(); i++) h.push_back({b[i], b[(i + 1) % b.size()]});
            vector<PD> res = halfPlaneInter(h);
            vector<PD> clip(a.begin(), a.end());
            for (int i = 0; i < (int)b.size() && !clip.empty(); i++) clip = cutPolygon(clip, PD(b[i]), PD(b[(i + 1) % b.size()]));
            ld ar = res.empty() ? 0 : area2d(res), ac = clip.empty() ? 0 : area2d(clip);
            assert(fabsl(ar - ac) < 1e-6L);
        }
    }

    // ---- circles ----
    { auto v = circleLine({PD(0, 0), 5}, PD(-1, 3), PD(1, 3));
      assert(v.size() == 2 && fabsl(fabsl(v[0].x) - 4) < EPS && fabsl(v[1].y - 3) < EPS);
      assert(circleLine({PD(0, 0), 5}, PD(-1, 5), PD(1, 5)).size() == 1);
      assert(circleLine({PD(0, 0), 5}, PD(-1, 6), PD(1, 6)).empty());
      auto w = circleCircle({PD(0, 0), 5}, {PD(6, 0), 5});
      assert(w.size() == 2 && fabsl(w[0].x - 3) < EPS && fabsl(fabsl(w[0].y) - 4) < EPS);
      assert(circleCircle({PD(0, 0), 5}, {PD(10, 0), 5}).size() == 1);
      assert(circleCircle({PD(0, 0), 5}, {PD(11, 0), 5}).empty());
      auto t = tangentPoints({PD(0, 0), 6}, PD(10, 0));
      assert(t.size() == 2 && fabsl(t[0].x - 3.6L) < EPS && fabsl(fabsl(t[0].y) - 4.8L) < EPS);
      Circle cc = circumcircle(PD(0, 0), PD(4, 0), PD(0, 3));
      assert(fabsl(cc.c.x - 2) < EPS && fabsl(cc.c.y - 1.5L) < EPS && fabsl(cc.r - 2.5L) < EPS); }

    // ---- 3D ----
    { P3 o{0, 0, 0}, a{1, 0, 0}, b{0, 1, 0}, c{0, 0, 1};
      P3 n = cross3(a, b); assert(n.x == 0 && n.y == 0 && n.z == 1);
      assert(volume6(o, a, b, c) == 1); }

    puts("all geometry tests passed");
    return 0;
}
