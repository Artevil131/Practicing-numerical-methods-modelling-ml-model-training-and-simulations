"""Chapter 01 — Arrays & hashing. Python port of example.c: same skeletons, same inputs.
Run: python3 example.py
"""
from collections import Counter, defaultdict
from itertools import accumulate

# ---------------------------------------------------------------------------
# 1. Hash set: set is the built-in open-addressing table
# ---------------------------------------------------------------------------

def first_duplicate(a: list[int]) -> int | None:
    seen: set[int] = set()
    for x in a:
        if x in seen:
            return x
        seen.add(x)
    return None


def longest_consecutive(a: list[int]) -> int:
    s = set(a)
    best = 0
    for x in s:
        if x - 1 in s:                      # start only from run heads
            continue
        y = x
        while y + 1 in s:
            y += 1
        best = max(best, y - x + 1)
    return best

# ---------------------------------------------------------------------------
# 2. Array as its own hash set (values bounded to 1..n)
# ---------------------------------------------------------------------------

def duplicates_by_sign_marking(a: list[int]) -> list[int]:
    out: list[int] = []
    for i in range(len(a)):
        v = abs(a[i])                       # always read abs()
        if a[v - 1] < 0:
            out.append(v)
        else:
            a[v - 1] = -a[v - 1]
    return out


def first_missing_positive(a: list[int]) -> int:
    n = len(a)
    for i in range(n):
        while 1 <= a[i] <= n and a[a[i] - 1] != a[i]:
            j = a[i] - 1
            a[i], a[j] = a[j], a[i]
    for i in range(n):
        if a[i] != i + 1:
            return i + 1
    return n + 1

# ---------------------------------------------------------------------------
# 3. Frequency counting
# ---------------------------------------------------------------------------

def is_anagram(s: str, t: str) -> bool:
    if len(s) != len(t):
        return False
    cnt = [0] * 26
    for c, d in zip(s, t):
        cnt[ord(c) - 97] += 1
        cnt[ord(d) - 97] -= 1
    return not any(cnt)


def top_k_frequent(a: list[int], k: int) -> list[tuple[int, int]]:
    freq = Counter(a)
    buckets: list[list[int]] = [[] for _ in range(len(a) + 1)]
    for v, f in freq.items():
        buckets[f].append(v)
    out: list[tuple[int, int]] = []
    for f in range(len(a), 0, -1):
        for v in buckets[f]:
            out.append((v, f))
            if len(out) == k:
                return out
    return out


def majority_element(a: list[int]) -> int:
    cand, cnt = a[0], 0
    for x in a:
        if cnt == 0:
            cand = x
        cnt += 1 if x == cand else -1
    return cand

# ---------------------------------------------------------------------------
# 4. Hash map as memory: complement lookup
# ---------------------------------------------------------------------------

def pair_with_sum(a: list[int], target: int) -> tuple[int, int] | None:
    seen: dict[int, int] = {}
    for k, x in enumerate(a):
        if target - x in seen:              # query BEFORE insert
            return seen[target - x], k
        seen[x] = k
    return None


def is_bijection(s: str, t: str) -> bool:
    if len(s) != len(t):
        return False
    st: dict[str, str] = {}
    ts: dict[str, str] = {}
    for c, d in zip(s, t):
        if st.get(c, d) != d or ts.get(d, c) != c:
            return False
        st[c] = d
        ts[d] = c
    return True

# ---------------------------------------------------------------------------
# 5. Prefix sums
# ---------------------------------------------------------------------------

def build_prefix(a: list[int]) -> list[int]:
    return list(accumulate(a, initial=0))   # n+1 entries, P[0] = 0


def range_sum(P: list[int], l: int, r: int) -> int:
    return P[r + 1] - P[l]                  # INCLUSIVE a[l..r]


def count_subarrays_with_sum(a: list[int], k: int) -> int:
    seen: dict[int, int] = defaultdict(int)
    seen[0] = 1
    prefix = ans = 0
    for x in a:
        prefix += x
        ans += seen[prefix - k]
        seen[prefix] += 1
    return ans


def longest_zero_sum_subarray(a: list[int]) -> int:
    first: dict[int, int] = {0: -1}
    prefix = best = 0
    for i, x in enumerate(a):
        prefix += x
        if prefix in first:
            best = max(best, i - first[prefix])
        else:
            first[prefix] = i
    return best

# ---------------------------------------------------------------------------
# 6. Sorting as preprocessing: interval sweep
# ---------------------------------------------------------------------------

def merge_intervals(v: list[list[int]]) -> list[list[int]]:
    v.sort(key=lambda iv: iv[0])
    out: list[list[int]] = []
    for s, e in v:
        if out and s <= out[-1][1]:
            out[-1][1] = max(out[-1][1], e)
        else:
            out.append([s, e])
    return out

# ---------------------------------------------------------------------------
# 7. Matrix index arithmetic
# ---------------------------------------------------------------------------

def transpose(a: list[list[int]]) -> list[list[int]]:
    return [list(col) for col in zip(*a)]


def rotate_cw_inplace(a: list[list[int]]) -> None:
    n = len(a)
    for i in range(n):
        for j in range(i + 1, n):
            a[i][j], a[j][i] = a[j][i], a[i][j]
    for row in a:
        row.reverse()


def spiral_order(a: list[list[int]]) -> list[int]:
    out: list[int] = []
    top, bottom, left, right = 0, len(a) - 1, 0, len(a[0]) - 1
    while top <= bottom and left <= right:
        out.extend(a[top][j] for j in range(left, right + 1)); top += 1
        out.extend(a[i][right] for i in range(top, bottom + 1)); right -= 1
        if top <= bottom:
            out.extend(a[bottom][j] for j in range(right, left - 1, -1)); bottom -= 1
        if left <= right:
            out.extend(a[i][left] for i in range(bottom, top - 1, -1)); left += 1
    return out


def game_of_life_step(g: list[list[int]]) -> None:
    m, n = len(g), len(g[0])
    for i in range(m):
        for j in range(n):
            live = sum(g[i + di][j + dj] & 1
                       for di in (-1, 0, 1) for dj in (-1, 0, 1)
                       if (di or dj) and 0 <= i + di < m and 0 <= j + dj < n)
            alive = g[i][j] & 1
            if (alive and live in (2, 3)) or (not alive and live == 3):
                g[i][j] |= 2
    for row in g:
        for j in range(n):
            row[j] >>= 1

# ---------------------------------------------------------------------------
# main: tiny demos of every skeleton
# ---------------------------------------------------------------------------

def main() -> None:
    assert first_duplicate([3, 1, 4, 1, 5, 9, 2, 6]) == 1
    assert first_duplicate([1, 2, 3]) is None
    assert longest_consecutive([100, 4, 200, 1, 3, 2, -1, 0]) == 6
    s = {-7, -2**31}
    assert -7 in s and -2**31 in s and 7 not in s

    assert duplicates_by_sign_marking([4, 3, 2, 7, 8, 2, 3, 1]) == [2, 3]
    assert first_missing_positive([3, 4, -1, 1]) == 2
    assert first_missing_positive([1, 2, 0]) == 3
    assert first_missing_positive([7, 8, 9, 11, 12]) == 1

    assert is_anagram("listen", "silent") and not is_anagram("rat", "car") and not is_anagram("ab", "abc")
    assert top_k_frequent([1, 1, 1, 2, 2, 3], 2) == [(1, 3), (2, 2)]
    assert majority_element([2, 2, 1, 1, 1, 2, 2]) == 2

    assert pair_with_sum([2, 7, 11, 15, 3], 10) == (1, 4)
    assert pair_with_sum([5, 5], 10) == (0, 1)
    assert is_bijection("egg", "add") and not is_bijection("foo", "bar") and not is_bijection("badc", "baba")

    P = build_prefix([3, -1, 4, 1, -5, 9])
    assert P == [0, 3, 2, 6, 7, 2, 11]
    assert range_sum(P, 1, 3) == 4 and range_sum(P, 0, 5) == 11
    assert count_subarrays_with_sum([1, 1, 1, -1, 2], 2) == 5
    assert longest_zero_sum_subarray([1, -1, 3, -3, 4, 0, -4]) == 7
    assert -7 % 5 == 3                                  # Python: floored, already in [0, k)

    assert merge_intervals([[8, 10], [1, 3], [2, 6], [15, 18], [6, 7]]) == [[1, 7], [8, 10], [15, 18]]

    m = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
    assert transpose([[1, 2, 3], [4, 5, 6]]) == [[1, 4], [2, 5], [3, 6]]
    rotate_cw_inplace(m)
    assert m == [[7, 4, 1], [8, 5, 2], [9, 6, 3]]
    assert spiral_order([[1, 2, 3], [4, 5, 6], [7, 8, 9]]) == [1, 2, 3, 6, 9, 8, 7, 4, 5]
    assert spiral_order([[1, 2, 3]]) == [1, 2, 3] and spiral_order([[1], [2]]) == [1, 2]
    g = [[0, 1, 0], [0, 0, 1], [1, 1, 1], [0, 0, 0]]     # glider
    game_of_life_step(g)
    assert g == [[0, 0, 0], [1, 0, 1], [0, 1, 1], [0, 1, 0]]

    print("chapter 01 arrays & hashing: all asserts passed")


if __name__ == "__main__":
    main()
