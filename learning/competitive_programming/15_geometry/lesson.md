# Chapter 15 — Geometry

Companion library: `example.cpp` (compile with `c++ -Wall -Wextra -std=c++17 -O2 -o ex_demo example.cpp && ./ex_demo`).
Every snippet below is in that file with tests; your job is to be able to re-type each one.

## What you'll be able to do after this chapter

- Do *all* predicates (which side, on segment, do segments cross, is the point inside, is the
  polygon convex) with `long long` only — exact, no epsilon, and know exactly when `long long`
  overflows and when `__int128` is required.
- Compute areas, lattice-point counts (Pick), convex hulls (Andrew), farthest pairs and
  minimum enclosing rectangles (rotating calipers), closest pairs (D&C and sweep), union areas
  of rectangles (sweep + segment tree), Minkowski sums, half-plane intersections, polygon cuts,
  circle intersections and tangents.
- Sort by angle without `atan2`, and — when a real number is unavoidable — pick `long double`,
  a sane `EPS`, and comparisons that don't explode.
- Recognise the statement shapes that hide geometry ("is there a line separating…", "sum of
  distances", "minimum enclosing…", "can these two shapes be moved so that…").

## Where this shows up in contests

| Shape of statement | Typical placement |
|---|---|
| Many points/segments, query a predicate (side, intersection, inside) | CF Div2 C–D (1500–1900), BOI/CEOI day-1 easy problem |
| Convex hull then something (farthest pair, hull perimeter, hull of moving points) | CF Div2 E / Div1 C (2000–2300), IOI subtasks 3–4 |
| Area of union / covered length, sweep with a structure | CF Div1 C–D, ICPC regionals almost every year |
| Half-plane intersection / Minkowski sum / polygon clipping | CF Div1 D+ (2400+), ICPC WF |
| "Lattice points inside", gcd on segments | CF Div2 D, math-heavy rounds |
| Precision traps (output with 6 decimals, `sqrt`, angles) | Anything with `double`; ICPC more than IOI |

IOI itself uses little classical geometry (the 2D-plane tasks there are usually data-structure
tasks in disguise), but BOI, CEOI and every ICPC-style contest use it. On Codeforces, geometry
problems are systematically under-solved for their rating — a reliable, exact library is a real
edge.

Prerequisites: `../../algorithms_learning/03_binary_search/lesson.md` (binary search on a
monotone predicate — used in convex point location), `07_range_queries` (segment tree — used
in the rectangle-union sweep), sorting with custom comparators.

---

## 1. Points, vectors, cross and dot — and the exact-arithmetic argument

A point *is* a vector from the origin. Store `long long`:

```cpp
typedef long long ll;
struct P {
    ll x, y;
    P(ll x = 0, ll y = 0) : x(x), y(y) {}
    P operator+(P o) const { return P(x + o.x, y + o.y); }
    P operator-(P o) const { return P(x - o.x, y - o.y); }
    P operator*(ll k) const { return P(x * k, y * k); }
    bool operator<(P o) const { return x != o.x ? x < o.x : y < o.y; }
    bool operator==(P o) const { return x == o.x && y == o.y; }
};
ll cross(P a, P b) { return a.x * b.y - a.y * b.x; }
ll dot(P a, P b)   { return a.x * b.x + a.y * b.y; }
ll cross(P o, P a, P b) { return cross(a - o, b - o); }
ll norm2(P a) { return dot(a, a); }
int sgn(ll v) { return (v > 0) - (v < 0); }
int orient(P a, P b, P c) { return sgn(cross(a, b, c)); }   // +1 ccw / left, -1 cw / right, 0 collinear
```

**Meaning.** `cross(a,b) = |a||b| sin θ` is the signed area of the parallelogram spanned by
`a` and `b`; `cross(o,a,b)` is twice the signed area of triangle `oab`, positive when `o→a→b`
turns counter-clockwise. `dot(a,b) = |a||b| cos θ`: sign tells acute/obtuse, zero is
perpendicular. Everything in this chapter is built from these two numbers plus comparisons.

```
      b                        orient(a,b,c) = +1  (c is LEFT of a->b: counter-clockwise turn)
     /                         orient(a,b,d) = -1  (d is RIGHT)
    /   c                      orient(a,b,e) =  0  (e on the line)
   a--------e
      d
```

**Why integers.** With `double`, `orient` for three nearly-collinear points can return the wrong
sign, and every algorithm built on it (hull, intersection, point-in-polygon) then produces
garbage on exactly the tests the setters wrote to kill you. With `long long`, `orient` is
always correct — *if it does not overflow*.

**Overflow bound (know this cold).** If `|x|, |y| ≤ C`:

- a difference `a - o` has components `≤ 2C`;
- each product in `cross` is `≤ (2C)² = 4C²`;
- the cross product is a difference of two products: `≤ 8C²`.

For `C = 1e9`: `8·10¹⁸ < 9.22·10¹⁸ = 2⁶³−1`. **It fits — barely.** (The intuition "1e9 × 1e9
= 1e18, times 2 is 2e18, fine" is wrong by a factor of 4 but the conclusion survives.) For
`C = 2e9` (e.g. coordinates given up to 1e9 *after* a Minkowski sum or after doubling), `8C² =
3.2e19` overflows: use `__int128` for the cross product or reduce the inputs. Also:

- `norm2` of a difference: `≤ 8C²`, same bound;
- `cross * cross` or `norm2 * norm2` (comparing squared distances of ratios): `~6.4e37` —
  **always `__int128`** (max ≈ 1.7e38, so even this is tight);
- shoelace area: sum of `n` cross products of *raw coordinates* (not differences): each `≤ 2C² =
  2e18`, and the running sum of `n = 1e5` terms can reach `2e23` in theory — but the final
  answer is bounded by twice the area of the bounding box `≤ 8C² = 8e18`. Intermediate partial
  sums *can* exceed it: translate the polygon so `p[0]` is the origin, or accumulate in
  `__int128`, or accept that in practice the sum stays far below (the terms cancel).

**Complexity.** Everything in this section is O(1). The whole point is that it stays exact.

**Pitfalls.**
- `int` coordinates with `int` arithmetic — `1e4 × 1e4` is fine, `1e5 × 1e5 = 1e10` is not.
  Always compute cross/dot in `ll` even if inputs fit `int`.
- `abs` of a `long long` — use `llabs` or `std::abs` from `<cstdlib>`; `abs` from `<stdlib.h>`
  on some compilers is `int abs(int)` and silently truncates.
- Reading coordinates as `double` "just in case" destroys exactness for coordinates above 2⁵³.

---

## 2. Segments and lines

### 2.1 Point on segment

```cpp
bool onSeg(P p, P a, P b) { return orient(a, b, p) == 0 && dot(p - a, p - b) <= 0; }
```

Collinear, and `p` is "between" `a` and `b`: the vectors `p−a` and `p−b` point in opposite
directions (or one is zero). Handles `a == b` (then `p` must equal `a`).

### 2.2 Segment–segment intersection (all degenerate cases)

```cpp
bool segInter(P a, P b, P c, P d) {
    if (onSeg(a, c, d) || onSeg(b, c, d) || onSeg(c, a, b) || onSeg(d, a, b)) return true;
    return orient(a, b, c) * orient(a, b, d) < 0 && orient(c, d, a) * orient(c, d, b) < 0;
}
```

**Proof of completeness.** Two closed segments intersect either *improperly* — some endpoint of
one lies on the other (covers touching, T-junctions, collinear overlap, point segments) — or
*properly*: they cross at a single interior point of both. In the proper case `c` and `d` are
strictly on opposite sides of line `ab` *and* `a`, `b` strictly on opposite sides of line `cd`.
If the four `onSeg` tests fail and the segments are collinear, they are disjoint (a collinear
overlap always contains an endpoint of one segment inside the other). Note the two-sided
condition is needed: "c and d on opposite sides of ab" alone accepts the case where line `ab`
separates `c,d` but segment `ab` is far away.

```
proper            touching (improper)     collinear overlap        lines cross, segments don't
  c   b             c                       a---c===b---d              c
   \ /              |                                                   \      b
    X               a---b                                                d   /
   / \              |                                                       a
  a   d             d
 true              true                     true                          false
```

**Count intersections** among *n* segments? Brute force O(n²) is fine to n ≈ 5000. For
horizontal/vertical segments only (CSES "Intersection Points") sweep in x with a Fenwick tree
over y: add a horizontal segment's y at its left end, remove at its right end, and for a
vertical segment query the count in its y-range. O((n+m) log). General segments: Bentley–Ottmann
is a nightmare to implement — in a contest you almost never need it; look for structure.

### 2.3 Line–line intersection

The intersection of lines `a + t(b−a)` and `c + s(d−c)` has
`t = cross(c−a, d−c) / cross(b−a, d−c)`. Denominator zero ⇔ parallel.

```cpp
bool lineInterFrac(P a, P b, P c, P d, ll& num, ll& den) {   // point = a + (b-a) * num/den
    den = cross(b - a, d - c); if (den == 0) return false;
    num = cross(c - a, d - c);
    if (den < 0) { den = -den; num = -num; }
    return true;
}
```

Keep the fraction when you need to *compare* intersection points exactly (e.g. sort
intersections along a line: compare `num1·den2` vs `num2·den1` in `__int128`). Convert to
`long double` only for output. The `long double` version is `a + (b−a)·t`.

### 2.4 Distance point ↔ line / segment

`dist(p, line ab) = |cross(b−a, p−a)| / |b−a|`. For a *segment*: if `dot(p−a, b−a) ≤ 0` the
closest point is `a`; if `dot(p−b, a−b) ≤ 0` it is `b`; otherwise the perpendicular foot.

Exact comparison trick: the *squared* distance to a line is `cross² / norm2`, a rational —
compare `cross₁²·norm2₂` vs `cross₂²·norm2₁` in `__int128`. You only need `sqrt` for output.

### 2.5 Lattice points on a segment

The number of integer points on the closed segment from `(x₁,y₁)` to `(x₂,y₂)` is
`gcd(|x₂−x₁|, |y₂−y₁|) + 1`. (The direction vector divided by the gcd is the primitive step;
`gcd(0, k) = k` handles axis-parallel segments; `gcd(0,0)+1 = 1` handles a point.)

---

## 3. Polygons

Store as `vector<P>`, vertices in order, edge `i` = `(p[i], p[(i+1)%n])`.

### 3.1 Area — shoelace

```cpp
ll area2(const vector<P>& p) {          // twice the signed area, > 0 iff counter-clockwise
    ll s = 0; int n = p.size();
    for (int i = 0; i < n; i++) s += cross(p[i], p[(i + 1) % n]);
    return s;
}
```

Twice the area is always an integer for integer vertices — output `A2/2` and `.5` if odd,
never `double`. Sign gives orientation: use it to normalise a polygon to ccw
(`if (area2(p) < 0) reverse(p)`). Works for any simple polygon (concave too) — the proof is
that each `cross(p[i], p[i+1])` is twice the signed area of the triangle `(0, p[i], p[i+1])`,
and the pieces outside the polygon cancel.

### 3.2 Point in polygon — ray casting, O(n)

Shoot a ray from `q` to the right; the point is inside iff it crosses the boundary an odd
number of times. The two classic bugs — ray through a vertex, ray along an edge — vanish with
the *half-open rule*: an edge counts iff exactly one endpoint is strictly above `q.y`.

```cpp
int inPoly(const vector<P>& p, P q) {   // 0 outside, 1 boundary, 2 inside; any orientation
    int n = p.size(); bool in = false;
    for (int i = 0; i < n; i++) {
        P a = p[i], b = p[(i + 1) % n];
        if (onSeg(q, a, b)) return 1;
        if ((a.y > q.y) != (b.y > q.y) && ((cross(b - a, q - a) > 0) == (b.y > a.y))) in = !in;
    }
    return in ? 2 : 0;
}
```

The second condition says "the crossing point is to the right of q": the ray hits edge `ab`
at x-coordinate `> q.x` iff `q` is to the left of `a→b` when `b` is above `a`, and to the right
otherwise. No division, exact.

```
      +-------+          q inside: ray -----> crosses 1 edge   (odd)
      |   q---|------>   q' outside: ray crosses 0 or 2 edges (even)
      |       |          a vertex exactly at q.y: counted for the edge that goes UP from it only
  q'--|-------|------->
      +-------+
```

**Winding number variant.** Add `+1` for every edge that crosses the horizontal line upward to
the right of `q`, `−1` for downward. Inside ⇔ winding ≠ 0. Same cost, but also meaningful for
self-intersecting polygons and gives you the *number* of times the polygon wraps around `q`.

### 3.3 Point in convex polygon — O(log n)

For a strictly convex polygon in ccw order, all vertices are visible from `p[0]`, so the
"fan" `(p0, p1), (p0, p2), …` is sorted by angle. Binary search for the wedge containing `q`,
then one orientation test against the far edge:

```cpp
int inConvex(const vector<P>& p, P q) {   // strictly convex, ccw; 0/1/2 as above
    int n = p.size();
    if (orient(p[0], p[1], q) < 0 || orient(p[0], p[n - 1], q) > 0) return 0;
    int lo = 1, hi = n - 1;
    while (hi - lo > 1) { int m = (lo + hi) / 2; (orient(p[0], p[m], q) >= 0 ? lo : hi) = m; }
    int s = orient(p[lo], p[lo + 1], q);
    if (s < 0) return 0;
    if (s == 0) return 1;
    if (lo == 1 && orient(p[0], p[1], q) == 0) return 1;
    if (lo == n - 2 && orient(p[0], p[n - 1], q) == 0) return 1;
    return 2;
}
```

Use when there are `q = 2e5` queries against one polygon of `n = 2e5` vertices: O((n+q) log n).

### 3.4 Pick's theorem

For a simple polygon with integer vertices, `A = I + B/2 − 1`, where `I` = interior lattice
points, `B` = boundary lattice points. You get `B` from the gcd formula per edge (each vertex
counted once: `Σ (gcd + 1) − n = Σ gcd`), `A` from the shoelace, hence
`I = (2A − B + 2) / 2`. All integers, no rounding.

```cpp
pair<ll, ll> pickCount(const vector<P>& p) {          // {interior, boundary}
    int n = p.size(); ll B = 0;
    for (int i = 0; i < n; i++) B += std::gcd(llabs(p[i].x - p[(i+1)%n].x), llabs(p[i].y - p[(i+1)%n].y));
    return {(llabs(area2(p)) - B + 2) / 2, B};
}
```

Worked trace on the triangle `(0,0),(6,0),(0,4)`: `2A = 24`; edge gcds `6, 2, 4` ⇒ `B = 12`;
`I = (24 − 12 + 2)/2 = 7`. Check by counting: interior points with `x ≥ 1, y ≥ 1, 2x + 3y < 12`:
x=1: y ∈ {1,2,3} (3); x=2: y ∈ {1,2} (2); x=3: y=1 (1); x=4: y=1 (1) → 7. ✓

Pick fails for polygons with holes (subtract per hole) and for non-lattice vertices.

---

## 4. Convex hull — Andrew's monotone chain, O(n log n)

Sort points by `(x, y)`. Build the lower hull left-to-right: keep a stack; before pushing a
point, pop while the last two stack points and the new point make a non-left turn. Then build
the upper hull right-to-left the same way. Concatenate.

```cpp
vector<P> hull(vector<P> p, bool keepCollinear = false) {   // ccw, starts at min (x,y)
    sort(p.begin(), p.end()); p.erase(unique(p.begin(), p.end()), p.end());
    int n = p.size();
    if (n <= 2) return p;
    bool allCol = true;
    for (int i = 2; i < n && allCol; i++) allCol = orient(p[0], p[1], p[i]) == 0;
    if (allCol) { if (keepCollinear) return p; return {p[0], p[n - 1]}; }
    vector<P> h(2 * n); int k = 0;
    auto bad = [&](P a, P b, P c) { ll cr = cross(a, b, c); return keepCollinear ? cr < 0 : cr <= 0; };
    for (int i = 0; i < n; i++) {                       // lower hull
        while (k >= 2 && bad(h[k - 2], h[k - 1], p[i])) k--;
        h[k++] = p[i];
    }
    for (int i = n - 2, t = k + 1; i >= 0; i--) {       // upper hull
        while (k >= t && bad(h[k - 2], h[k - 1], p[i])) k--;
        h[k++] = p[i];
    }
    h.resize(k - 1);
    return h;
}
```

**Invariant.** After processing `p[i]`, the stack is the lower hull of `p[0..i]`: every
consecutive triple turns left. Popping is amortised — each point is pushed once and popped at
most once — so the loop is O(n) after the O(n log n) sort. Correctness: a point popped is
inside the triangle `(h[k−2], p[i], leftmost)` and can never be a hull vertex again.

**Trace** on `(0,0) (2,0) (1,0) (1,1) (2,2) (0,2) (1,2)`, sorted: `(0,0)(0,2)(1,0)(1,1)(1,2)(2,0)(2,2)`.

```
lower:  push (0,0)          [ (0,0) ]
        push (0,2)          [ (0,0) (0,2) ]
        (1,0): cross((0,0),(0,2),(1,0)) = 0*(-0)... = -2 <= 0 -> pop (0,2); push   [ (0,0) (1,0) ]
        (1,1): cross = 1 > 0 -> push                                       [ (0,0) (1,0) (1,1) ]
        (1,2): cross((1,0),(1,1),(1,2)) = 0 -> pop (1,1); cross((0,0),(1,0),(1,2)) = 2 -> push  [ (0,0) (1,0) (1,2) ]
        (2,0): cross((1,0),(1,2),(2,0)) = -2 -> pop (1,2); cross((0,0),(1,0),(2,0)) = 0 -> pop (1,0); push  [ (0,0) (2,0) ]
        (2,2): cross((0,0),(2,0),(2,2)) = 4 -> push                        [ (0,0) (2,0) (2,2) ]      t = 4
upper:  visits i = n-2 .. 0, i.e. (2,0) (1,2) (1,1) (1,0) (0,2) (0,0); pops only while k >= t = 4
        (2,0): k=3 < t=4, no pop possible; push                            [ (0,0) (2,0) (2,2) (2,0) ]
        (1,2): cross((2,2),(2,0),(1,2)) = (0)(0)-(-2)(-1) = -2 -> pop (2,0); k=3 < t, stop
               -> push                                                    [ (0,0) (2,0) (2,2) (1,2) ]
        (1,1): cross((2,2),(1,2),(1,1)) = (-1)(-1)-(0)(-1) = 1 > 0 keep; push  [ ... (2,2) (1,2) (1,1) ]
        (1,0): cross((1,2),(1,1),(1,0)) = 0 -> pop (1,1); cross((2,2),(1,2),(1,0)) = (-1)(-2)-0 = 2 -> push
        (0,2): cross((1,2),(1,0),(0,2)) = (0)(0)-(-2)(-1) = -2 -> pop (1,0); cross((2,2),(1,2),(0,2)) = 0 -> pop (1,2)
               push                                                       [ (0,0) (2,0) (2,2) (0,2) ]
        (0,0): cross((2,2),(0,2),(0,0)) = (-2)(-2)-(0)(-2) = 4 -> push;  resize(k-1) drops the duplicate (0,0)
result: (0,0) (2,0) (2,2) (0,2)   -- the 4 corners, ccw
```

(The first line of the upper pass shows the reason for `t = k+1`: the lower hull must never
be popped by the upper pass.)

**Collinear points.** With `cr <= 0` as "bad" you drop collinear boundary points and get a
*strictly* convex hull — what calipers and `inConvex` require. With `cr < 0` you keep them
(CSES "Convex Hull" accepts either, but many problems ask for "all points on the boundary").
The all-collinear special case is required in the keep-collinear mode: otherwise the two
passes produce every point twice.

**Variants.** Graham scan (sort by angle around the lowest point) — same complexity, more
error-prone; skip. Jarvis march O(nh) — only when `h` is tiny. Dynamic hull (insert points,
query tangent) — `std::set` of upper and lower hulls, O(log n) amortised per insert; CF 70D.
Hull of `n = 1e6` points in 1 s: fine (`sort` dominates).

---

## 5. Rotating calipers

On a strictly convex ccw polygon, many "extreme pair" quantities can be found in O(n) after
the O(n log n) hull, because as you rotate a direction around the polygon the extreme vertex in
that direction moves monotonically around the boundary.

### 5.1 Diameter (farthest pair of points)

The farthest pair of a point set is a pair of hull vertices, and it is *antipodal*: there
exist two parallel supporting lines through them. For each edge `i`, advance `j` while the
next vertex is farther from edge `i`'s line; candidates are `(i, j)` and `(i+1, j)`.

```cpp
ll diameter2(const vector<P>& h) {
    int n = h.size(); if (n <= 1) return 0; if (n == 2) return norm2(h[1] - h[0]);
    ll best = 0; int j = 1;
    for (int i = 0; i < n; i++) {
        P e = h[(i + 1) % n] - h[i];
        while (cross(e, h[(j + 1) % n] - h[j]) > 0) j = (j + 1) % n;   // next vertex is still farther
        best = max({best, norm2(h[i] - h[j]), norm2(h[(i + 1) % n] - h[j])});
    }
    return best;
}
```

`cross(e, h[j+1] − h[j]) > 0` means the edge `j→j+1` still moves *away* from line `i`. Total
pointer movement is `≤ 2n`. Keep the result as a squared integer.

### 5.2 Minimum width and minimum-area bounding rectangle

**Theorem.** A minimum-area enclosing rectangle has one side collinear with a hull edge.
(Rotate the rectangle slightly: the area as a function of angle is concave between two
consecutive "edge-flush" angles, so the minimum is at an endpoint.) So: for each edge `i`,
maintain three pointers — `j` = farthest from the edge (max `cross`), `r` = farthest along
the edge direction (max `dot`), `l` = farthest against it (min `dot`). All three rotate
monotonically. Height = `cross(e, h[j]−h[i]) / |e|`, width = `(dot_r − dot_l)/|e|`,
area = height·width. Minimum *width* of the polygon is just `min` over `i` of height.
Implementation in `example.cpp` (`minWidthAndRect`); the one subtlety is that pointer
advancement must use `>=`/`<=` so a two-point plateau (edge parallel to `e`) is stepped over,
and a guard so a pointer never overtakes `i`.

### 5.3 Other calipers

Distance between two disjoint convex polygons, width in a fixed direction, merging two hulls
(bridge finding), onion layers — all the same two-pointer discipline.

---

## 6. Closest pair of points

### 6.1 Divide and conquer, O(n log n)

Sort by x, split in half, recurse, `d = min(left, right)`. Only pairs within the vertical strip
`|x − x_mid| < d` remain. Merge the halves by y (so the whole recursion is O(n log n), not
O(n log² n)); for each strip point compare with the following strip points while `dy < d`.

**Strip argument.** In a `d × d` square on one side of the split, any two points are at
distance `≥ d` (they came from the same half), so it contains at most 4 points; hence at most
~7 successors in y-order need checking before `dy ≥ d`. The inner loop is O(1) amortised.

```cpp
ll closestDC(vector<P>& a, int lo, int hi) {   // a sorted by x on entry, by y on exit
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
```

### 6.2 Sweep with `std::set`, O(n log n) — shorter, the one to remember

Sort by x. Maintain the set of points whose x is within `d` of the current one, keyed by
`(y, x)`. For each new point, erase points too far left, then look only at set entries with
`y ∈ [y−d, y+d]` — by the packing argument there are O(1) of them.

```cpp
ll closestPairSweep(vector<P> a) {
    sort(a.begin(), a.end());
    set<pair<ll, ll>> s; ll best = LLONG_MAX; int left = 0;
    for (int i = 0; i < (int)a.size(); i++) {
        ll d = best == LLONG_MAX ? LLONG_MAX / 4 : (ll)sqrtl((long double)best) + 1;
        while (a[i].x - a[left].x >= d) { s.erase({a[left].y, a[left].x}); left++; }
        for (auto it = s.lower_bound({a[i].y - d, LLONG_MIN}); it != s.end() && it->first <= a[i].y + d; ++it)
            best = min(best, norm2(a[i] - P(it->second, it->first)));
        s.insert({a[i].y, a[i].x});
    }
    return best;
}
```

`d = floor(sqrt(best)) + 1` over-approximates the true distance, which is safe (we only prune
points with `dx ≥ d`). `n = 2e5` with coordinates to 1e9: ~0.1 s. CSES "Minimum Euclidean
Distance" wants the *squared* distance — never take the square root.

---

## 7. Sweep line with a segment tree: union area of rectangles

Events: each rectangle contributes `(y1, +1)` and `(y2, −1)` over its x-interval. Sweep
upward; between consecutive event y's the covered x-length is constant, so
`area += coveredLength · Δy`. Covered length is maintained by a segment tree over the
*compressed* x-coordinates where each node stores `cnt` (how many rectangles cover the whole
node range — never pushed down) and `len` (covered length inside the node):

```cpp
struct CoverTree {
    int n; vector<int> cnt; vector<ll> len; vector<ll> xs;
    CoverTree(const vector<ll>& xs_) : xs(xs_) { n = xs.size() - 1; cnt.assign(4 * n, 0); len.assign(4 * n, 0); }
    void upd(int node, int l, int r, int ql, int qr, int v) {   // elementary x-intervals [l, r)
        if (qr <= l || r <= ql) return;
        if (ql <= l && r <= qr) cnt[node] += v;
        else { int m = (l + r) / 2; upd(2*node, l, m, ql, qr, v); upd(2*node+1, m, r, ql, qr, v); }
        if (cnt[node] > 0) len[node] = xs[r] - xs[l];
        else len[node] = (r - l == 1) ? 0 : len[2*node] + len[2*node+1];
    }
};
```

The node value depends only on `cnt[node]` and the children — no lazy push needed because
we never query below the root. O(n log n). For CSES "Area of Rectangles" (n = 1e5,
coordinates to 1e6) the answer fits `long long` (≤ 4e12).

```
y ^                   events (y):  1:+A   2:+B   3:-A   4:-B
  |   +-----+         between y=1..2 covered = |A_x| ; y=2..3 covered = |A_x ∪ B_x| ; y=3..4 = |B_x|
  | +-|--+  |  B
  | | +--|--+
  | |    |     A
  | +----+
  +------------> x    (compressed x: only the 4 distinct rectangle x's matter)
```

**Variants.** Perimeter of the union (track number of maximal covered runs as well). Count
points covered by ≥ k rectangles. "Number of segments intersecting a vertical line" — the same
sweep with a Fenwick tree. Union of squares/Manhattan balls after rotation.

---

## 8. Half-plane intersection, O(n log n)

Each half-plane is "left of the directed line `a→b`". The intersection of `n` of them is a
convex polygon (possibly empty or unbounded — add a huge bounding box to make it bounded).

Algorithm: sort the lines by direction angle; sweep with a deque. When adding a line `L`,
pop from the back while the intersection point of the last two deque lines is outside `L`;
likewise from the front; push `L`. After all lines, trim: pop from the back while its last
vertex is outside the first line, and from the front symmetrically. Vertices = intersections
of consecutive deque lines. Parallel same-direction lines: keep only the most restrictive
(dedupe after sorting). Anti-parallel consecutive lines with empty overlap ⇒ empty result.

Implementation in `example.cpp` (`halfPlaneInter`), with the angular sort done on the
*integer* direction vectors (exact) and only the vertices in `long double`. Check your result
by clipping (Section 9) on random convex polygons — that is exactly what the test does.

Uses: intersection of convex polygons (feed all edges), "is there a point seeing all
segments", kernel of a polygon, feasibility of `n` linear constraints in 2 variables,
"maximum radius circle fitting inside a convex polygon" (binary search on `r`, shift each
half-plane inward by `r`, test non-empty).

---

## 9. Polygon cutting and Minkowski sum

### 9.1 Cut a polygon by a line, O(n)

Keep the left side of `a→b`: walk the edges; output each vertex on the kept side; whenever an
edge crosses the line, output the crossing point. Convex in ⇒ convex out. Clipping a convex
polygon by all edges of another gives their intersection in O(nm) — often simpler than
half-plane intersection when `n·m ≤ 1e7`.

### 9.2 Minkowski sum of convex polygons, O(n + m)

`A ⊕ B = {a + b}`. For convex `A`, `B` it is convex and its edges are the edges of `A` and
`B` **merged by angle**. Start both at the lowest (then leftmost) vertex so both edge
sequences start at angle ≥ 0, then merge like merge-sort with `cross` as the comparator
(`c ≥ 0` advance `A`, `c ≤ 0` advance `B`; both when parallel).

```cpp
vector<P> minkowski(vector<P> a, vector<P> b) {   // both strictly convex ccw
    reorder(a); reorder(b);                       // rotate so index 0 = lowest-leftmost
    int n = a.size(), m = b.size();
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
```

Classic use: "can polygon A be translated so that it intersects polygon B" ⇔
`0 ∈ B ⊕ (−A)`; "minimum distance between two convex polygons" = distance from origin to
`B ⊕ (−A)`. Coordinates double in magnitude — recheck the overflow bound.

---

## 10. Circles (`long double`)

- **Circle–line.** Foot of the perpendicular `f` from the centre; `h² = r² − |f−c|²`;
  `h² < 0` none, `≈ 0` tangent, else `f ± dir·sqrt(h²)/|dir|`.
- **Circle–circle.** Distance `d` between centres; none if `d > r₁+r₂` or `d < |r₁−r₂|`;
  `x = (d² + r₁² − r₂²)/(2d)` along the centre line, `h = sqrt(r₁² − x²)` perpendicular.
- **Tangents from a point.** The tangent points are the intersection of the circle with the
  circle centred at `p` of radius `sqrt(|p−c|² − r²)` — reuse circle–circle.
- **Circumcircle of 3 points**: perpendicular-bisector formula (in `example.cpp`).
- **Minimum enclosing circle**: Welzl's randomized incremental algorithm, expected O(n) —
  shuffle, keep the current circle, when a point is outside make it a boundary point and
  recurse. Needs circumcircle. (AtCoder ABC 151 F.)

Circles and angles are where `double` bites; see Section 12.

---

## 11. Angular sort without `atan2`

`atan2` is slow-ish and, worse, inexact: two vectors in exactly the same direction can get
different angles. Split the plane into two halves and compare by `cross` within a half:

```cpp
int half(P p) { return (p.y > 0 || (p.y == 0 && p.x > 0)) ? 0 : 1; }   // [0, pi) -> 0, [pi, 2pi) -> 1
bool angLess(P a, P b) { int ha = half(a), hb = half(b); return ha != hb ? ha < hb : cross(a, b) > 0; }
```

Exact, total order (equal directions compare equal). Sort the vectors from a common origin,
then a radial sweep is two-pointer: "how many points are within the angle `θ` of each other",
"count triangles containing the origin", "minimum angle between any two", "widest empty
wedge". Never call `atan2` in a comparator with `double` ties.

---

## 12. When doubles are unavoidable: precision rules

1. **Prefer `long double`** on x86-64 judges (80-bit, 64-bit mantissa; Codeforces and CSES
   both support it). It is ~2× slower than `double` — irrelevant for O(n log n).
2. **Delay `sqrt` and division** as long as possible: compare squared distances, compare
   fractions by cross-multiplication, output only at the end.
3. **`EPS`**: `1e-9` for coordinates up to ~1e6 in `double`; `1e-12` or smaller in
   `long double`. Rule of thumb: relative machine precision (`1e-16` / `1e-19`) × magnitude
   of the *intermediate* quantities × a safety factor of 1e3. If your intermediates reach
   `1e18`, an absolute `EPS = 1e-9` is meaningless — compare relatively:
   `|a−b| <= EPS · max(1, |a|, |b|)`.
4. **Never test `x == 0.0`** after arithmetic; use `sgn(x)` with `EPS`. Never use an
   `EPS`-comparison as a `std::set`/`sort` comparator — it is not transitive and corrupts the
   container. Round to a grid first if you must.
5. **`acos`/`asin` arguments** must be clamped to `[−1, 1]`; `acos(1.0000000002)` is NaN.
6. **Output**: `printf("%.9Lf")`; with `%.6f` requested, print more digits than asked (extra
   digits never hurt with a checker that uses tolerance; they do hurt with exact matching —
   read the statement).
7. **Angles**: avoid them entirely if you can (Section 11). If you need `π`, use `acosl(-1)`.
8. **Sanity**: when a `double` solution behaves oddly, compute the same thing in exact
   rationals on small tests; almost always the bug is an `EPS` decision, not the geometry.

---

## 13. 3D basics (mention)

`cross3(a,b)` gives a normal vector; `dot3(cross3(b−a, c−a), d−a)` is 6× the signed volume
of tetrahedron `abcd` (orientation test in 3D). Distance from a point to a plane through
`a` with normal `n` is `|dot(n, p−a)| / |n|`. 3D convex hull (gift wrapping O(n²), or
incremental O(n²)/O(n log n)) appears in ICPC finals, not in IOI/BOI — know the primitives,
don't memorise the hull.

---

## Recognition cheatsheet

| Statement signal | Technique | Complexity |
|---|---|---|
| "on which side", "is the polygon convex", "left turn" | `orient` | O(1) each |
| "do the two segments intersect" (with touching cases) | `segInter` with `onSeg` | O(1) |
| "how many pairs of segments intersect" (axis-parallel) | sweep + Fenwick | O(n log n) |
| "area of the polygon" (integer vertices) | shoelace, output `A2/2` with `.5` | O(n) |
| "inside / outside / boundary" for many query points, general polygon | ray casting per query | O(nq) |
| same, polygon convex, n,q large | `inConvex` binary search | O((n+q) log n) |
| "lattice points inside / on boundary" | Pick + gcd | O(n log C) |
| "smallest convex polygon containing", "fence" | Andrew's hull | O(n log n) |
| "farthest pair", "diameter" | hull + calipers | O(n log n) |
| "smallest rectangle / strip containing all points" | hull + 3-pointer calipers | O(n log n) |
| "closest pair" | sweep with `set` | O(n log n) |
| "area / perimeter of union of rectangles" | y-sweep + cover segment tree | O(n log n) |
| "intersection of convex polygons", "n linear inequalities in 2D", "kernel" | half-plane intersection (or repeated clipping) | O(n log n) / O(nm) |
| "translate shape A so that it touches/avoids B" | Minkowski sum `B ⊕ (−A)` | O(n + m) |
| "sort by angle", "sweep a ray", "count triangles containing O" | `half()`+`cross` comparator | O(n log n) |
| "max Manhattan distance" | rotate 45°: `(x+y, x−y)` → Chebyshev, keep max/min | O(n) |
| "sum of pairwise Manhattan distances" | sort each coordinate separately, prefix sums | O(n log n) |
| circles: tangents, intersections, enclosing circle | `long double`, Welzl | O(n) expected |
| answer requested to 1e-6, ugly numbers | delayed sqrt, `long double`, relative EPS | — |

---

## Implementation checklist for contests

- [ ] Coordinates read as `long long`; every product is of `long long`s; bound checked
      (`|coord| ≤ 1e9` ⇒ cross fits; anything squared ⇒ `__int128`).
- [ ] `orient` returns `−1/0/+1` (sign, not the raw cross) and every predicate handles the `0` case explicitly.
- [ ] Segment intersection includes the four `onSeg` tests before the strict sign tests.
- [ ] Polygon orientation normalised to ccw before hull-dependent routines (`inConvex`, calipers, Minkowski).
- [ ] Hull: duplicates removed; all-collinear case handled; strict vs. keep-collinear chosen deliberately.
- [ ] Ray casting uses the half-open rule; boundary detected first via `onSeg`.
- [ ] Pick: `B` counts each vertex once; area taken with `llabs`.
- [ ] Sweep-line events sorted with the right tie-break (for closed intervals: `+1` before `−1` at equal coordinate when touching counts).
- [ ] Closest pair: output the squared distance if asked; `d` for pruning over-approximates.
- [ ] Doubles: `long double`, `EPS` chosen for the intermediate magnitude, `sqrt`/`acos` arguments clamped, printed with ≥ 9 decimals.
- [ ] Stress test every predicate against the brute-force O(n²)/O(n³) version on random small
      coordinates (`−5..5` — small ranges produce the degenerate cases) before submitting.

---

## Further reading

- CPH (Laaksonen, *Competitive Programmer's Handbook*) ch. 29 "Geometry", ch. 30 "Sweep line algorithms".
- cp-algorithms.com: "Basic Geometry", "Finding the area of simple polygon", "Check if point
  belongs to the convex polygon in O(log N)", "Pick's theorem", "Convex Hull construction",
  "Minkowski sum of convex polygons", "Half-plane intersection – S&I algorithm", "Finding the
  nearest pair of points", "Search for a pair of intersecting segments", "Circle-Line
  Intersection", "Circle-Circle Intersection", "Common tangents to two circles".
- Shamos (1978), rotating calipers; Toussaint (1983), "Solving geometric problems with the
  rotating calipers".
- Preparata & Shamos, *Computational Geometry*, ch. 3–5 (hulls, proximity), for proofs.
- Victor Lecomte, *Handbook of geometry for competitive programmers* (free PDF) — the best
  contest-oriented treatment of precision.

## You can move on when...

- You can write `P`, `cross`, `orient`, `onSeg`, `segInter`, `area2`, `inPoly`, `hull`,
  `closestPairSweep` from memory in under 25 minutes total, and they pass the tests in
  `example.cpp` when pasted in place of the originals.
- You can state the overflow bound for `cross` with `|coord| ≤ 1e9` and say which two
  operations in this chapter need `__int128`.
- You have solved all 7 tasks of the classical CSES geometry block (`problems.md` 15.1–15.7)
  plus "Intersection Points" and "Area of Rectangles", and at least three of the
  Codeforces/AtCoder problems listed.
- You can explain, without notes, why the strip in closest-pair D&C has O(1) candidates per
  point, and why the minimum-area rectangle has a side flush with a hull edge.
