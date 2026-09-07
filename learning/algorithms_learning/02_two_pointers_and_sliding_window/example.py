"""Chapter 02 — Two pointers & sliding window. Python port of example.c.
Run: python3 example.py
"""
import random
from collections import defaultdict, deque

# ==================================================================
# 1. OPPOSITE ENDS
# ==================================================================

def pair_sum_sorted(a: list[int], target: int) -> tuple[int, int] | None:
    left, right = 0, len(a) - 1
    while left < right:
        s = a[left] + a[right]
        if s == target:
            return left, right
        if s < target:
            left += 1
        else:
            right -= 1
    return None

# ==================================================================
# 2. FAST & SLOW POINTERS
# ==================================================================

class Node:
    __slots__ = ("val", "next")

    def __init__(self, val: int, next: "Node | None" = None) -> None:
        self.val, self.next = val, next


def cycle_start(head: Node | None) -> Node | None:
    slow = fast = head
    while fast and fast.next:
        slow, fast = slow.next, fast.next.next
        if slow is fast:
            p = head
            while p is not slow:
                p, slow = p.next, slow.next
            return p
    return None


def middle(head: Node) -> Node:
    slow = fast = head
    while fast and fast.next:
        slow, fast = slow.next, fast.next.next
    return slow


def find_duplicate(nums: list[int]) -> int:
    """Function graph: successor of i is nums[i]. Values in [1, n], length n+1."""
    slow = fast = 0
    while True:
        slow, fast = nums[slow], nums[nums[fast]]
        if slow == fast:
            break
    p = 0
    while p != slow:
        p, slow = nums[p], nums[slow]
    return p

# ==================================================================
# 3. FIXED-SIZE WINDOW
# ==================================================================

def window_sums(a: list[int], k: int) -> list[int]:
    s = sum(a[:k])
    out = [s]
    for right in range(k, len(a)):
        s += a[right] - a[right - k]
        out.append(s)
    return out


def window_max(a: list[int], k: int) -> list[int]:
    dq: deque[int] = deque()
    out: list[int] = []
    for r, x in enumerate(a):
        while dq and a[dq[-1]] <= x:
            dq.pop()
        dq.append(r)
        if dq[0] <= r - k:
            dq.popleft()
        if r >= k - 1:
            out.append(a[dq[0]])
    return out

# ==================================================================
# 4. VARIABLE WINDOW
# ==================================================================

def longest_sum_at_most(a: list[int], bound: int) -> int:
    left = s = best = 0
    for right, x in enumerate(a):
        s += x
        while s > bound:
            s -= a[left]; left += 1
        best = max(best, right - left + 1)
    return best


def shortest_sum_at_least(a: list[int], target: int) -> int:
    left = s = 0
    best = len(a) + 1
    for right, x in enumerate(a):
        s += x
        while s >= target:
            best = min(best, right - left + 1)
            s -= a[left]; left += 1
    return 0 if best == len(a) + 1 else best


def longest_with_k_zeros(a: list[int], k: int) -> int:
    left = zeros = best = 0
    for right, x in enumerate(a):
        zeros += x == 0
        while zeros > k:
            zeros -= a[left] == 0
            left += 1
        best = max(best, right - left + 1)
    return best

# ==================================================================
# 5. WINDOW WITH A FREQUENCY TABLE
# ==================================================================

def longest_k_distinct(s: str, k: int) -> int:
    have: dict[str, int] = defaultdict(int)
    distinct = left = best = 0
    for right, c in enumerate(s):
        if have[c] == 0:
            distinct += 1
        have[c] += 1
        while distinct > k:
            d = s[left]; left += 1
            have[d] -= 1
            if have[d] == 0:
                distinct -= 1
        best = max(best, right - left + 1)
    return best


def count_at_most_k_odd(a: list[int], k: int) -> int:
    if k < 0:
        return 0
    left = odd = count = 0
    for right, x in enumerate(a):
        odd += x & 1
        while odd > k:
            odd -= a[left] & 1
            left += 1
        count += right - left + 1                # every subarray ending at right is valid
    return count


def count_exactly_k_odd(a: list[int], k: int) -> int:
    return count_at_most_k_odd(a, k) - count_at_most_k_odd(a, k - 1)

# ==================================================================
# 6. PARTITIONING IN PLACE
# ==================================================================

def move_zeroes(a: list[int]) -> None:
    write = 0
    for x in a:
        if x != 0:
            a[write] = x; write += 1
    a[write:] = [0] * (len(a) - write)


def sort_colors(a: list[int]) -> None:
    low, mid, high = 0, 0, len(a) - 1
    while mid <= high:
        if a[mid] == 0:
            a[low], a[mid] = a[mid], a[low]; low += 1; mid += 1
        elif a[mid] == 2:
            a[mid], a[high] = a[high], a[mid]; high -= 1
        else:
            mid += 1


def quickselect(a: list[int], k: int) -> int:
    """k-th smallest (0-based rank). Iterative -> no recursion depth issue. Mutates a."""
    lo, hi = 0, len(a) - 1
    while True:
        p = random.randint(lo, hi)
        a[p], a[hi] = a[hi], a[p]
        store = lo
        for i in range(lo, hi):
            if a[i] < a[hi]:
                a[store], a[i] = a[i], a[store]; store += 1
        a[store], a[hi] = a[hi], a[store]
        if store == k:
            return a[store]
        if k < store:
            hi = store - 1
        else:
            lo = store + 1

# ==================================================================
# main
# ==================================================================

def main() -> None:
    random.seed(1)
    assert pair_sum_sorted([1, 3, 4, 6, 8, 11], 10) == (2, 3)
    assert pair_sum_sorted([1, 2], 10) is None

    nodes = [Node(i) for i in range(1, 6)]
    for u, v in zip(nodes, nodes[1:]):
        u.next = v
    nodes[4].next = nodes[2]                                # 1->2->3->4->5->(3)
    assert cycle_start(nodes[0]) is nodes[2]
    nodes[4].next = None
    assert cycle_start(nodes[0]) is None
    assert middle(nodes[0]).val == 3
    assert find_duplicate([3, 1, 3, 4, 2]) == 3

    a = [2, 1, 5, 1, 3, 2]
    assert window_sums(a, 3) == [8, 7, 9, 6]
    assert max(window_sums(a, 3)) / 3 == 3.0               # divide ONCE at the end
    assert window_max(a, 3) == [5, 5, 5, 3]

    assert longest_sum_at_most([2, 1, 5, 1, 3, 2], 6) == 3
    assert shortest_sum_at_least([2, 3, 1, 2, 4, 3], 7) == 2
    assert shortest_sum_at_least([1, 1], 7) == 0
    assert longest_with_k_zeros([1, 1, 0, 1, 0, 1, 1], 1) == 4

    assert longest_k_distinct("eceba", 2) == 3
    assert count_exactly_k_odd([1, 1, 2, 1, 1], 3) == 2
    assert count_exactly_k_odd([2, 4, 6], 1) == 0

    z = [0, 1, 0, 3, 12]
    move_zeroes(z)
    assert z == [1, 3, 12, 0, 0]
    c = [2, 0, 1, 2, 0]
    sort_colors(c)
    assert c == [0, 0, 1, 2, 2]
    q = [7, 2, 9, 4, 1, 8, 3]
    assert quickselect(q[:], 3) == 4                        # sorted: 1 2 3 4 7 8 9
    assert quickselect(q[:], len(q) - 2) == 8               # 2nd largest = rank n-2

    print("chapter 02 two pointers & sliding window: all asserts passed")


if __name__ == "__main__":
    main()
