# Chapter 04 — Problems

How to work these:

1. Read the matching unit section in `lesson.md` first. Write the invariant of the stack in one sentence at the top of your file before typing any code.
2. Attempt the problem in C in `algorithms_learning/04_stack_and_monotonic_stack/solutions/<slug>.c` (create the `solutions/` folder yourself). Use the LeetCode function signature where sensible, and write your own tests in `main()` — the examples from the problem statement plus at least one edge case (empty input, all equal values, single element, deeply nested).
3. Compile with `cc -Wall -Wextra -std=c11 -O2 -o sol <slug>.c` and fix every warning.
4. Only when stuck for 15 minutes, open **Hint**. Only when stuck for 30 minutes (or after solving), open **Key idea & complexity**, then **Approach**.
5. Tick the box in `## Progress` when your tests pass and you can restate the invariant without looking.

Levels: 1 trivial, 2 pattern practice, 3 solid medium, 4 hard, 5 contest.

---

## Unit 1 — Bracket matching & simple stack

### 04.1.1  Valid Parentheses  ·  LC #20  ·  Easy  ·  string, stack, bracket-sequences
<https://leetcode.com/problems/valid-parentheses/>
**Level:** 2
<details><summary>Hint</summary>
Push opening brackets onto a stack, and compare every closing bracket against the top of the stack.
</details>
<details><summary>Key idea & complexity</summary>
Stack matching each closer to the top opener, O(n) time, O(n) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Scan the string left to right. An opening bracket is pushed onto the stack as is. A closing bracket needs two checks: the stack must not be empty, and the opening bracket on top of the stack must be exactly the matching type (easiest to implement with a map closer → opener; in C a small `switch` or a lookup array indexed by the character). If either check fails, the string is invalid immediately. At the end the stack must be empty — otherwise there were openers left without a matching closer. Both conditions are necessary: checking only during the scan would wrongly accept e.g. `"("` on its own, and checking only end-emptiness would wrongly accept `")("` where the order is wrong.
</details>

### 04.1.2  Remove Outermost Parentheses  ·  LC #1021  ·  Easy  ·  string, stack, bracket-sequences
<https://leetcode.com/problems/remove-outermost-parentheses/>
**Level:** 2
<details><summary>Hint</summary>
Count depth with an integer instead of a stack: a primitive piece starts and ends when the depth returns to zero.
</details>
<details><summary>Key idea & complexity</summary>
Depth counter acting as an implicit stack size, O(n) time, O(1) extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because the brackets are guaranteed valid, the full stack content need not be stored — the stack's **size** (depth) alone decides whether the current character belongs to the result. Increment the depth on an opening bracket before the check, but append the character to the result only if the depth was greater than 1 before the increment (i.e. this is not the outermost opener of a primitive piece); decrement the depth on a closing bracket afterwards, and append the character only if the depth is greater than 0 after the decrement. This is a good example of a stack being replaceable by a single integer when the only meaning of the stored data is its *amount*, not its content.
</details>

### 04.1.3  Make The String Great  ·  LC #1544  ·  Easy  ·  string, stack
<https://leetcode.com/problems/make-the-string-great/>
**Level:** 2
<details><summary>Hint</summary>
Push letters onto a stack, but if the new letter is the same as the top in the opposite case, they cancel each other.
</details>
<details><summary>Key idea & complexity</summary>
Stack cancelling adjacent case-mismatched same-letter pairs, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Scan the string and keep a stack of the result built so far. For each new letter: if the stack is non-empty and the top is the same letter in the opposite case (e.g. `a` and `A`), pop the top instead of pushing the new one — this simulates removing the "bad pair". Otherwise push the letter normally. Because removed pairs can expose a new bad pair (letters that were not originally adjacent), the stack is essential: without it you would have to rerun a linear pass repeatedly. The result is the stack content from bottom to top. In C, `c ^ 32` flips ASCII case, so "same letter, opposite case" is `top == (c ^ 32)`.
</details>

### 04.1.4  Baseball Game  ·  LC #682  ·  Easy  ·  array, stack, simulation
<https://leetcode.com/problems/baseball-game/>
**Level:** 2
<details><summary>Hint</summary>
Use a stack as the score history: `C` removes the latest, `D` and `+` read the top of the stack without removing.
</details>
<details><summary>Key idea & complexity</summary>
Stack simulating score history for undo/double/sum operations, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Every operation refers only to the most recently recorded scores, which is LIFO behaviour directly. A number is pushed as is. `"C"` pops the top (undoes the latest record). `"D"` pushes double the top value without removing the original. `"+"` pushes the sum of the two top values, keeping both originals. Finally the answer is the sum of everything on the stack. The problem is a good first step towards the next unit, where the stack acts specifically as an undo/state history rather than as bracket matching.
</details>

### 04.1.5  Minimum Add to Make Parentheses Valid  ·  LC #921  ·  Medium  ·  string, stack, greedy, bracket-sequences
<https://leetcode.com/problems/minimum-add-to-make-parentheses-valid/>
**Level:** 3
<details><summary>Hint</summary>
Count the number of open brackets with an integer; if it would go negative, that is one required insertion right there.
</details>
<details><summary>Key idea & complexity</summary>
Greedy counters for unmatched opens/closes, equivalent to stack depth, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Keep a counter `open` for the number of currently open brackets (unmatched `"("`) instead of a stack depth. An opening bracket increments the counter. A closing bracket tries to decrement it, but if the counter is already zero, this closer is itself unmatched — increment the answer counter by one instead. At the end the remaining `open` counter tells how many openers were left without a partner, and it is added directly to the answer. Because the only relevant information is the *number* of open brackets, not their exact content (they are all the same type), a full stack is unnecessary — an integer suffices, exactly as in Remove Outermost Parentheses.
</details>

### 04.1.6  Minimum Remove to Make Valid Parentheses  ·  LC #1249  ·  Medium  ·  string, stack
<https://leetcode.com/problems/minimum-remove-to-make-valid-parentheses/>
**Level:** 3
<details><summary>Hint</summary>
Store the indices of unmatched openers on the stack; mark unmatched closers for removal immediately.
</details>
<details><summary>Key idea & complexity</summary>
Stack of indices for unmatched openers, plus a marked-removal pass, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Scan the string and push the **index** (not the character) of every opening bracket. When a closing bracket is met: if the stack is non-empty, pop the top (it found its partner); if the stack is empty, this closer is unmatched and is marked for removal immediately. After the scan, every index still on the stack is an unmatched opener and is removed as well. Build the result by skipping all marked indices in the original string. Characters other than brackets are carried along untouched — the stack handles only bracket indices. In C a `bool removed[n]` (or reusing a `char` buffer) is the mark array.
</details>

### 04.1.7  Check if a Parentheses String Can Be Valid  ·  LC #2116  ·  Medium  ·  string, stack, greedy, bracket-sequences
<https://leetcode.com/problems/check-if-a-parentheses-string-can-be-valid/>
**Level:** 3
<details><summary>Hint</summary>
If the length is odd, the answer is no immediately; otherwise run two greedy counter sweeps, from the left and from the right, taking the locked flags into account.
</details>
<details><summary>Key idea & complexity</summary>
Two greedy passes bounding the range of achievable open-count, O(n) time, O(1) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
If the string length is odd, a valid balance can never arise. Otherwise run two greedy sweeps with a counter instead of a stack: left to right, the counter increases for every `"("` or every unlocked character (an unlocked character can be treated as an opener if needed) and decreases for every locked `")"` — if the counter would go negative, the answer is no. A symmetric right-to-left check ensures there cannot be too many closers left over. This extends the same idea as Minimum Add to Make Parentheses Valid: an integer counter suffices to represent the stack depth as long as the predicate is formulated correctly in both directions.
</details>

### 04.1.8  Score of Parentheses  ·  LC #856  ·  Medium  ·  string, stack, bracket-sequences
<https://leetcode.com/problems/score-of-parentheses/>
**Level:** 3
<details><summary>Hint</summary>
The stack accumulates the partial score of each depth level; a closing bracket merges the level into the result as `max(2*inner, 1)`.
</details>
<details><summary>Key idea & complexity</summary>
Stack accumulating nested scores, O(n) time, O(n) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The stack holds integers, each representing the accumulated score of a currently unclosed level. An opening bracket pushes a zero as a new level. A closing bracket pops the top (`inner`, the sum of the inner level) and adds `max(2*inner, 1)` to the current (outer) level's sum — factor two if there was something inside, otherwise the base score of one for the empty pair `"()"`. At the end the only remaining element on the stack is the score of the whole expression. This is a direct generalisation of score computation in which the stack carries partial intermediate results instead of mere indices — a good bridge to the expression-evaluation unit.
</details>

### 04.1.9  Longest Valid Parentheses  ·  LC #32  ·  Hard  ·  string, dynamic-programming, stack, bracket-sequences
<https://leetcode.com/problems/longest-valid-parentheses/>
**Level:** 4
<details><summary>Hint</summary>
Always keep at the bottom of the stack the index of the latest unmatched closer (or of the start); the length is measured from it to the current index.
</details>
<details><summary>Key idea & complexity</summary>
Stack of indices as a base-marker, tracking the longest span from the last unmatched index, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Initialise the stack with the index `-1` (an imaginary "boundary" before the start of the string). An opening bracket pushes its own index. A closing bracket pops the top first; if the stack is then empty, this closer is itself the new boundary and its index is pushed as the new bottom; otherwise the length of the valid span so far is `current index - new top of stack`, and the answer is updated with the maximum. The invariant is that the bottom of the stack is always the nearest index from which everything to the right is (possibly) valid — it is *not* "an open bracket waiting for a partner" as in the other problems, but a boundary marker. This distinction is the most common source of confusion in this problem. In C, because indices are stored with `-1`, use a signed type (`int` or `long`) for the stack.
</details>

---

## Unit 2 — Stack as an undo/state machine

### 04.2.1  Backspace String Compare  ·  LC #844  ·  Easy  ·  two-pointers, string, stack, simulation
<https://leetcode.com/problems/backspace-string-compare/>
**Level:** 2
<details><summary>Hint</summary>
Build a stack from each string: a normal letter is pushed, `#` pops the top.
</details>
<details><summary>Key idea & complexity</summary>
Stack simulating the backspace key, then compare results, O(n) time, O(n) space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Scan both strings separately and build a stack for each: a normal letter is pushed, `'#'` pops the top if the stack is non-empty (a `'#'` on an empty stack does nothing). The contents of the two resulting stacks are the final displayed strings, so compare them. An alternative O(1)-extra-space solution walks both strings right to left with a pair of skip counters and no explicit stack, but the stack solution is the most direct way to see that the "backspace key" is exactly the same LIFO undo model as bracket matching.
</details>

### 04.2.2  Remove All Adjacent Duplicates In String  ·  LC #1047  ·  Easy  ·  string, stack
<https://leetcode.com/problems/remove-all-adjacent-duplicates-in-string/>
**Level:** 2
<details><summary>Hint</summary>
Push letters onto a stack; if the new letter equals the top, pop the top instead.
</details>
<details><summary>Key idea & complexity</summary>
Stack cancelling equal adjacent letters, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Scan the string and keep a stack of the result being built. If the new letter equals the top, they form an adjacent pair and cancel each other — pop the top and do not push the new letter. Otherwise push normally. Because a removal can expose a new pair (letters that were not originally adjacent), a single linear pass without a stack is not enough — the stack makes the cancellation chain automatically in one pass. The result is read from the stack starting at the bottom.
</details>

### 04.2.3  Crawler Log Folder  ·  LC #1598  ·  Easy  ·  array, string, stack
<https://leetcode.com/problems/crawler-log-folder/>
**Level:** 2
<details><summary>Hint</summary>
Count directory depth with an integer: `../` decrements (but never below zero), `./` does nothing, anything else increments.
</details>
<details><summary>Key idea & complexity</summary>
Depth counter as an implicit path stack, O(n) time, O(1) extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because only the final depth is asked for, not the full path, the whole stack (directory names) need not be stored — a counter acting as the stack size suffices. `"../"` decrements the counter by one, but never below zero (you cannot go up from the root). `"./"` changes nothing. Any other operation increments the counter by one (entering a subdirectory). The answer is the counter's final value. Once again this is an example of a stack compressing to an integer when the only information asked for is the depth, not the stack's individual content.
</details>

### 04.2.4  Simplify Path  ·  LC #71  ·  Medium  ·  string, stack
<https://leetcode.com/problems/simplify-path/>
**Level:** 3
<details><summary>Hint</summary>
Split the path on `/` and handle each component: `..` pops the top, `.` and empty are skipped, anything else is pushed.
</details>
<details><summary>Key idea & complexity</summary>
Stack of path components processed by split-and-rules, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Split the path into components on the slash. Process the components in order: an empty component (consecutive slashes) and `"."` are skipped entirely, `".."` pops the top directory if the stack is non-empty (you cannot go up from the root), and any other name is pushed as is. Finally the stack content is joined back with slashes as separators, always starting with the root `"/"`. Unlike Crawler Log Folder, here the final path is asked for rather than just the depth, so the full stack (directory names) is needed and an integer counter is not enough. In C, store components as `(pointer, length)` pairs into the original string rather than copying — or copy with `malloc` and free them all at the end.
</details>

### 04.2.5  Validate Stack Sequences  ·  LC #946  ·  Medium  ·  array, stack, simulation
<https://leetcode.com/problems/validate-stack-sequences/>
**Level:** 3
<details><summary>Hint</summary>
Simulate the stack by pushing in `pushed` order; whenever the top matches the next `popped` value, pop it immediately.
</details>
<details><summary>Key idea & complexity</summary>
Simulate with an explicit stack against the push order, greedily popping when possible, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Simulate a real stack: walk the `pushed` sequence and push each value. After every push, greedily check (in a loop) whether the top matches the next expected value in `popped` — if it does, pop it and advance to the next expected value; repeat as long as matches continue. The greed is correct because if the top matches, popping it now is always at least as good as popping it later (later you would have to pop it anyway before the elements beneath it). At the end the sequence is valid if and only if the whole stack emptied during the check.
</details>

### 04.2.6  Asteroid Collision  ·  LC #735  ·  Medium  ·  array, stack, simulation
<https://leetcode.com/problems/asteroid-collision/>
**Level:** 3
<details><summary>Hint</summary>
Push right-moving asteroids onto the stack; a left-moving one can destroy (or be destroyed by) the top elements in a chain.
</details>
<details><summary>Key idea & complexity</summary>
Stack resolving right-moving vs left-moving collisions, O(n) time amortised.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Scan the asteroids left to right and keep a stack of survivors. A positive (right-moving) asteroid is always pushed directly, because a collision can only happen later with a left-moving one. A negative (left-moving) asteroid collides with the top elements as long as they are positive and smaller: the smaller one is destroyed (popped) and the loop continues; equal size destroys both (pop the top, do not push the new one); a larger top destroys the incoming asteroid (stop, push nothing). If the stack empties or the top is negative, the incoming asteroid survives and is pushed. This is a state-machine problem in the sense that each new element can cancel (destroy) several earlier ones in a chain, exactly like Remove All Adjacent Duplicates.
</details>

### 04.2.7  Exclusive Time of Functions  ·  LC #636  ·  Medium  ·  array, stack
<https://leetcode.com/problems/exclusive-time-of-functions/>
**Level:** 3
<details><summary>Hint</summary>
Push function ids onto the stack; start/end events charge time to the current top and adjust the offset of the next start.
</details>
<details><summary>Key idea & complexity</summary>
Call stack tracking active function and elapsed time deltas, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The stack models the program's call stack directly. `start` event: if the stack is non-empty, add the time elapsed from the current `prevTime` to the current timestamp to the account of the function on top (it was active until then), then push the new function id. `end` event: add `timestamp - prevTime + 1` to the account of the function on top, pop it, and set `prevTime = timestamp + 1` (the next function resumes immediately after). The invariant is that the top of the stack is always the function currently executing code — nested calls interrupt the outer function's time accounting exactly the way unclosed brackets do in Score of Parentheses. Parsing the log lines in C: `sscanf(line, "%d:%5[a-z]:%d", &id, word, &t)` or manual `strchr` on the colons.
</details>

---

## Unit 3 — Expression evaluation

### 04.3.1  Evaluate Reverse Polish Notation  ·  LC #150  ·  Medium  ·  array, math, stack
<https://leetcode.com/problems/evaluate-reverse-polish-notation/>
**Level:** 3
<details><summary>Hint</summary>
Push numbers onto a stack; each operator pops the top two and pushes the result back.
</details>
<details><summary>Key idea & complexity</summary>
Operand stack consumed two-at-a-time by each operator, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
In RPN (postfix) an operator always follows its operands, so a stack resolves the order directly without precedence rules or brackets: go token by token, push a number as is, and when an operator is met, pop the two top elements (mind the correct order: the second from the top is the *left* operand), compute the result and push it back. At the end the stack holds exactly one value, the result of the whole expression. This is the simplest problem of the unit precisely because postfix notation removes the need to handle nesting or operator precedence separately — the stack handles both automatically. In C, distinguish the token `"-3"` from the operator `"-"` by checking the token length or the second character.
</details>

### 04.3.2  Basic Calculator II  ·  LC #227  ·  Medium  ·  math, string, stack
<https://leetcode.com/problems/basic-calculator-ii/>
**Level:** 3
<details><summary>Hint</summary>
Keep addition and subtraction terms on the stack; multiplication and division are applied immediately to the previous top of the stack.
</details>
<details><summary>Key idea & complexity</summary>
Single-pass with a stack for +/- terms and inline evaluation of */÷, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
No brackets, so only operator precedence is needed. Scan the string reading whole numbers and remembering the previous operator. When a number and the next operator have been read (or the expression ends): if the previous operator was `+`, push the number as is; if `-`, push its negation; if `*` or `/`, pop the top, apply the multiplication/division to it and push the result back — this applies the higher-precedence operation immediately instead of leaving it pending. At the end the answer is the sum of all elements on the stack, since only additions and subtractions remain. Integer division rounds toward zero, which must be checked separately for negative results (C's `/` already truncates toward zero, so this is free in C but not in Python).
</details>

### 04.3.3  Decode String  ·  LC #394  ·  Medium  ·  string, stack, recursion
<https://leetcode.com/problems/decode-string/>
**Level:** 3
<details><summary>Hint</summary>
Push `(repeat count, string so far)` whenever you meet `[`, and unwind at `]` by repeating and concatenating.
</details>
<details><summary>Key idea & complexity</summary>
Stack of (count, partial-string) pairs for nested repetition, O(n * maxK) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Scan the string building the current string and reading digits into a multiplier. When `[` is met, push the pair (current accumulated number, string accumulated so far) onto the stack and reset both to start a new nested level. When `]` is met, pop the top pair `(k, prev)` and set the current string to `prev + current repeated k times`. Other characters are appended directly to the current string. The stack carries exactly the information needed to resume the outer level once the inner level has been unwound — the same nesting principle as Score of Parentheses, but with a string instead of a number. In C each level needs its own growable buffer (or a single shared buffer with saved lengths: push the current length, and at `]` repeat the tail `k` times in place).
</details>

### 04.3.4  Mini Parser  ·  LC #385  ·  Medium  ·  string, stack, depth-first-search
<https://leetcode.com/problems/mini-parser/>
**Level:** 3
<details><summary>Hint</summary>
Push a new list onto the stack on every `[`; attach it to the previous top when `]` closes the level.
</details>
<details><summary>Key idea & complexity</summary>
Stack of NestedInteger contexts, one per nesting level, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The stack directly represents the path of nested lists from the root to the current depth. When `[` is met, push a new empty NestedInteger list, the collector for the current level. When a number is met (possibly negative, read as a whole up to the next separator), add it inside the list on top of the stack. When `]` is met, pop the completed list on top and add it — if the stack is not yet empty — as an element into the new top (its parent list). Once the whole input is consumed, if the stack was empty the entire time (the input was a bare number), return that directly; otherwise return the last root list popped from the stack.
</details>

### 04.3.5  Number of Atoms  ·  LC #726  ·  Hard  ·  hash-table, string, stack, sorting
<https://leetcode.com/problems/number-of-atoms/>
**Level:** 4
<details><summary>Hint</summary>
Put a separate count table on the stack for every bracket level; `)` multiplies the whole inner table by the multiplier and merges it into the outer one.
</details>
<details><summary>Key idea & complexity</summary>
Stack of element-count maps merged on `)`, with digit-run multiplication, O(n log n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Keep a stack of hash maps (element → count), one per open bracket level, the bottom one being the collector for the whole formula. When `(` is met, push a new empty map as a new level. When an element name is read (uppercase letter + optional lowercase letters) followed by an optional multiplier, add it directly into the map on top. When `)` and its optional following multiplier `k` are met, pop the top map, multiply every value in it by `k`, and merge (sum) the result into the new top map. At the end the only remaining map on the stack holds the whole formula's elements, which are output in alphabetical order. The multiplication always happens immediately on closing, because an outer multiplier can multiply again if brackets are nested several levels deep. In C, a "map" can be a small sorted array of `(name[3], count)` structs or a fixed array of 26*27 slots indexed by the (up to two-letter) name; `qsort` the final keys with `strcmp`.
</details>

### 04.3.6  Basic Calculator  ·  LC #224  ·  Hard  ·  math, string, stack, recursion
<https://leetcode.com/problems/basic-calculator/>
**Level:** 4
<details><summary>Hint</summary>
Push `(current result, sign)` at every `(`, and merge into the top at `)` in the same way as Decode String.
</details>
<details><summary>Key idea & complexity</summary>
Stack of (sign, running-result) pairs across nested parentheses, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Now there are brackets but no multiplication/division, so only `+`/`-` and nesting are needed. Keep a running `result` and the current sign. When `(` is met, push the current `result` and current sign onto the stack and reset both to start a fresh inner computation. When `)` is met, pop the outer sign and outer `result`, and set `result = outer_result + outer_sign * result` — this merges the just-computed inner value back into the outer context. This structure is exactly the same as Decode String, but instead of string concatenation numbers are added — notice the same stack skeleton recurring for the third time with a different data type. Use `long` for the result; inputs fit `int` but intermediate sums are safer wide.
</details>

### 04.3.7  Parsing A Boolean Expression  ·  LC #1106  ·  Hard  ·  string, stack, recursion
<https://leetcode.com/problems/parsing-a-boolean-expression/>
**Level:** 4
<details><summary>Hint</summary>
Push truth values and operators onto the stack; `,` is skipped, `)` gathers all operands of the latest operator and evaluates.
</details>
<details><summary>Key idea & complexity</summary>
Recursive-descent evaluation using an explicit stack over the token string, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Scan the string character by character. `t`, `f`, `!`, `&`, `|` are pushed as is (truth values and operators); `(` and `,` are skipped (they carry no information in the stack skeleton). When `)` is met, pop all consecutive truth values from the stack until an operator appears — these are the operands of the group just closed. Pop the operator too, apply it to the operands (`!` takes one and negates it, `&`/`|` combine all of them with AND/OR logic), and push the result back as a new "operand" for the outer level. When the whole string has been scanned, the single remaining value on the stack is the answer. This generalises the Basic Calculator model to n-ary operators instead of a fixed two operands. A `char` stack suffices in C since every token is a single character.
</details>

---

## Unit 4 — Monotonic stack — next greater element

### 04.4.1  Next Greater Element I  ·  LC #496  ·  Easy  ·  array, hash-table, stack, monotonic-stack
<https://leetcode.com/problems/next-greater-element-i/>
**Level:** 2
<details><summary>Hint</summary>
Run a monotonic stack over all of `nums2` once, store each element's next greater in a hash map, and look `nums1` up in it.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic decreasing stack over nums2, mapped through a hash lookup, O(n+m) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Run a monotonic decreasing stack over the whole of `nums2` only once: scan left to right, and whenever the new number is greater than the top, pop the top and record in a hash map that its "next greater" is this new number; repeat until the top is no longer smaller or the stack empties; then push the new number. At the end every number still on the stack never got a successor, so its answer is -1. Because `nums1` is a subset of `nums2`, every query is a single hash-map lookup after the precomputation — the stack is run once, not once per query. In C, values are bounded (0..10^4), so a plain `int ans_of[10001]` array replaces the hash map.
</details>

### 04.4.2  Final Prices With a Special Discount in a Shop  ·  LC #1475  ·  Easy  ·  array, stack, monotonic-stack
<https://leetcode.com/problems/final-prices-with-a-special-discount-in-a-shop/>
**Level:** 2
<details><summary>Hint</summary>
Keep on the stack the indices whose discount has not been found yet; the first price that is smaller or equal gives the discount.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-decreasing stack of indices waiting for a discount, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same next-greater skeleton flipped: push indices (not values), and whenever the new price is at most the price at the index on top, this is exactly the "next smaller or equal" the top was waiting for — subtract the discount from that price directly and pop the index, repeating as long as the condition holds. Then push the current index. Indices left on the stack at the end never found a discount, so their price stays unchanged. This is a good first problem where storing indices instead of values pays off, because both the original value and its position are needed to update the result.
</details>

### 04.4.3  Daily Temperatures  ·  LC #739  ·  Medium  ·  array, stack, monotonic-stack
<https://leetcode.com/problems/daily-temperatures/>
**Level:** 3
<details><summary>Hint</summary>
Keep temperature indices on the stack in decreasing order; a warmer day pops every top it beats and computes the distance.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-increasing stack of indices, distance = current index - popped index, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The namesake problem of the whole unit. The stack holds indices whose temperatures are in decreasing (or equal) order from bottom to top. Walk the days: as long as the current temperature is greater than the temperature at the index on top, pop the top and set its answer to `current index - popped index` (the number of days waited), repeating until the condition fails. Then push the current index. Every index is pushed and popped at most once, so the amortised total is O(n) even though the naive solution would be O(n²).
</details>

### 04.4.4  Next Greater Element II  ·  LC #503  ·  Medium  ·  array, stack, monotonic-stack
<https://leetcode.com/problems/next-greater-element-ii/>
**Level:** 3
<details><summary>Hint</summary>
Walk the array twice (index modulo n) with the same monotonic stack so that wrap-around is taken into account.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic stack over the array traversed twice to simulate circularity, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same monotonic decreasing stack as in the ordinary next-greater problem, but the array is walked twice in succession using the index `i % n` — this simulates circularity without actually doubling the array in memory. The first round fills the stack and finds most of the answers; the second round pushes nothing new (or pushes but the push is never used, since those answers are already recorded) and only gives the first-round indices still on the stack a chance to find a successor at the start of the array. Elements that find no successor in either round remain on the stack and get the answer -1. Initialise the answer array to -1 and only ever write an answer once per index.
</details>

### 04.4.5  Online Stock Span  ·  LC #901  ·  Medium  ·  stack, design, monotonic-stack, data-stream
<https://leetcode.com/problems/online-stock-span/>
**Level:** 3
<details><summary>Hint</summary>
Keep `(price, span)` pairs on the stack in decreasing price order; a new price swallows all cheaper pairs and inherits their spans.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-increasing stack of (price, span) pairs, amortised O(1) per call.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the online version of the next-greater idea: on every `next(price)` call the stack holds `(price, span)` pairs in decreasing price order from bottom to top. As long as the price on top is at most the current price, pop it and add its span to the current tentative span (started at 1) — this collects all consecutive days whose price was at most today's. Finally push `(current price, collected span)` and return the span. Because each `(price, span)` pair is pushed and popped at most once over the whole call sequence, the amortised time per call is O(1) even though a single call could unwind the entire stack. In C the stack must be growable (`realloc`), since the number of calls is not known up front — this is the `Vec` pattern with a struct element.
</details>

### 04.4.6  Remove K Digits  ·  LC #402  ·  Medium  ·  string, stack, greedy, monotonic-stack
<https://leetcode.com/problems/remove-k-digits/>
**Level:** 3
<details><summary>Hint</summary>
Keep digits on the stack in increasing order; if the new digit is smaller than the top and removals remain, pop the top.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-decreasing stack of digits, popping larger digits while k allows, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The smallest possible number arises by keeping the leading digits as small as possible, so the stack is kept increasing: walk the digits, and as long as `k > 0` and the new digit is smaller than the top, pop the top and decrement `k` — this is always beneficial, because a smaller digit in an earlier position makes the whole number smaller regardless of what comes later. Then push the new digit. If `k` remains after the scan (the sequence was already increasing), remove the remaining `k` digits from the end of the stack. Finally strip any leading zeros and handle an empty result as the special case `"0"`.
</details>

### 04.4.7  Find the Most Competitive Subsequence  ·  LC #1673  ·  Medium  ·  array, stack, greedy, monotonic-stack
<https://leetcode.com/problems/find-the-most-competitive-subsequence/>
**Level:** 3
<details><summary>Hint</summary>
Same stack idea as Remove K Digits, but the removal limit is dynamic: pop the top only if the remaining elements still suffice to fill k.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-decreasing stack of chosen digits, respecting the remaining-length budget, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same monotonic increasing stack as Remove K Digits, but the pop condition is stricter: pop the top if the new element is smaller **and** after popping there are still enough remaining elements (including the current one) to fill the desired length `k` — the condition is `stack non-empty and top > current and (stack_size - 1) + (remaining elements including current) >= k`. Push the current element only if the stack size is still below `k`. This dynamic budget check is the difference from Remove K Digits: there the removal limit `k` was a fixed number, here it depends on the remaining input length at every moment.
</details>

### 04.4.8  Remove Duplicate Letters  ·  LC #316  ·  Medium  ·  string, stack, greedy, monotonic-stack
<https://leetcode.com/problems/remove-duplicate-letters/>
**Level:** 3
<details><summary>Hint</summary>
Keep letters on the stack in increasing order, but pop the top only if it occurs again later and is not already committed in the stack.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-decreasing stack with last-occurrence lookahead and an in-stack membership set, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same monotonic increasing stack, but with two extra conditions: (1) every letter may appear on the stack at most once, so keep a separate set of letters already on the stack and skip entirely any letter that is already there; (2) the top may be popped only if it still occurs later in the string (precompute each letter's last occurrence index) — otherwise popping would lose that letter completely. The pop condition is therefore: the new letter is smaller than the top, the new letter is not already on the stack, and the top still occurs later. This is the most intricate next-greater-style stack of the unit, because it combines monotonicity, uniqueness and lookahead in the same loop. In C both the membership set and the last-occurrence table are `int [26]` arrays.
</details>

### 04.4.9  132 Pattern  ·  LC #456  ·  Medium  ·  array, binary-search, stack, monotonic-stack
<https://leetcode.com/problems/132-pattern/>
**Level:** 3
<details><summary>Hint</summary>
Walk the array right to left keeping a stack of candidate middle values (the "2" of 132) and the best popped "k" candidate.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic decreasing stack tracking the best-so-far 'third' value, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Walk the array right to left keeping a variable `k` (the largest value popped from the stack so far — this represents the best possible "2" value for the middle of the 132 pattern). The stack stays decreasing and represents possible "2"-value candidates: as long as the current element is greater than the top, pop the top and update `k = max(k, popped)` — the popped value was smaller than the current element but larger than everything popped before it, so it is a good "2" candidate. If the current element is smaller than `k`, a 132 pattern has been found (current is the "1", `k` is the "2", and the "3" is some larger value popped earlier to the right). Otherwise push the current element for later. This is the hardest problem of the unit, because the stack does not directly yield the answer but maintains, via `k`, indirect information about the best possible middle value. Initialise `k` to `INT_MIN` (from `<limits.h>`) or `LONG_MIN` with a `long` stack.
</details>

---

## Unit 5 — Monotonic stack — histogram and rectangle problems

### 04.5.1  Sum of Subarray Minimums  ·  LC #907  ·  Medium  ·  array, dynamic-programming, stack, monotonic-stack
<https://leetcode.com/problems/sum-of-subarray-minimums/>
**Level:** 3
<details><summary>Hint</summary>
Each element's contribution to the sum is its value times the number of subarrays in which it is the minimum — compute that from the distances to the nearest smaller on the left and right.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-decreasing stack giving left/right span per element as contribution, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Each element `arr[i]` is the minimum of exactly those subarrays that fit within the interval (nearest strictly smaller on the left, nearest smaller-or-equal on the right) — call these distances `left[i]` and `right[i]`. The element's total contribution to the sum is `arr[i] * left[i] * right[i]`. Both distances are computed in one pass with an increasing monotonic stack: when an element is popped because the next one is smaller, its `right` distance has just been found (current index minus the popped index), and when it was pushed, its `left` distance was determined by the top of the stack at that time. To avoid double-counting equal elements, use strictly-smaller on one side and smaller-or-equal on the other. Accumulate in `long long` modulo 1e9+7.
</details>

### 04.5.2  Sum of Subarray Ranges  ·  LC #2104  ·  Medium  ·  array, stack, monotonic-stack
<https://leetcode.com/problems/sum-of-subarray-ranges/>
**Level:** 3
<details><summary>Hint</summary>
A subarray's range is max - min, so compute separately the sum of all subarray maximums and the sum of all subarray minimums with monotonic stacks, and subtract.
</details>
<details><summary>Key idea & complexity</summary>
Two monotonic-stack passes — sum of maximums minus sum of minimums, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because `range(subarray) = max(subarray) - min(subarray)`, the whole answer decomposes into two separate computations: the sum of all subarray maximums minus the sum of all subarray minimums. Each is computed with exactly the same technique as Sum of Subarray Minimums — one increasing monotonic stack for the minimum contributions, one decreasing stack for the maximum contributions, both in O(n) time. This problem is a direct extension of the previous one, and its key insight is to decompose "range" into two separate known subproblems before even starting to write the stack. Write one helper taking a comparison direction and call it twice.
</details>

### 04.5.3  Maximum Width Ramp  ·  LC #962  ·  Medium  ·  array, two-pointers, stack, monotonic-stack
<https://leetcode.com/problems/maximum-width-ramp/>
**Level:** 3
<details><summary>Hint</summary>
Collect on the stack the indices whose value is strictly smaller than everything before — these are the only valid left ends.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic decreasing candidate stack of left indices, consumed from the right, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A ramp `(i, j)` requires `arr[i] <= arr[j]` and `i < j`, and the width is maximised when `i` is as small and `j` as large as possible. First build a stack of the indices whose value is strictly smaller than all preceding values — these are the only indices that can ever be the optimal left end, because any earlier larger value would always be a worse left choice. Then walk the array **right to left**: as long as the value at the top of the stack is at most the current value, pop the top and update the best width with `current index - popped index`. The two passes (build from the left, consume from the right) make this a slightly different usage from the single-pass next-greater-style stack.
</details>

### 04.5.4  Largest Rectangle in Histogram  ·  LC #84  ·  Hard  ·  array, stack, monotonic-stack, range-minimum-maximum-query
<https://leetcode.com/problems/largest-rectangle-in-histogram/>
**Level:** 4
<details><summary>Hint</summary>
Same left/right-distance idea as Sum of Subarray Minimums, but the contribution is height times width, not value times distance.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-decreasing stack, area = height x (right - left - 1) per popped bar, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The namesake problem of the unit generalises the Sum of Subarray Minimums idea to an area: for every bar `h[i]`, the largest rectangle whose height is exactly `h[i]` extends left to the nearest strictly lower bar and right to the nearest bar of at most equal height. Keep an increasing monotonic stack of indices; when a new bar is lower than the top, pop the top `h` and compute its area `height[h] * (current index - new top remaining on stack - 1)` (or the whole distance from zero if the stack empties) — update the best answer. Finally append a sentinel bar of height 0 after the array so that all remaining bars on the stack get computed without a separate end-of-loop pass. Compute the area in `long long`; heights and widths are each up to 10^5.
</details>

### 04.5.5  Maximal Rectangle  ·  LC #85  ·  Hard  ·  array, dynamic-programming, stack, matrix
<https://leetcode.com/problems/maximal-rectangle/>
**Level:** 4
<details><summary>Hint</summary>
Turn every row into a histogram (height of consecutive 1s vertically), and run Largest Rectangle on each row.
</details>
<details><summary>Key idea & complexity</summary>
Per-row histogram heights fed into largest-rectangle-in-histogram, O(nm) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Convert the two-dimensional problem into a series of one-dimensional ones: for every row `r`, compute `height[j]` = how many consecutive `1`s there are vertically in column `j` ending at row `r` (if `matrix[r][j] == 0` the height resets to zero; otherwise `height[j] += 1` from the previous row). The height profile obtained for each row is exactly a histogram, on which the same Largest Rectangle in Histogram algorithm is run unchanged. The best over all rows is the answer. Total time is O(nm), because every row yields a histogram of length O(m) and the monotonic stack solves it in O(m) — a clear example of packaging the one-dimensional stack technique as a reusable helper function. Reuse your `largest_rect` from the previous problem verbatim.
</details>

### 04.5.6  Trapping Rain Water  ·  LC #42  ·  Hard  ·  array, two-pointers, dynamic-programming, stack
<https://leetcode.com/problems/trapping-rain-water/>
**Level:** 4
<details><summary>Hint</summary>
Keep the stack in decreasing order; when a taller wall appears, the popped bottom collects water according to the width and the minimum of the two wall heights.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic non-increasing stack pairing each valley bottom with left/right walls, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
An alternative to the two-pointer solution: keep a decreasing monotonic stack of indices (heights). When a new bar is taller than the top, pop the top `bottom` — it is the floor of the water between two walls. If the stack is not empty after the pop, the left wall is the new top remaining on the stack and the right wall is the current bar; the water collected at this level is `(min(leftHeight, rightHeight) - bottomHeight) * width`, where the width is the difference of the indices minus one. Repeat the pop while the condition holds, then push the current index. Structurally this is the mirror image of Largest Rectangle in Histogram: there the largest area around one low bar was sought, here the water that accumulates above one low floor between two taller walls is computed.
</details>

### 04.5.7  Sum of Total Strength of Wizards  ·  LC #2281  ·  Hard  ·  array, stack, monotonic-stack, prefix-sum
<https://leetcode.com/problems/sum-of-total-strength-of-wizards/>
**Level:** 4
<details><summary>Hint</summary>
Same left/right nearest-smaller skeleton as Sum of Subarray Minimums, but the contribution needs the sum of subarray sums — use the prefix sum of the prefix sums.
</details>
<details><summary>Key idea & complexity</summary>
Monotonic stack for min-per-span combined with prefix-of-prefix sums, O(n) time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The hardest problem of the unit combines the monotonic stack with a double prefix sum. For every element `i` (the minimum on a certain subarray range) compute the same `left[i]`/`right[i]` distance to the nearest smaller as in Sum of Subarray Minimums. The contribution, however, is no longer `value * left * right`, because what is required is the **sum of the sums** of all subarrays in which `i` is the minimum — this is computed with the prefix sum of the prefix sums (`prefix-of-prefix`, i.e. the cumulative sum of the cumulative sums), which gives in closed form the sum over any range without a separate loop per subarray. The formula combines `value[i]`, `left[i]`, `right[i]` and the prefix-of-prefix values from the left and the right in modular arithmetic (the result is asked modulo a prime). The stack skeleton itself is familiar; the difficulty is in deriving the contribution formula. Keep every intermediate reduced modulo 1e9+7 and add `MOD` before subtracting to stay non-negative.
</details>

---

## Unit 6 — Min-stack and stack-backed design problems

### 04.6.1  Implement Stack using Queues  ·  LC #225  ·  Easy  ·  stack, design, queue
<https://leetcode.com/problems/implement-stack-using-queues/>
**Level:** 2
<details><summary>Hint</summary>
Push the new element into the queue, then rotate the queue right away so the new element moves to the front — this turns FIFO into LIFO.
</details>
<details><summary>Key idea & complexity</summary>
One queue rotated after each push to keep the newest element in front, O(n) push, O(1) pop.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Use a single queue. `push(x)`: append `x` to the back of the queue, then rotate the queue as many times as it had elements before `x` was added (dequeue the front and enqueue it at the back) — this moves `x` to the front, so the newest element is always at the head. `pop()` and `top()` are then plain front-of-queue operations. This is a good counterpart to the previous unit: where a stack is usually implemented with an array, here LIFO behaviour is produced from a FIFO structure by a rotation trick alone, as long as the order is restored immediately after every push. In C the queue is a ring buffer (`../../c_learning/10_data_structures/lesson.md`).
</details>

### 04.6.2  Implement Queue using Stacks  ·  LC #232  ·  Easy  ·  stack, design, queue
<https://leetcode.com/problems/implement-queue-using-stacks/>
**Level:** 2
<details><summary>Hint</summary>
Always push onto the in-stack; when the out-stack runs empty, pour the whole in-stack into it at once before the next pop/peek.
</details>
<details><summary>Key idea & complexity</summary>
Two stacks — an in-stack for pushes, an out-stack refilled by reversal only when empty, amortised O(1).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Keep two stacks: `in` receives every `push` call directly. `pop()`/`peek()` use the `out` stack: if `out` is empty, pour the whole content of `in` into it (pop from `in` and push onto `out`, which reverses the order into correct FIFO order), then use the top of `out`. Every element moves from `in` to `out` at most once in its lifetime, so even though a single pour is O(n), the amortised time per operation is O(1). This is exactly the same "amortised double stack" principle as the previous problem, in the reverse direction.
</details>

### 04.6.3  Min Stack  ·  LC #155  ·  Medium  ·  stack, design
<https://leetcode.com/problems/min-stack/>
**Level:** 3
<details><summary>Hint</summary>
Store alongside every value the stack's minimum at that exact moment, so that popping automatically restores the correct previous minimum.
</details>
<details><summary>Key idea & complexity</summary>
Stack of (value, min-so-far) pairs, O(1) per operation.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The naive solution would search the whole stack for the minimum on every query, O(n). Instead every pushed node carries the pair (value, minimum of the stack right after this push) — when a new value is pushed, the new minimum is `min(new value, previous top's minimum)`. Because the minimum information is attached to every node rather than to a global variable, `pop()` automatically restores the correct previous minimum without any extra computation — the top node's minimum field is always current. An alternative O(1)-extra-space solution stores only differences from the previous minimum, but the pair solution is the most direct and generalises straight to the following design problems. In C: an array of `struct { int value, min; }` with `realloc` growth.
</details>

### 04.6.4  Design a Stack With Increment Operation  ·  LC #1381  ·  Medium  ·  array, stack, design
<https://leetcode.com/problems/design-a-stack-with-increment-operation/>
**Level:** 3
<details><summary>Hint</summary>
Do not update every element separately in increment — record the addition only at the topmost affected index and spread it on pop.
</details>
<details><summary>Key idea & complexity</summary>
Array-backed stack with a lazy per-index increment applied on pop, O(1) amortised per operation.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A naive `increment(k, val)` would update each of the bottom `k` elements separately, O(k) per call. Instead keep a separate `inc` array, and have `increment(k, val)` add `val` only at index `min(k, size) - 1` — this "lazy" mark means "when this element or any element below it is eventually popped, add this value". `pop()` returns the top `+ inc[i]`, and before removing it moves the value of `inc[i]` to the next lower index (`inc[i-1] += inc[i]`) before zeroing `inc[i]` — so the addition "trickles" downward only when needed. This lazy propagation makes push/pop and increment all O(1). The stack has a fixed `maxSize`, so in C two plain arrays of that size suffice, no `realloc`.
</details>

### 04.6.5  Design Browser History  ·  LC #1472  ·  Medium  ·  array, linked-list, stack, design
<https://leetcode.com/problems/design-browser-history/>
**Level:** 3
<details><summary>Hint</summary>
A new visit clears the forward history entirely — model it either by emptying the forward stack or by truncating the list at the current index.
</details>
<details><summary>Key idea & complexity</summary>
Two stacks (back/forward) or a dynamic array with a current index, O(1) amortised visit, O(k) back/forward.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The simplest model is two stacks: `back` holds all previous pages, `forward` holds the pages you can go forward to. `visit(url)`: push the current page onto `back`, make the new page current, and **empty the whole forward stack** — a new navigation discards the old forward history entirely. `back(steps)`: move the current page onto `forward` and pop from `back`, repeating `steps` times or until `back` empties. `forward(steps)` is symmetric in the other direction. The clearing in `visit` is the most important detail of the problem: it is the same "undo the rest of the history" principle as in Remove Outermost Parentheses-type problems, but across the entire forward stack at once. With the array-plus-index model, "empty forward" is just `len = cur + 1`, which is O(1) — the URL strings must be owned (copied) by the structure and freed on truncation.
</details>

### 04.6.6  Flatten Nested List Iterator  ·  LC #341  ·  Medium  ·  stack, tree, depth-first-search, design
<https://leetcode.com/problems/flatten-nested-list-iterator/>
**Level:** 3
<details><summary>Hint</summary>
Keep iterators representing the nested lists on the stack; `hasNext()` lazily unpacks lists until the top is a single integer.
</details>
<details><summary>Key idea & complexity</summary>
Stack of iterators/indices, lazily flattening on next()/hasNext(), amortised O(1) per element.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Instead of flattening the whole nested structure up front (which would waste memory and time if the iterator is never exhausted), the stack holds state lazily: at initialisation push the entire input list (in reverse order) onto the stack. `hasNext()` unpacks the top as long as it is a list rather than a single integer: pop the list, push its elements in reverse order, repeat until the top is an integer or the stack is empty. `next()` calls `hasNext()` first for safety and then pops the top integer. Every element is unpacked from the stack exactly once over the whole iteration, so the total time stays linear even though the work is spread across different `hasNext()` calls. In C the stack elements are pointers to `NestedInteger` nodes — an explicit-stack replacement for the recursive DFS.
</details>

### 04.6.7  Maximum Frequency Stack  ·  LC #895  ·  Hard  ·  hash-table, stack, design, ordered-set
<https://leetcode.com/problems/maximum-frequency-stack/>
**Level:** 4
<details><summary>Hint</summary>
Group elements by frequency into their own stacks; push always lifts an element one frequency level up, pop takes from the highest level's stack.
</details>
<details><summary>Key idea & complexity</summary>
Hash map of frequency to a stack of elements at that frequency, plus a running maxFreq, O(1) amortised per operation.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Keep a hash map from frequency to a stack containing all the elements that have been seen exactly that many times *right now*, in correct push order. `push(x)`: increment `x`'s frequency and push `x` onto the stack of its new frequency level; update `maxFreq` if needed. `pop()`: take and remove the top element of the `maxFreq`-level stack (by definition it is both the most frequent and the most recently pushed among those), decrement its frequency by one, and if the `maxFreq`-level stack became empty, decrement `maxFreq` by one. Because every frequency level is its own separate stack, the rule "most frequent, newest wins ties" is realised automatically through the stack's LIFO order without a separate timestamp. In C: the frequency-to-stack map is an array of growable stacks indexed by frequency (frequency <= number of pushes), and the value-to-frequency map is a hash table you write (values are `int` in a bounded range — an open-addressing table with `int` keys is enough).
</details>

### 04.6.8  Dinner Plate Stacks  ·  LC #1172  ·  Hard  ·  hash-table, stack, design, heap-priority-queue
<https://leetcode.com/problems/dinner-plate-stacks/>
**Level:** 4
<details><summary>Hint</summary>
Keep an array of stacks and a min-heap of the indices of stacks that still have room, so that push finds the right stack fast.
</details>
<details><summary>Key idea & complexity</summary>
Array of capped stacks plus a min-heap of indices with room, amortised O(log n) per operation.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Each "stack" is its own bounded-capacity stack in an array. `push(val)`: use a min-heap (or an ordered set) of the indices of stacks that still have room, take the smallest such index and push there — if the stack fills up, it drops out of the candidate set automatically the next time it is checked; if the candidate set is empty, open a new stack at the end. `popAtStack(index)` removes directly from that stack and adds its index back into the candidate set if the stack did not empty completely. `pop()` is `popAtStack(rightmost non-empty stack)` — finding the rightmost non-empty stack is handled by keeping a separate pointer that moves left as stacks at the end empty out. This is the hardest design problem of the unit, because it combines many small stacks into one larger indexed structure. The heap is the one from `../../c_learning/10_data_structures/lesson.md`; guard against stale heap entries (an index popped from the heap that is already full or beyond the current rightmost stack) by re-checking before use.
</details>

---

## Progress

- [ ] 04.1.1 Valid Parentheses (LC #20)
- [ ] 04.1.2 Remove Outermost Parentheses (LC #1021)
- [ ] 04.1.3 Make The String Great (LC #1544)
- [ ] 04.1.4 Baseball Game (LC #682)
- [ ] 04.1.5 Minimum Add to Make Parentheses Valid (LC #921)
- [ ] 04.1.6 Minimum Remove to Make Valid Parentheses (LC #1249)
- [ ] 04.1.7 Check if a Parentheses String Can Be Valid (LC #2116)
- [ ] 04.1.8 Score of Parentheses (LC #856)
- [ ] 04.1.9 Longest Valid Parentheses (LC #32)
- [ ] 04.2.1 Backspace String Compare (LC #844)
- [ ] 04.2.2 Remove All Adjacent Duplicates In String (LC #1047)
- [ ] 04.2.3 Crawler Log Folder (LC #1598)
- [ ] 04.2.4 Simplify Path (LC #71)
- [ ] 04.2.5 Validate Stack Sequences (LC #946)
- [ ] 04.2.6 Asteroid Collision (LC #735)
- [ ] 04.2.7 Exclusive Time of Functions (LC #636)
- [ ] 04.3.1 Evaluate Reverse Polish Notation (LC #150)
- [ ] 04.3.2 Basic Calculator II (LC #227)
- [ ] 04.3.3 Decode String (LC #394)
- [ ] 04.3.4 Mini Parser (LC #385)
- [ ] 04.3.5 Number of Atoms (LC #726)
- [ ] 04.3.6 Basic Calculator (LC #224)
- [ ] 04.3.7 Parsing A Boolean Expression (LC #1106)
- [ ] 04.4.1 Next Greater Element I (LC #496)
- [ ] 04.4.2 Final Prices With a Special Discount in a Shop (LC #1475)
- [ ] 04.4.3 Daily Temperatures (LC #739)
- [ ] 04.4.4 Next Greater Element II (LC #503)
- [ ] 04.4.5 Online Stock Span (LC #901)
- [ ] 04.4.6 Remove K Digits (LC #402)
- [ ] 04.4.7 Find the Most Competitive Subsequence (LC #1673)
- [ ] 04.4.8 Remove Duplicate Letters (LC #316)
- [ ] 04.4.9 132 Pattern (LC #456)
- [ ] 04.5.1 Sum of Subarray Minimums (LC #907)
- [ ] 04.5.2 Sum of Subarray Ranges (LC #2104)
- [ ] 04.5.3 Maximum Width Ramp (LC #962)
- [ ] 04.5.4 Largest Rectangle in Histogram (LC #84)
- [ ] 04.5.5 Maximal Rectangle (LC #85)
- [ ] 04.5.6 Trapping Rain Water (LC #42)
- [ ] 04.5.7 Sum of Total Strength of Wizards (LC #2281)
- [ ] 04.6.1 Implement Stack using Queues (LC #225)
- [ ] 04.6.2 Implement Queue using Stacks (LC #232)
- [ ] 04.6.3 Min Stack (LC #155)
- [ ] 04.6.4 Design a Stack With Increment Operation (LC #1381)
- [ ] 04.6.5 Design Browser History (LC #1472)
- [ ] 04.6.6 Flatten Nested List Iterator (LC #341)
- [ ] 04.6.7 Maximum Frequency Stack (LC #895)
- [ ] 04.6.8 Dinner Plate Stacks (LC #1172)
