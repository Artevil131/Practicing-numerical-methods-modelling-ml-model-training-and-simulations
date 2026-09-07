"""Chapter 04 — Stack & monotonic stack. Python port of example.c: same skeletons, same inputs.
Run: python3 example.py
"""

# ------------------------------------------------------------------------
# 1. Bracket matching — the three checks
# ------------------------------------------------------------------------

PAIRS = {")": "(", "]": "[", "}": "{"}


def bracket_valid(s: str) -> bool:
    st: list[str] = []
    for c in s:
        if c in "([{":
            st.append(c)
        elif not st or st[-1] != PAIRS[c]:
            return False
        else:
            st.pop()
    return not st

# ------------------------------------------------------------------------
# 2. Depth counter — a stack whose content is irrelevant
# ------------------------------------------------------------------------

def depth_counter_fixes(s: str) -> int:
    """Single bracket type. Minimum insertions to balance."""
    depth = fixes = 0
    for c in s:
        if c == "(":
            depth += 1
        elif depth == 0:
            fixes += 1                           # unmatched closer
        else:
            depth -= 1
    return fixes + depth                         # leftover openers

# ------------------------------------------------------------------------
# 3. Undo / state-machine stack — cancel while the top collides
# ------------------------------------------------------------------------

def cancel_adjacent(s: str) -> str:
    st: list[str] = []
    for c in s:
        if st and st[-1] == c:
            st.pop()
        else:
            st.append(c)
    return "".join(st)


def asteroid_collision(a: list[int]) -> list[int]:
    st: list[int] = []
    for x in a:
        alive = True
        while alive and st and st[-1] > 0 > x:
            if st[-1] < -x:
                st.pop()
            elif st[-1] == -x:
                st.pop(); alive = False
            else:
                alive = False
        if alive:
            st.append(x)
    return st

# ------------------------------------------------------------------------
# 4. RPN (postfix) evaluation — operand stack
# ------------------------------------------------------------------------

def eval_rpn(tokens: list[str]) -> int:
    st: list[int] = []
    for t in tokens:
        if t in ("+", "-", "*", "/"):
            b, a = st.pop(), st.pop()            # a is the LEFT operand
            if t == "+":
                st.append(a + b)
            elif t == "-":
                st.append(a - b)
            elif t == "*":
                st.append(a * b)
            else:
                st.append(int(a / b))            # truncate toward zero
        else:
            st.append(int(t))
    assert len(st) == 1
    return st[0]

# ------------------------------------------------------------------------
# 5. Infix +/- with brackets — push (result, sign) on '(' and merge on ')'
# ------------------------------------------------------------------------

def eval_infix_pm(s: str) -> int:
    result, sign, num = 0, 1, 0
    st: list[tuple[int, int]] = []
    for c in s + "+":
        if c.isdigit():
            num = num * 10 + int(c)
        elif c in "+-":
            result += sign * num; num = 0
            sign = 1 if c == "+" else -1
        elif c == "(":
            st.append((result, sign)); result, sign = 0, 1
        elif c == ")":
            result += sign * num; num = 0
            outer, outer_sign = st.pop()
            result = outer + outer_sign * result
    return result

# ------------------------------------------------------------------------
# 6. Monotonic stack — next strictly greater element (index or -1)
# ------------------------------------------------------------------------

def next_greater(a: list[int]) -> list[int]:
    ans = [-1] * len(a)
    st: list[int] = []
    for i, x in enumerate(a):
        while st and a[st[-1]] < x:
            ans[st.pop()] = i
        st.append(i)
    return ans


def remove_k_digits(num: str, k: int) -> str:
    st: list[str] = []
    for d in num:
        while k and st and st[-1] > d:
            st.pop(); k -= 1
        st.append(d)
    if k:
        st = st[:len(st) - k]
    return "".join(st).lstrip("0") or "0"

# ------------------------------------------------------------------------
# 7. Monotonic stack — largest rectangle under a histogram
# ------------------------------------------------------------------------

def largest_rect(h: list[int]) -> int:
    st: list[int] = []
    best = 0
    for i, cur in enumerate(h + [0]):            # sentinel bar of height 0
        while st and h[st[-1]] > cur:
            k = st.pop()
            width = i if not st else i - st[-1] - 1
            best = max(best, h[k] * width)
        st.append(i)
    return best

# ------------------------------------------------------------------------
# 8. Monotonic stack — contribution sum: value * left_span * right_span
# ------------------------------------------------------------------------

def sum_subarray_mins(a: list[int], mod: int = 10**9 + 7) -> int:
    """Strict on the left, non-strict on the right."""
    n = len(a)
    left = [0] * n
    right = [0] * n
    st: list[int] = []
    for i, x in enumerate(a):
        while st and a[st[-1]] >= x:
            k = st.pop()
            right[k] = i - k
        left[i] = i - (st[-1] if st else -1)
        st.append(i)
    while st:
        k = st.pop()
        right[k] = n - k
    return sum(x * l * r for x, l, r in zip(a, left, right)) % mod


def sum_subarray_mins_slow(a: list[int]) -> int:
    n = len(a)
    return sum(min(a[i:j + 1]) for i in range(n) for j in range(i, n))

# ------------------------------------------------------------------------
# 9. MinStack — each node carries the minimum so far
# ------------------------------------------------------------------------

class MinStack:
    def __init__(self) -> None:
        self._st: list[tuple[int, int]] = []

    def push(self, v: int) -> None:
        m = min(v, self._st[-1][1]) if self._st else v
        self._st.append((v, m))

    def pop(self) -> None:
        self._st.pop()

    def top(self) -> int:
        return self._st[-1][0]

    def get_min(self) -> int:
        return self._st[-1][1]

# ------------------------------------------------------------------------
# 10. Queue from two stacks — pour only when `out` is empty
# ------------------------------------------------------------------------

class TwoStackQueue:
    def __init__(self) -> None:
        self._in: list[int] = []
        self._out: list[int] = []

    def push(self, x: int) -> None:
        self._in.append(x)

    def _pour(self) -> None:
        if not self._out:
            while self._in:
                self._out.append(self._in.pop())

    def pop(self) -> int:
        self._pour()
        return self._out.pop()

    def peek(self) -> int:
        self._pour()
        return self._out[-1]

    def empty(self) -> bool:
        return not self._in and not self._out

# ------------------------------------------------------------------------
# main
# ------------------------------------------------------------------------

def main() -> None:
    assert bracket_valid("{[()]}") and not bracket_valid("{[()]}(")
    assert not bracket_valid(")(") and not bracket_valid("(") and bracket_valid("")
    assert depth_counter_fixes("())(") == 2 and depth_counter_fixes("(()") == 1

    assert cancel_adjacent("abbaccd") == "d"
    assert asteroid_collision([5, 10, -5]) == [5, 10]
    assert asteroid_collision([8, -8]) == []
    assert asteroid_collision([10, 2, -5]) == [10]

    assert eval_rpn(["2", "1", "+", "3", "*"]) == 9
    assert eval_rpn(["4", "13", "5", "/", "+"]) == 6
    assert eval_rpn(["3", "4", "-"]) == -1
    assert eval_rpn(["-7", "2", "/"]) == -3                  # truncation, not floor

    assert eval_infix_pm("1 - (2 + 3) + 10") == 6
    assert eval_infix_pm("(1+(4+5+2)-3)+(6+8)") == 23
    assert eval_infix_pm("42") == 42

    assert next_greater([2, 1, 2, 4, 3]) == [3, 2, 3, -1, -1]
    assert remove_k_digits("1432219", 3) == "1219"
    assert remove_k_digits("10200", 1) == "200"
    assert remove_k_digits("10", 2) == "0"

    assert largest_rect([2, 1, 5, 6, 2, 3]) == 10
    assert largest_rect([2, 2, 2]) == 6 and largest_rect([]) == 0

    a = [3, 1, 2, 4]
    assert sum_subarray_mins(a) == 17 == sum_subarray_mins_slow(a)
    b = [11, 81, 94, 43, 3]
    assert sum_subarray_mins(b) == 444 == sum_subarray_mins_slow(b)
    assert sum_subarray_mins([2, 2, 2]) == 12 == sum_subarray_mins_slow([2, 2, 2])  # duplicates once

    ms = MinStack()
    for v in (5, 3, 7, 2):
        ms.push(v)
    assert ms.get_min() == 2
    ms.pop(); assert ms.get_min() == 3 and ms.top() == 7
    ms.pop(); assert ms.get_min() == 3

    q = TwoStackQueue()
    q.push(1); q.push(2); q.push(3)
    assert q.pop() == 1
    q.push(4)                                                # interleaved push: order must hold
    assert q.peek() == 2 and q.pop() == 2 and q.pop() == 3 and q.pop() == 4 and q.empty()

    print("chapter 04 stack & monotonic stack: all asserts passed")


if __name__ == "__main__":
    main()
