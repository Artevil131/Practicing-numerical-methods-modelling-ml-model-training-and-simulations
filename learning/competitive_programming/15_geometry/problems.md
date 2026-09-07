# Chapter 15 — Problems

How to work them:

1. Before the first problem, re-type the library from memory into
   `competitive_programming/15_geometry/solutions/geo.h` (P, cross, orient, onSeg, segInter,
   area2, inPoly, inConvex, hull, closestPairSweep). Diff against `example.cpp`. Fix. Repeat
   tomorrow.
2. Each problem goes in `competitive_programming/15_geometry/solutions/<source>_<id>.cpp`.
3. Stress-test every predicate-based solution against a brute-force version on random points
   with coordinates in `[-5, 5]` — small ranges generate the collinear / touching / duplicate
   cases the judge is waiting for.
4. Time yourself: ★1–★2 in ≤ 20 min, ★3 in ≤ 45 min, ★4–★5 upsolve after reading the hint only.
5. Upsolve everything you failed within a week, from a blank file.

Difficulty: ★1 (CF ~1200) … ★5 (CF ~2500+). Ratings for Codeforces problems are approximate.

---

## A. Exact primitives — the CSES geometry block

### 15.1  Point Location Test  ·  CSES 2189  ·  ★1
https://cses.fi/problemset/task/2189
**Technique:** orientation test (`cross`, sign).
<details><summary>Hint</summary>Coordinates go to 1e9. What is the largest intermediate value, and does it fit in `long long`?</details>
<details><summary>Approach sketch</summary>For each query compute `sgn(cross(p2 - p1, p3 - p1))` and print LEFT / RIGHT / TOUCH. Differences are ≤ 2e9, products ≤ 4e18, the difference ≤ 8e18 — fits `long long` with ~1e18 to spare. Do not use `double`: 4e18 has no exact representation.</details>

### 15.2  Line Segment Intersection  ·  CSES 2190  ·  ★2
https://cses.fi/problemset/task/2190
**Technique:** segment–segment intersection with all degenerate cases.
<details><summary>Hint</summary>Enumerate the ways two closed segments can share a point: proper crossing, endpoint touching the other segment, collinear overlap, a segment that is a single point. Which single helper covers everything but the first?</details>
<details><summary>Approach sketch</summary>If any endpoint of one segment lies on the other (`onSeg`: collinear and `dot(p-a, p-b) <= 0`) answer YES. Otherwise the segments intersect iff `c, d` are strictly on opposite sides of line `ab` AND `a, b` strictly on opposite sides of line `cd`. Test your code on the eleven cases in `example.cpp` before submitting — the judge has all of them.</details>

### 15.3  Polygon Area  ·  CSES 2191  ·  ★1
https://cses.fi/problemset/task/2191
**Technique:** shoelace formula.
<details><summary>Hint</summary>The task asks for twice the area. Why is that a hint about the output type?</details>
<details><summary>Approach sketch</summary>Sum `cross(p[i], p[i+1])` over the closed cycle, print the absolute value. Twice the area of a lattice polygon is an integer, so no `double` anywhere. Bound: n ≤ 1000 vertices with |coord| ≤ 1e9, partial sums stay within `long long` because the final value is at most twice the bounding-box area (8e18) and terms cancel; to be safe translate all points by `-p[0]` first.</details>

### 15.4  Point in Polygon  ·  CSES 2192  ·  ★2
https://cses.fi/problemset/task/2192
**Technique:** ray casting with the half-open rule; boundary via `onSeg`.
<details><summary>Hint</summary>The polygon may be concave and the query point may lie exactly on a vertex or on an edge; the ray may pass exactly through a vertex. Decide the rule that makes each vertex count exactly once.</details>
<details><summary>Approach sketch</summary>For each query point, first check every edge with `onSeg` → BOUNDARY. Otherwise count edges with exactly one endpoint strictly above `q.y` whose crossing with the horizontal line lies to the right of `q` (sign of `cross(b-a, q-a)` compared with the edge direction). Odd → INSIDE. n, m ≤ 1000 so O(nm) = 1e6 is trivial; the difficulty is purely the degenerate cases.</details>

### 15.5  Polygon Lattice Points  ·  CSES 2193  ·  ★2
https://cses.fi/problemset/task/2193
**Technique:** Pick's theorem + gcd.
<details><summary>Hint</summary>Two integers determine both answers: twice the area and the number of boundary lattice points.</details>
<details><summary>Approach sketch</summary>`B = Σ gcd(|dx|, |dy|)` over edges (each vertex counted once). `2A` from the shoelace. Then `I = (2A - B + 2) / 2`. Print `I` and `B`. Everything integer; the division is exact because `2A - B` is always even for a lattice polygon.</details>

### 15.6  Minimum Euclidean Distance  ·  CSES 2194  ·  ★3
https://cses.fi/problemset/task/2194
**Technique:** closest pair — sweep with `std::set` (or divide & conquer).
<details><summary>Hint</summary>Output is the squared distance; never compute a square root except to bound the sweep window. Why is the number of candidates per point O(1)?</details>
<details><summary>Approach sketch</summary>Sort by x. Keep a `set<(y,x)>` of the points with `x` within `d` of the current one (`d = floor(sqrt(best)) + 1`), erase from the left as the sweep advances, and for each new point test only set entries with `y ∈ [y-d, y+d]`. Packing: in a `d × 2d` window points are pairwise ≥ `d` apart, so there are at most ~6. n = 2e5, coordinates to 1e9: squared distances reach 8e18 — `long long`, not `int`, not `double`.</details>

### 15.7  Convex Hull  ·  CSES 2195  ·  ★3
https://cses.fi/problemset/task/2195
**Technique:** Andrew's monotone chain.
<details><summary>Hint</summary>The output wants all points on the hull boundary, collinear ones included. What changes in the pop condition — and what breaks when all points are collinear?</details>
<details><summary>Approach sketch</summary>Sort, dedupe, lower chain then upper chain popping while `cross < 0` (keep collinear). Handle the all-collinear case separately (return the sorted unique points once, otherwise the two passes emit them twice). Print the count and the points in boundary order. Stress against the O(n³) "edge with all points on its left" brute force.</details>

## B. Sweeps, rotations, transformations — the rest of the CSES section

### 15.8  Maximum Manhattan Distances  ·  CSES 3410  ·  ★2
https://cses.fi/problemset/task/3410
**Technique:** 45° rotation `(x+y, x−y)` turns Manhattan into Chebyshev.
<details><summary>Hint</summary>`|x1-x2| + |y1-y2| = max(|u1-u2|, |v1-v2|)` with `u = x+y`, `v = x−y`.</details>
<details><summary>Approach sketch</summary>Maintain running `max/min` of `u` and of `v` as points arrive; the maximum Manhattan distance among the points so far is `max(maxU-minU, maxV-minV)`. O(1) per point, so a prefix answer for every insertion is free.</details>

### 15.9  All Manhattan Distances  ·  CSES 3411  ·  ★2
https://cses.fi/problemset/task/3411
**Technique:** separability of Manhattan distance; sorting + prefix sums.
<details><summary>Hint</summary>`Σ|xi−xj| + Σ|yi−yj|` — the two sums are independent.</details>
<details><summary>Approach sketch</summary>Sort the x-coordinates; the i-th smallest contributes `x_i * i - prefix(i)` to the sum of pairwise differences. Same for y. Add. O(n log n). Check the magnitude against the statement: `n²/2` pairs times a distance up to `2C` is `4e19` for n = 2e5, C = 1e9, which overflows `long long` — if the constraints are that large, accumulate in `__int128` (or `unsigned long long` if the bound is just under 1.8e19).</details>

### 15.10  Intersection Points  ·  CSES 1740  ·  ★3
https://cses.fi/problemset/task/1740
**Technique:** sweep line + Fenwick tree (horizontal/vertical segments).
<details><summary>Hint</summary>Sweep in x. A horizontal segment is "alive" between its endpoints; a vertical segment asks "how many alive y's lie in my range".</details>
<details><summary>Approach sketch</summary>Events sorted by x: horizontal start (+1 at y), vertical segment (query count of y in [y1,y2]), horizontal end (−1 at y) — in that order at equal x so touching endpoints count. Fenwick tree over compressed y. O((n) log n) for n = 1e5.</details>

### 15.11  Area of Rectangles  ·  CSES 1741  ·  ★3
https://cses.fi/problemset/task/1741
**Technique:** y-sweep with a "cover count + covered length" segment tree.
<details><summary>Hint</summary>Between two consecutive event y's the covered x-length is constant. What structure maintains "total covered length" under add/remove interval?</details>
<details><summary>Approach sketch</summary>Compress x. Segment tree where each node stores how many rectangles fully cover it and the covered length below it (no lazy push — never queried below the root). Sort events by y, accumulate `covered * dy`. O(n log n), answer up to 4e12.</details>

### 15.12  Robot Path  ·  CSES 1742  ·  ★4
https://cses.fi/problemset/task/1742
**Technique:** axis-parallel segment intersection with a sweep / ordered sets; first self-intersection.
<details><summary>Hint</summary>The robot walks n axis-parallel segments and stops at the first point that was already visited. You need the earliest intersection of segment i with any earlier segment — including collinear overlaps with the segment two steps back.</details>
<details><summary>Approach sketch</summary>Each new segment is horizontal or vertical. Keep the earlier horizontal segments indexed by y (map from y to a set of x-intervals) and vertical ones by x. When a new vertical segment is added, it can hit earlier horizontals whose y lies in its y-range: iterate the ys in that range (a `map` range query) and check x-containment — total work is bounded because a hit stops the walk, but a full scan per step is O(n²) in the worst case; use a segment tree of sets keyed by y, or the offline trick: sort candidate intersections and take the earliest along the path. Handle the collinear case (walking back over your own trail) explicitly, it is the most common WA.</details>

### 15.13  Line Segments Trace I  ·  CSES 3427  ·  ★3
https://cses.fi/problemset/task/3427
**Technique:** read the statement first — segment intersection primitives with a walk/ordering component.
<details><summary>Hint</summary>Solve it with the exact `segInter` and `onSeg` helpers; if a scan over all segments per step passes the constraints, do that first, then optimise.</details>
<details><summary>Approach sketch</summary>Newer CSES task; the shape is "follow/trace along segments and report what is hit". Build the solution from the chapter's primitives (`segInter`, exact line intersection as a fraction to order hits along a segment) and only optimise if the O(n²) version fails the largest subtask.</details>

### 15.14  Line Segments Trace II  ·  CSES 3428  ·  ★4
https://cses.fi/problemset/task/3428
**Technique:** as 15.13 with tighter constraints — sweep line / ordered structure instead of a scan.
<details><summary>Hint</summary>Whatever the per-step scan in Trace I looked for, it must now be answered by a data structure: sort events, keep the "active" segments in a `set` ordered along the sweep.</details>
<details><summary>Approach sketch</summary>Same primitives, but the active set of segments is maintained in a balanced tree ordered by their position along the sweep direction (compare two segments by exact orientation at the current sweep position). Standard sweep-line discipline: every comparison exact, ties handled by index.</details>

### 15.15  Lines and Queries I  ·  CSES 3429  ·  ★3
https://cses.fi/problemset/task/3429
**Technique:** upper envelope of lines (convex hull trick) — geometry meets DP optimisation.
<details><summary>Hint</summary>The maximum of `a_i·x + b_i` over all lines at a given x is a convex piecewise-linear function; its pieces are the lines on the upper convex hull of the points `(a_i, b_i)`.</details>
<details><summary>Approach sketch</summary>Sort lines by slope, build the upper envelope with the same pop test as a convex hull (a line is useless if the intersection of its neighbours makes it never on top — compare with exact cross-multiplication in `__int128`), then answer each query by binary search over the breakpoints. O((n+q) log n).</details>

### 15.16  Lines and Queries II  ·  CSES 3430  ·  ★4
https://cses.fi/problemset/task/3430
**Technique:** Li Chao tree or dynamic upper envelope (lines arrive online / mixed with queries).
<details><summary>Hint</summary>If lines and queries interleave, the static envelope must be replaced by a structure supporting insert-line and query-max at x: a Li Chao tree over the x-range is 30 lines of code.</details>
<details><summary>Approach sketch</summary>Li Chao tree: each node keeps one line; on insert, keep the better line at the node's midpoint and push the other to the side where it can still win. O(log C) per insert and query. Read the statement to check whether the queries are on integer x in a bounded range (Li Chao) or arbitrary (then use a `set`-based dynamic hull).</details>

## C. Codeforces / AtCoder

### 15.17  Convex Quadrilateral  ·  AtCoder ABC 266 C  ·  ★1
https://atcoder.jp/contests/abc266/tasks/abc266_c
**Technique:** orientation.
<details><summary>Hint</summary>A quadrilateral given in order is strictly convex iff all four consecutive turns have the same sign.</details>
<details><summary>Approach sketch</summary>Compute `orient` for the four consecutive triples; answer Yes iff all are `+1` (or all `−1`). Zero means a straight angle — not allowed here.</details>

### 15.18  Opposite  ·  AtCoder ABC 197 D  ·  ★2
https://atcoder.jp/contests/abc197/tasks/abc197_d
**Technique:** rotation about a point (`long double`).
<details><summary>Hint</summary>The centre of the regular polygon is the midpoint of `p0` and `p_{n/2}`; `p1` is `p0` rotated by `2π/n` about it.</details>
<details><summary>Approach sketch</summary>Translate so the centre is the origin, rotate `p0 - c` by `2π/n` with `rot`, translate back. Print with 10 decimals.</details>

### 15.19  Congruence Points  ·  AtCoder ABC 207 D  ·  ★3
https://atcoder.jp/contests/abc207/tasks/abc207_d
**Technique:** normalising point sets up to rotation/translation; exact via scaling.
<details><summary>Hint</summary>Translate both sets so their centroid is at the origin (multiply all coordinates by n to stay integer). Then try mapping one fixed point of S to each point of T with the same norm: this fixes the rotation.</details>
<details><summary>Approach sketch</summary>Centroid-translate (scaled by n), pick `s0 ≠ 0`; for each `t` in T with `norm2(t) == norm2(s0)`, the rotation is determined by `cos, sin = dot/(norm), cross/(norm)`; apply it to every point of S as exact rationals (or scale by norm2 to keep integers) and check set equality with a `set`. O(n² log n) for n ≤ 100.</details>

### 15.20  Enclose All  ·  AtCoder ABC 151 F  ·  ★3
https://atcoder.jp/contests/abc151/tasks/abc151_f
**Technique:** minimum enclosing circle — Welzl (or ternary search on the centre).
<details><summary>Hint</summary>The optimal circle is determined by 2 or 3 of the points. n ≤ 50, so even the O(n⁴) "try all pairs and triples" passes — but implement Welzl once, you will need it later.</details>
<details><summary>Approach sketch</summary>Shuffle; maintain circle C; for each point outside C, rebuild with that point on the boundary (recursively with one, then two fixed boundary points using the circumcircle). Expected O(n). Output radius with 10 decimals in `long double`.</details>

### 15.21  Ancient Berland Circus  ·  Codeforces 1C  ·  ★4 (≈2100)
https://codeforces.com/problemset/problem/1/C
**Technique:** circumcircle, central angles, gcd of real numbers with tolerance.
<details><summary>Hint</summary>Three vertices of a regular n-gon lie on its circumcircle. The central angles they subtend are multiples of `2π/n`; find the largest angle step that divides all three — a real-valued gcd with an epsilon.</details>
<details><summary>Approach sketch</summary>Circumradius from the triangle (`R = abc / (4·Area)`), central angles via the law of cosines, then `gcd(α, β, 2π−α−β)` with tolerance ~1e-4 (n ≤ 100 gives a lower bound on the step). Area = `n/2 · R² · sin(2π/n)`. A classic precision problem — `long double` and a careful `fmod` loop.</details>

### 15.22  Polygons  ·  Codeforces 166B  ·  ★4 (≈2100)
https://codeforces.com/problemset/problem/166/B
**Technique:** convex hull with collinear handling; strictly-inside test for many points.
<details><summary>Hint</summary>Polygon A is convex; B is inside A strictly iff the hull of A ∪ B (keeping collinear points) contains no vertex of B. Why is that equivalent, and why must collinear points be kept?</details>
<details><summary>Approach sketch</summary>Tag every point with its origin, build the hull of the union in keep-collinear mode, and answer YES iff no B-point appears on the hull. A B-point on A's boundary would be kept as a collinear hull point → correctly rejected. O((n+m) log). Alternative: `inConvex` per B point with strict interior required.</details>

### 15.23  Nearest vectors  ·  Codeforces 598C  ·  ★4 (≈2300)
https://codeforces.com/problemset/problem/598/C
**Technique:** angular sort without `atan2`, exact comparison of angles between adjacent pairs.
<details><summary>Hint</summary>Coordinates are up to 1e4 in absolute value but the answer needs the *smallest angle* among 1e5 vectors: `atan2` in `double` loses. Sort exactly, then compare adjacent angles exactly.</details>
<details><summary>Approach sketch</summary>Sort by `half()` + `cross`. The minimum angle is between two adjacent vectors in this order (including the wrap-around pair). To compare two angles `∠(a,b)` and `∠(c,d)` without floating point: both are in `(0, π]` so compare their cosines via `dot/|·|` — i.e. compare `dot(a,b)·|c||d|` vs `dot(c,d)·|a||b|` by squaring carefully (sign first, then `__int128` or `long double` with a relative check). Many accepted solutions use `long double atan2l`; know why `double` fails.</details>

### 15.24  Professor's task  ·  Codeforces 70D  ·  ★5 (≈2700)
https://codeforces.com/problemset/problem/70/D
**Technique:** dynamic convex hull (insert point, query inside) with `std::set`.
<details><summary>Hint</summary>Maintain the upper and lower hulls separately as `map<x, y>`; inserting a point deletes a contiguous run of neighbours — amortised O(log n).</details>
<details><summary>Approach sketch</summary>For a query, locate the two hull points bracketing `x` in each map and test orientation. For an insertion, if the point is inside do nothing; otherwise insert, then repeatedly erase the left neighbour while it makes a non-convex turn, same to the right. Each point is erased at most once → O(n log n) total. Do the same with y negated for the lower hull.</details>

### 15.25  Mogohu-Rea Idol  ·  Codeforces 87E  ·  ★5 (≈2900)
https://codeforces.com/problemset/problem/87/E
**Technique:** Minkowski sum of three convex polygons + point-in-convex-polygon queries.
<details><summary>Hint</summary>"Is there a triple of points, one from each polygon, whose centroid is q" ⇔ `3q ∈ A ⊕ B ⊕ C`.</details>
<details><summary>Approach sketch</summary>Compute `A ⊕ B ⊕ C` with the angle-merge (sizes up to 5e4 each, sum has ≤ 1.5e5 vertices), then answer 1e5 queries with `inConvex` on `3q`. Coordinates triple — check that cross products still fit (they do at 3·1e6... verify against the statement's bound).</details>

### Practice more

- Codeforces problemset, tag `geometry`, rating 1800–2300 — after the ladder above, do ten of
  these; keep only exact-arithmetic solutions unless the statement forces reals.
- Codeforces problemset, tag `geometry` + `data structures`, rating 2100–2400 for sweep-line
  work.
- AtCoder Beginner Contest problems E–F with "points on a plane" — good `long double`
  discipline practice with tolerant checkers.

---

## Progress

- [ ] 15.1 Point Location Test (CSES 2189)
- [ ] 15.2 Line Segment Intersection (CSES 2190)
- [ ] 15.3 Polygon Area (CSES 2191)
- [ ] 15.4 Point in Polygon (CSES 2192)
- [ ] 15.5 Polygon Lattice Points (CSES 2193)
- [ ] 15.6 Minimum Euclidean Distance (CSES 2194)
- [ ] 15.7 Convex Hull (CSES 2195)
- [ ] 15.8 Maximum Manhattan Distances (CSES 3410)
- [ ] 15.9 All Manhattan Distances (CSES 3411)
- [ ] 15.10 Intersection Points (CSES 1740)
- [ ] 15.11 Area of Rectangles (CSES 1741)
- [ ] 15.12 Robot Path (CSES 1742)
- [ ] 15.13 Line Segments Trace I (CSES 3427)
- [ ] 15.14 Line Segments Trace II (CSES 3428)
- [ ] 15.15 Lines and Queries I (CSES 3429)
- [ ] 15.16 Lines and Queries II (CSES 3430)
- [ ] 15.17 Convex Quadrilateral (ABC 266 C)
- [ ] 15.18 Opposite (ABC 197 D)
- [ ] 15.19 Congruence Points (ABC 207 D)
- [ ] 15.20 Enclose All (ABC 151 F)
- [ ] 15.21 Ancient Berland Circus (CF 1C)
- [ ] 15.22 Polygons (CF 166B)
- [ ] 15.23 Nearest vectors (CF 598C)
- [ ] 15.24 Professor's task (CF 70D)
- [ ] 15.25 Mogohu-Rea Idol (CF 87E)
