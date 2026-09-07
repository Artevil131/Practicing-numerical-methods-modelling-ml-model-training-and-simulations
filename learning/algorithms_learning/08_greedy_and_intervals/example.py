"""Chapter 08 — Greedy & intervals. Python port of example.c: same skeletons, same tiny neutral inputs.
Run: python3 example.py
"""

import heapq
import random
from functools import cmp_to_key

# ------------------------------------------------------------------------
# 1. Sort-then-scan greedy
# ------------------------------------------------------------------------

def greedy_fill(cost: list[int], budget: int) -> int:
    cost = sorted(cost)
    taken = 0
    for c in cost:
        if c > budget:
            break
        budget -= c
        taken += 1
    return taken


def match_smallest_sufficient(demands: list[int], supplies: list[int]) -> int:
    """Each demand gets the smallest supply that is >= it. Returns # matched."""
    demands = sorted(demands)
    supplies = sorted(supplies)
    i = j = matched = 0
    while i < len(demands) and j < len(supplies):
        if supplies[j] >= demands[i]:
            matched += 1
            i += 1
            j += 1
        else:
            j += 1
    return matched

# ------------------------------------------------------------------------
# 2. Exchange-argument greedy
# ------------------------------------------------------------------------

def cmp_job(a: tuple[int, int], b: tuple[int, int]) -> int:
    pa, wa = a
    pb, wb = b
    lhs, rhs = pa * wb, pb * wa
    return (lhs > rhs) - (lhs < rhs)


def min_weighted_completion_time(jobs: list[tuple[int, int]]) -> int:
    order = sorted(jobs, key=cmp_to_key(cmp_job))
    t = 0
    total = 0
    for p, w in order:
        t += p
        total += w * t
    return total


def cmp_concat_desc(a: str, b: str) -> int:
    ab, ba = a + b, b + a
    return (ba > ab) - (ba < ab)


def largest_number(pieces: list[str]) -> str:
    order = sorted(pieces, key=cmp_to_key(cmp_concat_desc))
    result = "".join(order)
    return result.lstrip("0") or "0"

# ------------------------------------------------------------------------
# 3. Intervals
# ------------------------------------------------------------------------

def merge_intervals(intervals: list[list[int]]) -> list[list[int]]:
    if not intervals:
        return []
    intervals = sorted(intervals, key=lambda iv: iv[0])
    merged = [intervals[0][:]]
    for s, e in intervals[1:]:
        if s <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], e)
        else:
            merged.append([s, e])
    return merged


def max_non_overlapping(intervals: list[list[int]]) -> int:
    intervals = sorted(intervals, key=lambda iv: iv[1])
    kept = 0
    last_end = float("-inf")
    for s, e in intervals:
        if s >= last_end:
            kept += 1
            last_end = e
    return kept


def min_arrows(intervals: list[list[int]]) -> int:
    """Min points/arrows so every interval contains at least one. Strict '>' for a new arrow."""
    intervals = sorted(intervals, key=lambda iv: iv[1])
    arrows = 0
    arrow_pos = float("-inf")
    for s, e in intervals:
        if s > arrow_pos:
            arrows += 1
            arrow_pos = e
    return arrows


def intersect_sorted_lists(a: list[list[int]], b: list[list[int]]) -> list[list[int]]:
    """Both lists sorted and internally disjoint. Advance the interval that ends first."""
    i = j = 0
    out = []
    while i < len(a) and j < len(b):
        lo = max(a[i][0], b[j][0])
        hi = min(a[i][1], b[j][1])
        if lo <= hi:
            out.append([lo, hi])
        if a[i][1] < b[j][1]:
            i += 1
        else:
            j += 1
    return out

# ------------------------------------------------------------------------
# 4. Greedy with a heap
# ------------------------------------------------------------------------

def shortest_available_first(tasks: list[tuple[int, int]]) -> list[int]:
    """tasks = [(arrival, duration), ...]. Returns run order by original index."""
    n = len(tasks)
    order_by_arrival = sorted(range(n), key=lambda i: tasks[i][0])
    heap: list[tuple[int, int]] = []                 # (duration, index)
    result: list[int] = []
    t = 0
    ptr = 0
    while len(result) < n:
        if not heap and ptr < n and tasks[order_by_arrival[ptr]][0] > t:
            t = tasks[order_by_arrival[ptr]][0]        # idle: jump to next arrival
        while ptr < n and tasks[order_by_arrival[ptr]][0] <= t:
            idx = order_by_arrival[ptr]
            heapq.heappush(heap, (tasks[idx][1], idx))
            ptr += 1
        dur, idx = heapq.heappop(heap)
        result.append(idx)
        t += dur
    return result

# ------------------------------------------------------------------------
# 5. Reach/jump greedy
# ------------------------------------------------------------------------

def can_reach_end(a: list[int]) -> bool:
    farthest = 0
    for i, x in enumerate(a):
        if i > farthest:
            return False
        farthest = max(farthest, i + x)
    return True


def min_jumps(a: list[int]) -> int:
    n = len(a)
    jumps = 0
    current_end = farthest = 0
    for i in range(n - 1):
        farthest = max(farthest, i + a[i])
        if i == current_end:
            if farthest <= i:
                return -1
            jumps += 1
            current_end = farthest
            if current_end >= n - 1:
                break
    return jumps


def circular_gas_start(gas: list[int], cost: list[int]) -> int:
    """Returns the starting station index, or -1 if no solution."""
    total = 0
    tank = 0
    start = 0
    for i in range(len(gas)):
        d = gas[i] - cost[i]
        total += d
        tank += d
        if tank < 0:
            start = i + 1
            tank = 0
    return start if total >= 0 else -1

# ------------------------------------------------------------------------
# 6. Where greedy fails: side-by-side with DP + brute-force cross-check
# ------------------------------------------------------------------------

def coin_change_greedy(coins: list[int], amount: int) -> int:
    """WRONG in general: largest coin first. Kept for the counterexample demo."""
    coins = sorted(coins, reverse=True)
    count = 0
    for c in coins:
        while amount >= c:
            amount -= c
            count += 1
    return count if amount == 0 else -1


def coin_change_dp(coins: list[int], amount: int) -> int:
    INF = float("inf")
    dp = [INF] * (amount + 1)
    dp[0] = 0
    for c in coins:
        for s in range(c, amount + 1):
            dp[s] = min(dp[s], dp[s - c] + 1)
    return -1 if dp[amount] == INF else dp[amount]


def best_non_adjacent(a: list[int]) -> int:
    take_prev2 = take_prev1 = 0
    for x in a:
        cur = max(take_prev1, take_prev2 + x)
        take_prev2, take_prev1 = take_prev1, cur
    return take_prev1


def knapsack01_ratio_greedy(weights: list[int], values: list[int], cap: int) -> int:
    """WRONG for 0/1 knapsack in general: best value/weight ratio first."""
    n = len(weights)
    order = sorted(range(n), key=lambda i: values[i] / weights[i], reverse=True)
    total = 0
    remaining = cap
    for i in order:
        if weights[i] <= remaining:
            remaining -= weights[i]
            total += values[i]
    return total


def knapsack01_brute(weights: list[int], values: list[int], cap: int) -> int:
    n = len(weights)
    best = 0
    for mask in range(1 << n):
        w = v = 0
        for i in range(n):
            if mask & (1 << i):
                w += weights[i]
                v += values[i]
        if w <= cap:
            best = max(best, v)
    return best


def find_knapsack_greedy_counterexample(trials: int, seed: int) -> tuple[list[int], list[int], int] | None:
    rng = random.Random(seed)
    for _ in range(trials):
        n = rng.randint(1, 6)
        weights = [rng.randint(1, 10) for _ in range(n)]
        values = [rng.randint(1, 10) for _ in range(n)]
        cap = rng.randint(1, 20)
        if knapsack01_ratio_greedy(weights, values, cap) != knapsack01_brute(weights, values, cap):
            return weights, values, cap
    return None

# ------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------

def main() -> None:
    # 1. sort-then-scan
    assert greedy_fill([7, 2, 9, 4, 3], 12) == 3
    assert match_smallest_sufficient([1, 2, 3], [3, 1, 1, 2]) == 3

    # 2. exchange-argument
    jobs = [(3, 1), (1, 2), (2, 2), (4, 4)]              # A B C D
    assert min_weighted_completion_time(jobs) == 46
    assert largest_number(["3", "30", "34", "5", "9"]) == "9534330"

    # 3. intervals
    ivs = [[1, 3], [8, 10], [2, 6], [15, 18], [9, 12]]
    assert merge_intervals(ivs) == [[1, 6], [8, 12], [15, 18]]
    assert max_non_overlapping([[1, 100], [2, 3], [4, 5]]) == 2
    assert min_arrows([[10, 16], [2, 8], [1, 6], [7, 12]]) == 2
    assert intersect_sorted_lists([[0, 2], [5, 10], [13, 23]], [[1, 5], [8, 12], [15, 24]]) == \
        [[1, 2], [5, 5], [8, 10], [15, 23]]

    # 4. heap-driven greedy
    tasks = [(0, 5), (1, 2), (2, 1), (8, 3), (9, 1)]
    assert shortest_available_first(tasks) == [0, 2, 1, 3, 4]

    # 5. reach/jump greedy
    assert can_reach_end([2, 3, 1, 1, 4])
    assert not can_reach_end([3, 2, 1, 0, 4])
    assert min_jumps([1, 3, 0, 0, 4, 1, 0]) == 3
    assert circular_gas_start([1, 2, 3, 4, 5], [3, 4, 5, 1, 2]) == 3

    # 6. where greedy fails
    assert coin_change_greedy([1, 3, 4], 6) == 3          # wrong: 4+1+1
    assert coin_change_dp([1, 3, 4], 6) == 2              # right: 3+3
    assert best_non_adjacent([3, 2, 5, 10, 7]) == 15       # 3+5+7

    counterexample = find_knapsack_greedy_counterexample(trials=2000, seed=42)
    assert counterexample is not None, "expected the ratio-greedy to fail on some tiny instance"
    w, v, cap = counterexample
    assert knapsack01_ratio_greedy(w, v, cap) < knapsack01_brute(w, v, cap)

    print("chapter 08 greedy & intervals: all asserts passed")


if __name__ == "__main__":
    main()
