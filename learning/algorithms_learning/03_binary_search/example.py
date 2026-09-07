"""Chapter 03 — Binary search. Python port of example.c: same skeletons, same inputs.
Run: python3 example.py
"""
import math
from bisect import bisect_left, bisect_right
from collections import deque
from itertools import accumulate

# ------------------------------------------------------------------------
# 1. Closed-interval exact-match search
# ------------------------------------------------------------------------

def search_closed(a: list[int], t: int) -> int:
    """Index of t in ascending a, or -1. Invariant: if t exists it is in [lo, hi]."""
    lo, hi = 0, len(a) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if a[mid] == t:
            return mid
        if a[mid] < t:
            lo = mid + 1
        else:
            hi = mid - 1
    return -1

# ------------------------------------------------------------------------
# 2. Half-open boundary searches
# ------------------------------------------------------------------------

def lower_bound(a: list[int], t: int) -> int:
    """First i with a[i] >= t, or n. Invariant: a[:lo] < t, a[hi:] >= t."""
    lo, hi = 0, len(a)
    while lo < hi:
        mid = (lo + hi) // 2
        if a[mid] >= t:
            hi = mid
        else:
            lo = mid + 1
    return lo


def upper_bound(a: list[int], t: int) -> int:
    """First i with a[i] > t, or n. Same skeleton, one comparison changed."""
    lo, hi = 0, len(a)
    while lo < hi:
        mid = (lo + hi) // 2
        if a[mid] > t:
            hi = mid
        else:
            lo = mid + 1
    return lo

# ------------------------------------------------------------------------
# 3. Boundary search with a non-trivial predicate
# ------------------------------------------------------------------------

def find_peak(a: list[int]) -> int:
    """Index of some peak (edges count as -inf). P(mid) = a[mid] > a[mid+1], on [0, n-1)."""
    lo, hi = 0, len(a) - 1
    while lo < hi:
        mid = (lo + hi) // 2
        if a[mid] > a[mid + 1]:
            hi = mid
        else:
            lo = mid + 1
    return lo


def weighted_pick(prefix: list[int], r: int) -> int:
    """prefix[i] = w[0] + ... + w[i]; r in [1, total]. First i with prefix[i] >= r."""
    return bisect_left(prefix, r)

# ------------------------------------------------------------------------
# 4. Search on a computed value
# ------------------------------------------------------------------------

def isqrt_bs(x: int) -> int:
    """Largest r with r*r <= x. Closed interval on VALUES; hi is the answer at exit."""
    lo, hi = 0, x
    while lo <= hi:
        mid = (lo + hi) // 2
        if mid * mid <= x:                       # no overflow in Python
            lo = mid + 1
        else:
            hi = mid - 1
    return hi

# ------------------------------------------------------------------------
# 5. Rotated sorted array
# ------------------------------------------------------------------------

def rotated_min_index(a: list[int]) -> int:
    lo, hi = 0, len(a) - 1
    while lo < hi:
        mid = (lo + hi) // 2
        if a[mid] > a[hi]:
            lo = mid + 1
        else:
            hi = mid
    return lo


def rotated_search(a: list[int], t: int) -> int:
    lo, hi = 0, len(a) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if a[mid] == t:
            return mid
        if a[lo] <= a[mid]:                      # left half sorted
            if a[lo] <= t < a[mid]:
                hi = mid - 1
            else:
                lo = mid + 1
        else:                                    # right half sorted
            if a[mid] < t <= a[hi]:
                lo = mid + 1
            else:
                hi = mid - 1
    return -1


def rotated_contains_dup(a: list[int], t: int) -> bool:
    """With duplicates: when a[lo] == a[mid] == a[hi] nothing can be inferred -> shrink both."""
    lo, hi = 0, len(a) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        if a[mid] == t:
            return True
        if a[lo] == a[mid] == a[hi]:
            lo += 1; hi -= 1
        elif a[lo] <= a[mid]:
            if a[lo] <= t < a[mid]:
                hi = mid - 1
            else:
                lo = mid + 1
        else:
            if a[mid] < t <= a[hi]:
                lo = mid + 1
            else:
                hi = mid - 1
    return False

# ------------------------------------------------------------------------
# 6. Binary search on the answer
# ------------------------------------------------------------------------

def fits_in_parts(a: list[int], cap: int, parts: int) -> bool:
    used, cur = 1, 0
    for x in a:
        if cur + x > cap:
            used += 1; cur = 0
        cur += x
    return used <= parts


def min_max_chunk_sum(a: list[int], parts: int) -> int:
    """MINIMISE THE MAXIMUM: smallest cap such that fits_in_parts is true."""
    lo, hi = max(a), sum(a)
    while lo < hi:
        mid = (lo + hi) // 2
        if fits_in_parts(a, mid, parts):
            hi = mid
        else:
            lo = mid + 1
    return lo


def min_max_chunk_sum_slow(a: list[int], parts: int) -> int:
    return next(cap for cap in range(max(a), sum(a) + 1) if fits_in_parts(a, cap, parts))


def can_place(pos: list[int], gap: int, balls: int) -> bool:
    """pos sorted ascending: can we place `balls` with pairwise distance >= gap? Greedy."""
    placed, last = 1, pos[0]
    for p in pos[1:]:
        if p - last >= gap:
            placed += 1; last = p
    return placed >= balls


def max_min_gap(pos: list[int], balls: int) -> int:
    """MAXIMISE THE MINIMUM: largest gap such that can_place is true. mid rounds UP."""
    pos = sorted(pos)
    lo, hi = 1, pos[-1] - pos[0]
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if can_place(pos, mid, balls):
            lo = mid
        else:
            hi = mid - 1
    return lo

# ------------------------------------------------------------------------
# 7. 2D matrix, rows and columns each ascending
# ------------------------------------------------------------------------

def staircase_find(a: list[list[int]], t: int) -> bool:
    m, n = len(a), len(a[0])
    r, c = 0, n - 1
    while r < m and c >= 0:
        v = a[r][c]
        if v == t:
            return True
        if v > t:
            c -= 1
        else:
            r += 1
    return False


def count_le(a: list[list[int]], x: int) -> int:
    m, n = len(a), len(a[0])
    r, c, cnt = m - 1, 0, 0
    while r >= 0 and c < n:
        if a[r][c] <= x:
            cnt += r + 1; c += 1
        else:
            r -= 1
    return cnt


def kth_smallest(a: list[list[int]], k: int) -> int:
    lo, hi = a[0][0], a[-1][-1]
    while lo < hi:
        mid = (lo + hi) // 2
        if count_le(a, mid) >= k:
            hi = mid
        else:
            lo = mid + 1
    return lo

# ------------------------------------------------------------------------
# 8. Threshold + BFS feasibility
# ------------------------------------------------------------------------

def reachable_under(h: list[list[int]], t: int) -> bool:
    m, n = len(h), len(h[0])
    if h[0][0] > t:
        return False
    seen = [[False] * n for _ in range(m)]
    seen[0][0] = True
    q: deque[tuple[int, int]] = deque([(0, 0)])
    while q:
        r, c = q.popleft()
        if (r, c) == (m - 1, n - 1):
            return True
        for nr, nc in ((r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)):
            if 0 <= nr < m and 0 <= nc < n and not seen[nr][nc] and h[nr][nc] <= t:
                seen[nr][nc] = True
                q.append((nr, nc))
    return False


def min_level(h: list[list[int]]) -> int:
    lo, hi = 0, max(map(max, h))
    while lo < hi:
        mid = (lo + hi) // 2
        if reachable_under(h, mid):
            hi = mid
        else:
            lo = mid + 1
    return lo

# ------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------

def main() -> None:
    a = [1, 3, 4, 7, 9, 12, 15]
    assert search_closed(a, 9) == 4 and search_closed(a, 8) == -1 and search_closed([], 1) == -1

    b = [2, 4, 4, 4, 7, 9]
    assert lower_bound(b, 4) == 1 == bisect_left(b, 4)
    assert upper_bound(b, 4) == 4 == bisect_right(b, 4)
    for t in range(0, 11):                       # brute-force cross-check
        assert lower_bound(b, t) == bisect_left(b, t) == sum(x < t for x in b)
        assert upper_bound(b, t) == bisect_right(b, t) == sum(x <= t for x in b)
    assert lower_bound(b, 100) == len(b)

    assert find_peak([1, 2, 1, 3, 5, 6, 4]) in (1, 5)
    prefix = list(accumulate([1, 3, 2]))         # [1, 4, 6]
    assert [weighted_pick(prefix, r) for r in range(1, 7)] == [0, 1, 1, 1, 2, 2]

    for x in (0, 1, 15, 16, 17, 10**18, 2**62 - 1):
        assert isqrt_bs(x) == math.isqrt(x)

    rot = [4, 5, 6, 7, 0, 1, 2]
    assert rotated_min_index(rot) == 4
    assert rotated_search(rot, 1) == 5 and rotated_search(rot, 3) == -1
    assert rotated_contains_dup([2, 5, 6, 0, 0, 1, 2], 0)
    assert not rotated_contains_dup([2, 5, 6, 0, 0, 1, 2], 3)
    assert rotated_contains_dup([1, 1, 1, 1, 1], 1) and not rotated_contains_dup([1, 1, 1, 1, 1], 2)

    c = [7, 2, 5, 10, 8]
    assert min_max_chunk_sum(c, 2) == 18 == min_max_chunk_sum_slow(c, 2)
    assert min_max_chunk_sum(c, 3) == 14 == min_max_chunk_sum_slow(c, 3)
    assert max_min_gap([1, 2, 3, 4, 7], 3) == 3
    assert max_min_gap([5, 4, 3, 2, 1, 1000000000], 2) == 999999999

    mat = [[1, 4, 7, 11], [2, 5, 8, 12], [3, 6, 9, 16], [10, 13, 14, 17]]
    assert staircase_find(mat, 9) and not staircase_find(mat, 15)
    assert count_le(mat, 8) == 8
    assert kth_smallest(mat, 8) == 8 and kth_smallest(mat, 1) == 1 and kth_smallest(mat, 16) == 17
    assert kth_smallest(mat, 8) == sorted(v for row in mat for v in row)[7]

    grid = [[0, 2, 6], [5, 3, 7], [8, 4, 1]]
    assert not reachable_under(grid, 3) and reachable_under(grid, 4)
    assert min_level(grid) == 4

    print("chapter 03 binary search: all asserts passed")


if __name__ == "__main__":
    main()
