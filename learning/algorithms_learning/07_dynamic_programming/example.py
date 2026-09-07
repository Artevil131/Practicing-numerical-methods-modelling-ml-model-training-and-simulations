"""Chapter 07 — Dynamic programming. Python port of example.c: same skeletons, same tiny neutral inputs.
Run: python3 example.py
"""

from functools import lru_cache

INF = float("inf")

# ------------------------------------------------------------------------
# 1. 1D DP: state, transition, base case
# ------------------------------------------------------------------------

def count_ways_1_3(n: int) -> int:
    """Ordered sums of n using parts {1,3}. ways[i] = ways[i-1] + ways[i-3]."""
    w0, w1, w2 = 1, 0, 0                 # dp[i-1], dp[i-2], dp[i-3]
    for _ in range(1, n + 1):
        w0, w1, w2 = w0 + w2, w0, w1
    return w0


@lru_cache(maxsize=None)
def count_ways_1_3_memo(i: int) -> int:
    if i < 0:
        return 0
    if i == 0:
        return 1
    return count_ways_1_3_memo(i - 1) + count_ways_1_3_memo(i - 3)


def longest_increasing_run(a: list[int]) -> int:
    """Longest strictly increasing CONTIGUOUS run. 'ending at i' pattern."""
    if not a:
        return 0
    end_here = best = 1
    for i in range(1, len(a)):
        end_here = end_here + 1 if a[i] > a[i - 1] else 1
        best = max(best, end_here)
    return best


def max_subarray(a: list[int]) -> int:
    end_here = best = a[0]
    for x in a[1:]:
        end_here = max(x, end_here + x)
        best = max(best, end_here)
    return best

# ------------------------------------------------------------------------
# 2. Decision DP: a state machine per step
# ------------------------------------------------------------------------

def min_switch_cost(run: list[int], miss: list[int], sw: int) -> int:
    """on[i]/off[i] = min cost of steps 0..i ending ON/OFF. Switching costs `sw`."""
    on, off = run[0], miss[0]
    for i in range(1, len(run)):
        on, off = (
            run[i] + min(on, off + sw),
            miss[i] + min(off, on + sw),
        )
    return min(on, off)


def max_profit_unlimited(prices: list[int]) -> int:
    cash, hold = 0, -prices[0]
    for p in prices[1:]:
        cash, hold = max(cash, hold + p), max(hold, cash - p)
    return cash

# ------------------------------------------------------------------------
# 3. Knapsack & subset sums
# ------------------------------------------------------------------------

def subset_sum_reachable(weights: list[int], target: int) -> bool:
    reach = [False] * (target + 1)
    reach[0] = True
    for wt in weights:
        for s in range(target, wt - 1, -1):        # DESCENDING: each item once
            reach[s] = reach[s] or reach[s - wt]
    return reach[target]


def coin_change_min(coins: list[int], amount: int) -> int:
    dp = [INF] * (amount + 1)
    dp[0] = 0
    for c in coins:
        for s in range(c, amount + 1):               # ASCENDING: unlimited reuse
            dp[s] = min(dp[s], dp[s - c] + 1)
    return -1 if dp[amount] == INF else dp[amount]


def combinations_count(items: list[int], target: int) -> int:
    """item outer, sum inner -> combinations (order irrelevant)."""
    dp = [0] * (target + 1)
    dp[0] = 1
    for x in items:
        for s in range(x, target + 1):
            dp[s] += dp[s - x]
    return dp[target]


def permutations_count(items: list[int], target: int) -> int:
    """sum outer, item inner -> permutations (order matters)."""
    dp = [0] * (target + 1)
    dp[0] = 1
    for s in range(1, target + 1):
        for x in items:
            if x <= s:
                dp[s] += dp[s - x]
    return dp[target]


def knapsack01_reconstruct(weights: list[int], values: list[int], cap: int) -> tuple[int, list[int]]:
    """Full 2D table so we can reconstruct which items were taken."""
    n = len(weights)
    T = [[0] * (cap + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        wt, val = weights[i - 1], values[i - 1]
        for c in range(cap + 1):
            T[i][c] = T[i - 1][c]
            if wt <= c:
                T[i][c] = max(T[i][c], T[i - 1][c - wt] + val)
    chosen = []
    c = cap
    for i in range(n, 0, -1):
        if T[i][c] != T[i - 1][c]:
            chosen.append(i - 1)
            c -= weights[i - 1]
    chosen.reverse()
    return T[n][cap], chosen

# ------------------------------------------------------------------------
# 4. Grid DP
# ------------------------------------------------------------------------

def min_cost_path(cost: list[list[int]]) -> int:
    """Sentinel border of INF; moves down/right."""
    h, w = len(cost), len(cost[0])
    cols = w + 1
    D = [[INF] * cols for _ in range(h + 1)]
    for r in range(h):
        for c in range(w):
            if r == 0 and c == 0:
                D[1][1] = cost[0][0]
                continue
            m = min(D[r][c + 1], D[r + 1][c])          # up, left
            D[r + 1][c + 1] = m if m >= INF else m + cost[r][c]
    return D[h][w]


def count_monotone_paths(grid: list[list[int]]) -> int:
    """grid[r][c] == 1 is a wall. Single rolling row."""
    h, w = len(grid), len(grid[0])
    row = [0] * w
    for c in range(w):
        row[c] = 0 if grid[0][c] == 1 else 1
    for r in range(1, h):
        for c in range(w):
            if grid[r][c] == 1:
                row[c] = 0
            elif c > 0:
                row[c] += row[c - 1]
    return row[-1]

# ------------------------------------------------------------------------
# 5. String DP
# ------------------------------------------------------------------------

def lcs_length(s: str, t: str) -> int:
    n, m = len(s), len(t)
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            if s[i - 1] == t[j - 1]:
                dp[i][j] = dp[i - 1][j - 1] + 1
            else:
                dp[i][j] = max(dp[i - 1][j], dp[i][j - 1])
    return dp[n][m]


def edit_distance(s: str, t: str) -> int:
    n, m = len(s), len(t)
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n + 1):
        dp[i][0] = i
    for j in range(m + 1):
        dp[0][j] = j
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            if s[i - 1] == t[j - 1]:
                dp[i][j] = dp[i - 1][j - 1]
            else:
                dp[i][j] = 1 + min(dp[i - 1][j - 1], dp[i - 1][j], dp[i][j - 1])
    return dp[n][m]


def palindrome_table(s: str) -> list[list[bool]]:
    n = len(s)
    P = [[False] * n for _ in range(n)]
    for length in range(1, n + 1):
        for i in range(n - length + 1):
            j = i + length - 1
            P[i][j] = s[i] == s[j] and (length < 3 or P[i + 1][j - 1])
    return P

# ------------------------------------------------------------------------
# 6. Interval DP
# ------------------------------------------------------------------------

def matrix_chain_min_mults(dims: list[int]) -> int:
    """dims has n+1 entries; matrix k is dims[k] x dims[k+1]."""
    n = len(dims) - 1                                  # number of matrices
    C = [[0] * n for _ in range(n)]
    for length in range(2, n + 1):
        for i in range(n - length + 1):
            j = i + length - 1
            best = INF
            for k in range(i, j):
                c = C[i][k] + C[k + 1][j] + dims[i] * dims[k + 1] * dims[j + 1]
                best = min(best, c)
            C[i][j] = best
    return C[0][n - 1]


def predict_the_winner_margin(a: list[int]) -> int:
    """D[i][j] = best (mover - opponent) score on a[i..j]. Positive => first player wins."""
    n = len(a)
    D = [[0] * n for _ in range(n)]
    for i in range(n):
        D[i][i] = a[i]
    for length in range(2, n + 1):
        for i in range(n - length + 1):
            j = i + length - 1
            D[i][j] = max(a[i] - D[i + 1][j], a[j] - D[i][j - 1])
    return D[0][n - 1]

# ------------------------------------------------------------------------
# 7. Bitmask DP
# ------------------------------------------------------------------------

def assignment_min_cost(cost: list[list[int]]) -> int:
    """n slots, n items. dp[mask] = min cost filling first popcount(mask) slots
    with exactly the items in mask."""
    n = len(cost)
    full = (1 << n) - 1
    dp = [INF] * (1 << n)
    dp[0] = 0
    for mask in range(full):
        if dp[mask] >= INF:
            continue
        slot = mask.bit_count()
        for i in range(n):
            if mask & (1 << i):
                continue
            nm = mask | (1 << i)
            cand = dp[mask] + cost[slot][i]
            if cand < dp[nm]:
                dp[nm] = cand
    return dp[full]


def submasks(mask: int) -> list[int]:
    """All submasks of `mask`, including mask itself and 0."""
    out = []
    sub = mask
    while True:
        out.append(sub)
        if sub == 0:
            break
        sub = (sub - 1) & mask
    return out

# ------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------

def main() -> None:
    # 1. 1D DP
    assert count_ways_1_3(5) == 4            # 11111, 113, 131, 311 (ordered sums of 1s and 3s)
    for n in range(10):
        assert count_ways_1_3(n) == count_ways_1_3_memo(n)
    assert longest_increasing_run([1, 2, 2, 3, 4, 1, 2]) == 3   # 2,3,4
    assert max_subarray([2, -3, 4, -1, 2, -5, 3]) == 5           # [4,-1,2]

    # 2. decision DP
    assert min_switch_cost([2, 2, 2, 2], [1, 1, 1, 1], sw=5) == 4  # always off is cheapest
    assert max_profit_unlimited([3, 1, 4, 2, 5]) == 6

    # 3. knapsack & subset sums
    assert subset_sum_reachable([3, 4, 5], 9)
    assert subset_sum_reachable([3, 4], 7)              # 3+4
    assert not subset_sum_reachable([3, 4], 8)           # no subset sums to 8
    assert coin_change_min([1, 3, 4], 6) == 2               # 3+3
    assert combinations_count([1, 2], 4) == 3               # 1111,112,22
    assert permutations_count([1, 2], 4) == 5               # 112,121,211,22,1111 orderings

    best_val, chosen = knapsack01_reconstruct([2, 3, 4, 5], [3, 4, 5, 6], 5)
    assert best_val == 7                                     # items 0,1 (weights 2+3=5, values 3+4=7)
    assert chosen == [0, 1]

    # 4. grid DP
    cost = [[1, 3, 1], [1, 5, 1], [4, 2, 1]]
    assert min_cost_path(cost) == 7                          # 1+3+1+1+1
    wall_grid = [[0, 0, 0], [0, 1, 0], [0, 0, 0]]
    assert count_monotone_paths(wall_grid) == 2

    # 5. string DP
    assert lcs_length("ABC", "AC") == 2
    assert edit_distance("horse", "ros") == 3
    pal = palindrome_table("abba")
    assert pal[0][3] is True and pal[0][2] is False and pal[1][2] is True

    # 6. interval DP
    assert matrix_chain_min_mults([10, 20, 30, 40, 30]) == 30000
    assert predict_the_winner_margin([3, 9, 1, 2]) == 7

    # 7. bitmask DP
    assign_cost = [[4, 2, 8], [4, 3, 7], [3, 1, 9]]
    assert assignment_min_cost(assign_cost) == 12
    assert sorted(submasks(0b101)) == [0, 1, 4, 5]

    print("chapter 07 dynamic programming: all asserts passed")


if __name__ == "__main__":
    main()
