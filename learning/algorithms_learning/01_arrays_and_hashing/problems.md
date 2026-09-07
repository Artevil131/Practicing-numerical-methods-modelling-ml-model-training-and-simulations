# Chapter 01 — Problems

How to work these:

1. Read the matching unit section in `lesson.md` first.
2. Attempt the problem in C, in `algorithms_learning/01_arrays_and_hashing/solutions/<slug>.c` (create the `solutions/` folder yourself). Write your own tests in `main()` — small inputs, the edge cases from the lesson's pitfalls, and one case with collisions if you use a hash table. Compile with `cc -Wall -Wextra -std=c11 -O2 -o sol <slug>.c` and fix every warning.
3. Only when stuck (or after 30 minutes) open the **Hint**.
4. Only after solving, or after another honest attempt, open **Key idea & complexity** and then **Approach**. The approach is the worked reasoning — read it to compare with what you did, not to copy.

Levels: 1 = warm-up, 2 = standard pattern application, 3 = combination of ideas or a non-obvious invariant, 4 = hard, 5 = very hard.

---

## Unit 1 — Hash-set membership & dedup

### 01.1.1  Contains Duplicate  ·  LC #217  ·  Easy  ·  array, hash-table, sorting
<https://leetcode.com/problems/contains-duplicate/>
**Level:** 2
<details><summary>Hint</summary>
Add values to a set, and before each insert check whether the value is already there.
</details>
<details><summary>Key idea & complexity</summary>
Hash set in one pass, `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Walk the array once and maintain a hash set of values seen so far. Before inserting each element, check whether it is already in the set — if it is, you have found a duplicate and can return `true` immediately. Invariant: the set contains exactly the elements seen before the current index, so the check is always correct at that moment. Membership test and insert are `O(1)` on average, so the whole solution is `O(n)` time and `O(n)` space in the worst case. An alternative is to sort the array first (`O(n log n)`, `O(1)` extra space) and compare adjacent elements — worth remembering when memory is tight. Most common mistake: forgetting that a duplicate means any two equal values, not only adjacent ones.
</details>

### 01.1.2  Single Number  ·  LC #136  ·  Easy  ·  array, bit-manipulation
<https://leetcode.com/problems/single-number/>
**Level:** 2
<details><summary>Hint</summary>
Think about what happens when you XOR the numbers that occur in pairs with each other.
</details>
<details><summary>Key idea & complexity</summary>
XOR over all elements, `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This one does not need a hash set at all — it is the instructive counter-example of the pattern. XOR is commutative and associative, and `x ^ x = 0`, `x ^ 0 = x`. When you XOR the whole array together, all the paired numbers cancel each other regardless of order, and what remains is exactly the number that occurs once. One pass, one integer variable — `O(n)` time, `O(1)` space. It pays to see this and the hash-set approach (count occurrences in a map, return the one whose count is 1) side by side: both work, but XOR needs no extra memory. Pitfall: this trick only works when exactly one element occurs an odd number of times and all others exactly twice — the more general "occurs `k` times" version (`single-number-ii`) needs a different technique.
</details>

### 01.1.3  Missing Number  ·  LC #268  ·  Easy  ·  array, hash-table, math, binary-search
<https://leetcode.com/problems/missing-number/>
**Level:** 2
<details><summary>Hint</summary>
Compare the array's sum with what the sum of `0..n` should be.
</details>
<details><summary>Key idea & complexity</summary>
Sum formula or XOR, `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Exactly one number from the range `0..n` is missing from a set of `n+1` candidates. Compute the expected sum with the formula `n(n+1)/2` and subtract the actual sum of the array — the difference is the missing number. This is `O(n)` time and `O(1)` space and needs no hash set. An alternative and numerically safer way (large arrays can overflow the sum) is XOR: XOR together all indices `0..n` and all array values — what remains is the missing number, by the same principle as in `single-number`. The problem belongs to this group because it is the logical cousin of duplicate-finding: instead of asking "which value has been seen twice", it asks "which value has not been seen at all". Remember that the array length is `n` but the value range is `0..n`, i.e. `n+1` possible values.
</details>

### 01.1.4  Intersection of Two Arrays  ·  LC #349  ·  Easy  ·  array, hash-table, two-pointers, binary-search
<https://leetcode.com/problems/intersection-of-two-arrays/>
**Level:** 2
<details><summary>Hint</summary>
Turn the first array into a set and filter the second one through it.
</details>
<details><summary>Key idea & complexity</summary>
Two hash sets, `O(n+m)` time, `O(n+m)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Convert one of the arrays into a hash set in `O(n)` time. Then walk the other array and keep, in a result set, the values that are also found in the first set — the result is collected into its own set so that duplicates are eliminated automatically, as the problem requires. Total time `O(n+m)`, space `O(n+m)` in the worst case. If the arrays are already sorted or memory is tight, a two-pointer approach (`O(n log n + m log m)` to sort, then `O(n+m)` to sweep, `O(1)` extra space) is the better option — that is where the idea for `intersection-of-two-arrays-ii`, which also preserves multiplicities, comes from. Most common pitfall: forgetting that the intersection returns each common value only once, even if it appears several times in both arrays.
</details>

### 01.1.5  Set Mismatch  ·  LC #645  ·  Easy  ·  array, hash-table, bit-manipulation, sorting
<https://leetcode.com/problems/set-mismatch/>
**Level:** 2
<details><summary>Hint</summary>
One number appears twice and one is missing — use the index as the hash.
</details>
<details><summary>Key idea & complexity</summary>
One pass + sums, or in-place marking, `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The array holds the values `1..n`, but one has been replaced by a copy of another, so one number is missing and one repeats. The handy way: walk the array and use the index `|v|-1` pointed to by each value `v` as a marker — flip the value at that position negative. If that position is already negative, its value is the duplicate. On a second pass, the index that stayed positive, plus one, is the missing number. This is the precursor of `first-missing-positive`: the array acts as its own hash set because the value range is bounded to `1..n`, which drops the space from `O(n)` to `O(1)`. `O(n)` time, two passes. Remember to restore the array to its original state if the problem requires it, and be careful not to swap the order of duplicate and missing number in the answer.
</details>

### 01.1.6  Find All Duplicates in an Array  ·  LC #442  ·  Medium  ·  array, hash-table, sorting
<https://leetcode.com/problems/find-all-duplicates-in-an-array/>
**Level:** 3
<details><summary>Hint</summary>
Same trick as set-mismatch, but there can be several duplicates.
</details>
<details><summary>Key idea & complexity</summary>
In-place marking using the value as an index, `O(n)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Generalisation of the previous problem: values are in `1..n` and each occurs 1 or 2 times; all duplicates must be listed. Walk the array and use the value `v` as the index `|v|-1`: flip the number there negative. If it is already negative, you have visited this index before — then `|v|` is a duplicate; add it to the result. This is exactly the same invariant as the hash-set approach (mark as seen), but the mark is stored in the array's own sign instead of a separate structure, so the space is `O(1)` extra rather than `O(n)`. `O(n)` time. Pitfall: use the absolute value `|v|` when reading a value, because earlier marks may have flipped it negative — the original sign must never be read directly.
</details>

### 01.1.7  Longest Consecutive Sequence  ·  LC #128  ·  Medium  ·  array, hash-table, union-find
<https://leetcode.com/problems/longest-consecutive-sequence/>
**Level:** 3
<details><summary>Hint</summary>
Start extending only from numbers whose predecessor is not in the set.
</details>
<details><summary>Key idea & complexity</summary>
Hash set + start only from run beginnings, `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The naive solution (sort and find the longest consecutive run) is `O(n log n)`. The linear solution needs a hash-set trick: put all the numbers into a set, then iterate over only those numbers `x` whose predecessor `x-1` is **not** in the set — these are the possible starts of runs. From each such start, count `x, x+1, x+2, ...` as far as the set contains them. The invariant that guarantees linearity: every number serves as an interior extension of a run at most once during the whole algorithm, because it is never used again as a starting point. Therefore the inner `while` loop performs `O(n)` steps in total across all outer iterations, even though at first glance it looks `O(n^2)`. `O(n)` time, `O(n)` space. Most common mistake: omitting the predecessor check and counting the same run many times from different starting points.
</details>

### 01.1.8  First Missing Positive  ·  LC #41  ·  Hard  ·  array, hash-table
<https://leetcode.com/problems/first-missing-positive/>
**Level:** 4
<details><summary>Hint</summary>
Place every value `v` in `[1, n]` into its own slot at index `v-1`.
</details>
<details><summary>Key idea & complexity</summary>
Array as its own hash (cyclic sort), `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The answer is always in `[1, n+1]`, because `n` elements can cover at most the numbers `1..n`. This bounds the search space so that the array itself suffices as the hash structure: walk the array and, for every value `v` with `1 <= v <= n`, swap it into index `v-1`, repeating until the value at the current position is either already correct or invalid. Invariant: after each swap at least one more number is in its correct place, so the loop performs at most `O(n)` swaps in total. On a second pass, the first index `i` where `nums[i] != i+1` gives the answer `i+1`; if all match, the answer is `n+1`. `O(n)` time, `O(1)` extra space. Pitfall: negative numbers, zeros and values greater than `n` must be left out of the placement, otherwise you get an indexing error.
</details>

---

## Unit 2 — Frequency counting

### 01.2.1  Valid Anagram  ·  LC #242  ·  Easy  ·  hash-table, string, sorting
<https://leetcode.com/problems/valid-anagram/>
**Level:** 2
<details><summary>Hint</summary>
Count the letters of the first word with plus, the second with minus, then check for zeros.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map or a 26-array, `O(n)` time, `O(1)` space (fixed alphabet).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
If the lengths of the words differ, the answer is `false` right away. Otherwise keep a counter array of 26 elements (or a hash map if the alphabet is larger): increment the counter for every letter of the first word and decrement for every letter of the second. Two words are anagrams exactly when every counter returns to zero — an invariant that follows directly from the fact that an anagram is the same multiset of letters in a different order. `O(n)` time, `O(1)` space with a fixed alphabet (otherwise `O(k)`, `k` = number of distinct characters). An alternative is to sort both words and compare (`O(n log n)`) — slower but simpler to prove correct. Pitfall: with a Unicode character set the 26-array is not enough; then you need a general map.
</details>

### 01.2.2  Ransom Note  ·  LC #383  ·  Easy  ·  hash-table, string, counting
<https://leetcode.com/problems/ransom-note/>
**Level:** 2
<details><summary>Hint</summary>
Count the magazine's characters and subtract the ransom note's characters from those counts.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map of magazine characters, `O(n+m)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Count every letter of the `magazine` string into a map or a 26-array. Then walk `ransomNote` and subtract one from the corresponding counter for each letter; if a counter goes below zero at any point, there were not enough letters and the answer is `false`. This is the one-directional cousin of `valid-anagram`: instead of demanding an exact balance, it is enough that the demand for each letter **fits** within the supply. `O(n+m)` time (`n` = ransomNote, `m` = magazine), `O(1)` space with a fixed alphabet. Most common mistake: forgetting that letters cannot be reused — every magazine letter is consumed once, so counting (not a mere membership check with a hash set) is mandatory.
</details>

### 01.2.3  Jewels and Stones  ·  LC #771  ·  Easy  ·  hash-table, string
<https://leetcode.com/problems/jewels-and-stones/>
**Level:** 2
<details><summary>Hint</summary>
Put the jewel characters in a set and count how many stones hit it.
</details>
<details><summary>Key idea & complexity</summary>
Hash set of jewels + counting, `O(n+m)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Build a hash set from the characters of the `jewels` string in `O(n)` time. Then walk `stones` and increment a counter for every character found in the set. This is the simplest possible example of combining a membership test with counting: no need to track quantities, only a yes/no check per stone, but the end result is still a count. `O(n+m)` time, `O(n)` space for the jewel set. A good problem for noticing the difference from the exact balance of the `valid-anagram` type: here only a membership test per element is needed, not a relation between a value and its count. Pitfall: letter case is significant (upper and lower case are different jewels); do not normalise case.
</details>

### 01.2.4  Majority Element  ·  LC #169  ·  Easy  ·  array, hash-table, divide-and-conquer, sorting
<https://leetcode.com/problems/majority-element/>
**Level:** 2
<details><summary>Hint</summary>
Simplest: count each number's occurrences and pick the one exceeding `n/2`.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map or Boyer-Moore voting, `O(n)` time, `O(1)` space (voting).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
With a frequency map the solution is straightforward: count every number's occurrences and return the one whose count exceeds `n/2` — the problem guarantees one exists, so the counter suffices and no separate check is needed. `O(n)` time, `O(n)` space. The more space-efficient solution is **Boyer-Moore voting**: keep a candidate and a counter; when the counter is 0, switch the candidate to the current element; increment the counter when the element matches the candidate, otherwise decrement. Invariant: because the majority element occurs more than half the time, it can never be completely "cancelled" by the other elements — the final candidate is always the majority element. `O(n)` time, `O(1)` space. This problem is a good bridge from counting-based thinking to invariant-based thinking.
</details>

### 01.2.5  Unique Number of Occurrences  ·  LC #1207  ·  Easy  ·  array, hash-table
<https://leetcode.com/problems/unique-number-of-occurrences/>
**Level:** 2
<details><summary>Hint</summary>
Count each value's occurrences, then check that the counts are all different.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map + a second set for the counts, `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two-stage counting: first a hash map from value to occurrence count (`O(n)`), then a second hash set of the map's **values** (i.e. the counts) — if inserting some count into the set fails (it is already there), two different values occur the same number of times and the answer is `false`. This directly combines the two ideas of this unit: frequency counting and a membership test for duplicates, but applied to the set of counters rather than to the original values. `O(n)` time and space. A good exercise in seeing that the values of a hash map can themselves be the keys of another hash structure — two-level thinking that recurs in more complex counting problems.
</details>

### 01.2.6  Group Anagrams  ·  LC #49  ·  Medium  ·  array, hash-table, string, sorting
<https://leetcode.com/problems/group-anagrams/>
**Level:** 3
<details><summary>Hint</summary>
Use each word's sorted form (or its letter histogram) as the map key.
</details>
<details><summary>Key idea & complexity</summary>
Hash map from sorted key to list, `O(n · k log k)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two words are anagrams exactly when they have the same multiset of letters — and a convenient way to normalise a multiset into a comparable key is to sort the word's letters alphabetically. Build a hash map whose key is each word's sorted form and whose value is a list of the original words; add every word to the list under the right key. All the words accumulated under the same key are anagrams of each other. `O(n · k log k)` time (`n` words, `k` = maximum length), because every word is sorted separately; this can be reduced to `O(n · k)` by using a 26-number histogram as the string key instead of sorting. `O(n · k)` space. Pitfall: any canonical representation works as the key, as long as two anagrams always produce the same key and two non-anagrams different keys.
</details>

### 01.2.7  Top K Frequent Elements  ·  LC #347  ·  Medium  ·  array, hash-table, divide-and-conquer, sorting
<https://leetcode.com/problems/top-k-frequent-elements/>
**Level:** 3
<details><summary>Hint</summary>
Count frequencies, then bucket the numbers by frequency as an index into an array.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map + bucket sort, `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
First a hash map counts every number's occurrences in `O(n)` time. Naively you would get the `k` most frequent by sorting by frequency in `O(n log n)` or with a heap in `O(n log k)`, but the linear solution uses **bucketing (bucket sort)**: because an occurrence count is always in `1..n`, create `n+1` buckets with index = frequency, and place every number in its bucket. Finally walk the buckets from largest to smallest and collect numbers until `k` have been gathered. Invariant: because the range of frequencies is bounded to `[1, n]` and not arbitrarily large, sorting can be replaced by direct indexing — the same idea as counting sort. `O(n)` time and space. Pitfall: remember that several numbers can share the same frequency; a bucket must be a list, not a single value.
</details>

### 01.2.8  Valid Sudoku  ·  LC #36  ·  Medium  ·  array, hash-table, matrix
<https://leetcode.com/problems/valid-sudoku/>
**Level:** 3
<details><summary>Hint</summary>
Keep a separate set of seen digits for every row, column and box.
</details>
<details><summary>Key idea & complexity</summary>
Nine hash sets each for rows, columns and 3x3 boxes, `O(1)` time (fixed size).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Maintain three sets (hash set or bitmask) per row, per column and per 3x3 box — 27 sets in total, or one map structure keyed by `(type, index, value)`. Visit every filled cell once; in each of the three corresponding sets (row, column, box) check whether the value has already been seen — if so, the board is not valid. The box index is computed as `(row/3)*3 + col/3`. This combines the frequency thinking of this unit with three simultaneous constraints during a single pass — a good example of one element being a member of several different hash structures at the same time. Because the board size is fixed at 9x9, time and space are effectively `O(1)` (more generally `O(n^2)` for an `n x n` board). Pitfall: check only filled cells; empty ones (`.`) are skipped.
</details>

---

## Unit 3 — Hash-map index lookup

### 01.3.1  Design HashMap  ·  LC #706  ·  Easy  ·  array, hash-table, linked-list, design
<https://leetcode.com/problems/design-hashmap/>
**Level:** 2
<details><summary>Hint</summary>
Use a fixed-size array of "buckets", each holding a list of key-value pairs.
</details>
<details><summary>Key idea & complexity</summary>
Bucket array + chaining (linked list), `O(1)` average per operation.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Build a fixed-size array (e.g. 1000 buckets), and let the hash function `hash(key) = key % bucketCount` choose the bucket. Each bucket is a list (or linked list) of key-value pairs to handle collisions. `put`: search the bucket for the key, update if found, otherwise append. `get`/`remove`: the same search in the bucket. This reveals what a hash map does under the hood in the other problems of this unit: the hash function turns a key into an index, and chaining (or open addressing) resolves collisions when two keys land in the same bucket. `O(1)` per operation on average, if the number of buckets is proportional to the number of elements and the hash function spreads keys evenly; `O(n)` in the worst case if everything lands in one bucket. Pitfall: rehashing (resize) for a growing table is often omitted for simplicity, but performance suffers without it on large inputs.
</details>

### 01.3.2  Two Sum  ·  LC #1  ·  Easy  ·  array, hash-table
<https://leetcode.com/problems/two-sum/>
**Level:** 2
<details><summary>Hint</summary>
Store each number and its index in a map; ask whether the complement is already in the map.
</details>
<details><summary>Key idea & complexity</summary>
Hash map complement → index, `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Walk the array once. At every step `i`, first ask whether the map contains the value `target - nums[i]` — if it does, its index together with `i` is the answer. Only then add `nums[i]` to the map with index `i`. The order matters: query before insert, so that the same element does not pair with itself. Invariant: the map always contains exactly the values seen before the current index, so a found pair is always in the correct order (the first index is smaller). `O(n)` time in one pass, `O(n)` space for the map — an improvement over the brute force `O(n^2)`. Pitfall: if the same value occurs several times, only the latest index survives in the map, but that does no harm because the first hit is still found correctly with the query-before-insert order.
</details>

### 01.3.3  Isomorphic Strings  ·  LC #205  ·  Easy  ·  hash-table, string
<https://leetcode.com/problems/isomorphic-strings/>
**Level:** 2
<details><summary>Hint</summary>
Require the mapping in both directions so it is truly one-to-one and not many-to-one.
</details>
<details><summary>Key idea & complexity</summary>
Two hash maps in both directions, `O(n)` time, `O(1)` space (fixed alphabet).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two strings are isomorphic if there exists a **bijection** from character to character. One map is not enough: if you only map `s -> t`, two different `s` characters could map to the same `t` character, which would break bijectivity. Therefore keep two maps, `mapST` and `mapTS`, and at every index check that, if either mapping already exists, it agrees with the current pair; if neither exists, add both at the same time. Invariant: the state of both maps always describes a consistent partial bijection over the characters seen so far. `O(n)` time, `O(1)` space (fixed alphabet, e.g. 256 ASCII characters in two directions). Pitfall: the one-map solution passes most tests but fails on a case such as `s="badc", t="baba"`.
</details>

### 01.3.4  Word Pattern  ·  LC #290  ·  Easy  ·  hash-table, string
<https://leetcode.com/problems/word-pattern/>
**Level:** 2
<details><summary>Hint</summary>
Same bijection idea as isomorphic-strings, but the units are words, not characters.
</details>
<details><summary>Key idea & complexity</summary>
Two hash maps word ↔ letter, `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same bijection invariant as in `isomorphic-strings`, but the unit is a word instead of a single character: split the sentence into words and keep two maps, letter to word and word to letter. At every index, check both directions for consistency before adding a new correspondence. If the number of words does not match the pattern's length, the answer is `false` right away. `O(n)` time (`n` = number of words), `O(n)` space for the maps, because words (unlike characters) do not come from a fixed alphabet. This problem is a good test of whether the learner understands the bijection idea in general and not just as a memorised 26-array trick — the unit changes, but the invariant stays the same.
</details>

### 01.3.5  First Unique Character in a String  ·  LC #387  ·  Easy  ·  hash-table, string, queue, counting
<https://leetcode.com/problems/first-unique-character-in-a-string/>
**Level:** 2
<details><summary>Hint</summary>
First count every character, then find the first whose count is 1.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map + second pass, `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two passes: first a hash map (or a 26-array) counts every character's occurrences in the whole string, in `O(n)` time. On the second pass, walk the string again from the start and return the **first** index whose character's counter is exactly 1. Two passes are mandatory, because during the first pass you cannot yet know whether the current character reappears later. Invariant: at the start of the second pass the map is complete and unchanging, so the first character in order with a count of 1 is genuinely the first unique character in the original order. `O(n)` time in total, `O(1)` space with a fixed alphabet. Pitfall: do not return the first character you *see* while counting — return the first one whose final count is 1.
</details>

### 01.3.6  4Sum II  ·  LC #454  ·  Medium  ·  array, hash-table
<https://leetcode.com/problems/4sum-ii/>
**Level:** 3
<details><summary>Hint</summary>
Put all sums from two arrays into a map, then look up the complement from the other two.
</details>
<details><summary>Key idea & complexity</summary>
Hash map of pair sums, `O(n^2)` time, `O(n^2)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Four arrays; we want how many quadruples sum to zero. Brute force would be `O(n^4)`. Instead, compute all `n^2` pairwise sums `A[i]+B[j]` into a hash map whose key is the sum and whose value is how many pairs produce it (`O(n^2)` time and space). Then iterate over all `n^2` pairs `C[k]+D[l]` and ask the map whether the complement `-(C[k]+D[l])` exists — if so, add its counter to the answer. This is an extension of the `two-sum` idea: the same "store one half in a map, query the complement from the other half" principle, but the unit is now a pair instead of a single number, which drops the total time from `O(n^4)` to `O(n^2)`. Pitfall: the same sum value can arise from many different pairs; a counter (not membership) is mandatory.
</details>

### 01.3.7  Sort Characters By Frequency  ·  LC #451  ·  Medium  ·  hash-table, string, sorting, heap-priority-queue
<https://leetcode.com/problems/sort-characters-by-frequency/>
**Level:** 3
<details><summary>Hint</summary>
Count character frequencies, then bucket them by frequency as in top-k-frequent-elements.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map + bucketing (bucket sort) by frequency, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Count every character's occurrences with a hash map in `O(n)` time. Because a frequency is bounded to `[1, n]`, use the same bucketing trick as in `top-k-frequent-elements`: create `n+1` buckets with index = frequency and place every character in its bucket. Build the answer by walking the buckets from the largest frequency to the smallest and repeating each character as many times as its frequency says. `O(n)` time in total (compared with `O(n log n)` if you sorted directly by frequency), `O(n)` space. This problem directly combines two patterns of this module: the hash map here counts instead of indexing, and the bucketing exploits a bounded value range like the array-as-hash trick of unit 1. Pitfall: ties (same frequency) may be output in any order; the problem does not require a particular internal order.
</details>

---

## Unit 4 — Prefix sums

### 01.4.1  Running Sum of 1d Array  ·  LC #1480  ·  Easy  ·  array, prefix-sum
<https://leetcode.com/problems/running-sum-of-1d-array/>
**Level:** 2
<details><summary>Hint</summary>
Each result element is the previous result element plus the current input.
</details>
<details><summary>Key idea & complexity</summary>
Direct prefix-sum computation, `O(n)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The most direct possible example of the pattern: `result[i] = result[i-1] + nums[i]`, computed in place into the same array or into a new one. This *is* the build phase of a prefix sum as such, without the query part — a good starting point for understanding what `P[i]` means before moving on to more complex queries. `O(n)` time, `O(1)` extra space if you overwrite the input array. Invariant: at every index `i` the result is exactly `a[0] + ... + a[i]`, which holds by induction — the base case `i=0` is trivial, and each step adds only one term to the previous correct sum. Pitfall: if you overwrite the input, make sure the problem does not need the original values later.
</details>

### 01.4.2  Find the Highest Altitude  ·  LC #1732  ·  Easy  ·  array, prefix-sum
<https://leetcode.com/problems/find-the-highest-altitude/>
**Level:** 2
<details><summary>Hint</summary>
The altitude at every point is the cumulative sum of the gain values up to the start.
</details>
<details><summary>Key idea & complexity</summary>
Prefix sum + tracking the maximum, `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The altitude at point `i` is the prefix sum `sum_{j<i} gain[j]` on top of the starting altitude (0). It is enough to walk the `gain` array once, keep a running sum (the current altitude) and update the maximum seen at every step — you do not even need to store a separate prefix array, because the query is "what is the largest value along the whole trip" rather than an arbitrary range. `O(n)` time, `O(1)` space. This is a good example of the prefix-sum *idea* (cumulative state) being useful even when the whole array need not be materialised — the same observation applies to many sliding-window problems. Pitfall: the starting altitude is 0, not the first gain value.
</details>

### 01.4.3  Find Pivot Index  ·  LC #724  ·  Easy  ·  array, prefix-sum
<https://leetcode.com/problems/find-pivot-index/>
**Level:** 2
<details><summary>Hint</summary>
The right sum at index `i` is the total sum minus the left sum minus `nums[i]`.
</details>
<details><summary>Key idea & complexity</summary>
Total sum + running left sum, `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
First compute the sum of the whole array in `O(n)` time. Then walk the array again keeping a running left sum `leftSum`; at each index the right sum is `total - leftSum - nums[i]`. The pivot is found when `leftSum == rightSum`. This avoids materialising an `O(n)` prefix array — only two integers are needed in memory, because the query is made in a single pass rather than repeatedly for different ranges. `O(n)` time, `O(1)` space. Invariant: at every index `leftSum` is exactly the sum of `a[0..i-1]` before `nums[i]` is processed, which keeps the left and right sum computations consistent throughout the pass. Pitfall: do not add `nums[i]` to `leftSum` before the comparison at this index.
</details>

### 01.4.4  Range Sum Query - Immutable  ·  LC #303  ·  Easy  ·  array, design, prefix-sum
<https://leetcode.com/problems/range-sum-query-immutable/>
**Level:** 2
<details><summary>Hint</summary>
Build the prefix array once in the constructor; answer queries with a difference.
</details>
<details><summary>Key idea & complexity</summary>
Precomputed prefix sums, `O(n)` build, `O(1)` per query.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the canonical use case of the prefix sum: in the constructor compute `P[i] = a[0] + ... + a[i-1]` for all `i`, `O(n)` once. Every query `sumRange(l, r)` returns `P[r+1] - P[l]`, `O(1)` per query regardless of the range length. This is exactly the efficiency gain no single pass can give: with `q` queries, the naive solution costs `O(nq)`, the prefix sum `O(n+q)`. Invariant: the array is immutable (`Immutable` in the name), so the precomputed `P` stays correct for the object's whole lifetime — if the array could change, every update would require recomputing `P` or a segment tree. Pitfall: the index shift — `P[i]` corresponds to the sum **before** index `i`, not including it — is an easy off-by-one.
</details>

### 01.4.5  Product of Array Except Self  ·  LC #238  ·  Medium  ·  array, prefix-sum
<https://leetcode.com/problems/product-of-array-except-self/>
**Level:** 3
<details><summary>Hint</summary>
`result[i]` = product of everything to the left times product of everything to the right.
</details>
<details><summary>Key idea & complexity</summary>
Prefix and suffix products without division, `O(n)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same idea as a prefix sum, but for the product: `result[i]` is the product of all the other elements, i.e. the prefix product of the left side times the suffix product of the right side. On the first pass compute `result[i] = product(a[0..i-1])` (prefix product, left to right). On the second pass (right to left) multiply into each `result[i]` the suffix product `product(a[i+1..n-1])`, keeping a running right-side product in a single variable instead of a separate array. `O(n)` time, `O(1)` extra space (not counting the output array). The problem forbids division precisely because zeros would break it — prefix/suffix products work perfectly well with zeros too. Pitfall: forgetting to initialise `result[i] = 1` before multiplying, or using two separate `O(n)` arrays when a single running variable suffices.
</details>

### 01.4.6  Subarray Sum Equals K  ·  LC #560  ·  Medium  ·  array, hash-table, prefix-sum
<https://leetcode.com/problems/subarray-sum-equals-k/>
**Level:** 3
<details><summary>Hint</summary>
At every index ask whether `(currentPrefix - k)` has already been seen in the map, and how many times.
</details>
<details><summary>Key idea & complexity</summary>
Prefix sum + hash map of counts, `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is the unit's most powerful pattern: accumulate a hash map that counts the **number of occurrences** of each prefix-sum value so far, initialised to `{0: 1}` (the empty subarray sums to zero). At every index `i` update the running prefix sum, and add to the answer the count found in the map for the key `prefix - k` — every such earlier prefix sum marks exactly one subarray with sum `k` ending at index `i`. Only after that is the current prefix added to the map. Invariant: the map contains the exact counts of all prefix sums before index `i`, so every hit found corresponds to a unique subarray. `O(n)` time, `O(n)` space — compared with the brute force `O(n^2)` of trying every subarray. Pitfall: the `{0: 1}` initialisation is mandatory because of negative numbers and subarrays starting at index zero.
</details>

### 01.4.7  Continuous Subarray Sum  ·  LC #523  ·  Medium  ·  array, hash-table, math, prefix-sum
<https://leetcode.com/problems/continuous-subarray-sum/>
**Level:** 3
<details><summary>Hint</summary>
Two prefix sums with the same remainder modulo `k` bracket a subarray divisible by `k`.
</details>
<details><summary>Key idea & complexity</summary>
Prefix sum modulo `k` + hash map of remainders, `O(n)` time, `O(k)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
If two prefix sums `P[i]` and `P[j]` (`i < j`) have the same remainder modulo `k`, the subarray `a[i..j-1]` between them sums to a multiple of `k` — because `P[j] - P[i] ≡ 0 (mod k)`. Therefore keep a map from remainder to the **first** index where it was seen (not a count, because you only need some valid pair and the longest possible range). At every index compute the remainder of the running sum and check whether the same remainder was seen at least two indices earlier. The initialisation `{0: -1}` handles the case where the subarray starts at index 0. `O(n)` time, `O(k)` space (there are at most `k` remainders). Pitfall: the problem requires a subarray length of at least 2, so store only the *first* index seen for each remainder and never update it later.
</details>

### 01.4.8  Contiguous Array  ·  LC #525  ·  Medium  ·  array, hash-table, prefix-sum
<https://leetcode.com/problems/contiguous-array/>
**Level:** 3
<details><summary>Hint</summary>
Turn 0 into -1 and find the longest range whose prefix sum is the same at two positions.
</details>
<details><summary>Key idea & complexity</summary>
Prefix sum (+1/-1) + hash map of first indices, `O(n)` time, `O(n)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The balance between zeros and ones becomes a sum problem when every `0` is interpreted as `-1`: a range with an equal number of zeros and ones corresponds exactly to a range whose sum is `0`. Accumulate a hash map from prefix-sum value to the **first** index where it was seen (not a count — we want the longest range, not all of them). If the same prefix sum is seen again at index `j`, the subarray between the earlier index `i` and `j` is balanced, and its length `j - i` is a candidate answer. The initialisation `{0: -1}` covers ranges starting at the beginning. `O(n)` time, `O(n)` space. This is the same transform-and-look-for-a-repeated-prefix idea as `subarray-sum-equals-k`, but instead of a count we want the longest range, so the map stores an index rather than a counter.
</details>

### 01.4.9  Subarray Sums Divisible by K  ·  LC #974  ·  Medium  ·  array, hash-table, prefix-sum
<https://leetcode.com/problems/subarray-sums-divisible-by-k/>
**Level:** 3
<details><summary>Hint</summary>
Same remainder trick as continuous-subarray-sum, but count the pairs instead of just finding one.
</details>
<details><summary>Key idea & complexity</summary>
Prefix sum modulo `k` + hash map of counts, `O(n)` time, `O(k)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Combines the counting idea of `subarray-sum-equals-k` with the remainder idea of `continuous-subarray-sum`: two prefix sums with the same remainder modulo `k` bracket a range whose sum is divisible by `k`. Accumulate a hash map from remainder to its **count** among prefixes seen so far, initialised to `{0: 1}`. At every index compute the current remainder (remember to normalise negative remainders by adding `k` and taking the modulo again, because in many languages `-1 % 5 = -1` rather than `4`) and add to the answer the count found in the map for the same remainder, before updating the map. `O(n)` time, `O(k)` space. Pitfall: normalising the negative remainder is easy to forget and produces wrong map hits.
</details>

---

## Unit 5 — Sorting as preprocessing

### 01.5.1  Relative Sort Array  ·  LC #1122  ·  Easy  ·  array, hash-table, sorting, counting-sort
<https://leetcode.com/problems/relative-sort-array/>
**Level:** 2
<details><summary>Hint</summary>
Count the frequencies in `arr1`, output in `arr2`'s order, then the rest sorted.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map + custom order (counting sort), `O(n+m)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
First count every number's occurrences in `arr1` with a hash map. Then walk `arr2` in order and output each number as many times as its counter says, removing them from the map as you go. Finally, the remaining numbers (those not in `arr2`) are sorted normally and appended. This is a combination of preprocessing and counting: `arr2` acts as a custom sort key, and the frequency map turns the output of repeated numbers into an `O(1)` lookup per number. `O(n+m)` time (`n` = `arr1`, `m` = `arr2`) plus `O(k log k)` for the `k` leftover numbers. Pitfall: `arr1` may contain duplicates; a counter (not membership) is mandatory for the output count.
</details>

### 01.5.2  Merge Intervals  ·  LC #56  ·  Medium  ·  array, sorting, quicksort
<https://leetcode.com/problems/merge-intervals/>
**Level:** 3
<details><summary>Hint</summary>
Once intervals are sorted by start time, overlap only shows up between neighbours.
</details>
<details><summary>Key idea & complexity</summary>
Sort by start time, merge adjacent, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the intervals by start time, `O(n log n)`. After that a single pass suffices: keep a "current merged interval" as a variable, and for each following interval check whether it starts before or at the same time as the current interval's end — if yes, extend the current interval (`end = max(end, next.end)`); if not, close the current interval into the result and start a new one. This works *only* because sorting guarantees that no later interval can start earlier than the ones already processed — overlap can therefore only appear between adjacent intervals, not distant ones. `O(n log n)` time in total (sorting dominates), `O(n)` space for the result. Pitfall: the new interval's end is not always greater than the current one's — a fully nested interval requires a `max` comparison, not a direct overwrite.
</details>

### 01.5.3  Insert Interval  ·  LC #57  ·  Medium  ·  array
<https://leetcode.com/problems/insert-interval/>
**Level:** 3
<details><summary>Hint</summary>
The input is already sorted — add before the overlap, merge the overlapping ones, add the rest.
</details>
<details><summary>Key idea & complexity</summary>
Three phases without re-sorting, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because the input intervals are *already* sorted and disjoint, no re-sorting is needed — this is a special case of `merge-intervals` where only one new interval is added. Three phases: (1) copy all intervals that end before the new interval's start into the result as they are; (2) merge all intervals that overlap the new interval into one extended interval (`start = min`, `end = max` at each overlapping one); (3) copy the remaining intervals that start after the new (merged) interval has ended. `O(n)` time in a single pass, because the order is already in place and no `O(n log n)` phase is needed. Pitfall: the overlap condition is `interval.start <= newEnd`, not a strict inequality — adjacent intervals (e.g. `[1,2]` and `[2,3]`) are often wrongly treated as separate.
</details>

### 01.5.4  H-Index  ·  LC #274  ·  Medium  ·  array, sorting, counting-sort
<https://leetcode.com/problems/h-index/>
**Level:** 3
<details><summary>Hint</summary>
Sort citations in descending order and find the largest `i` with `citations[i] >= i+1`.
</details>
<details><summary>Key idea & complexity</summary>
Sort descending + find the threshold, `O(n log n)` time (or `O(n)` with counting sort).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Sort the citations in descending order. The h-index is the largest `h` for which at least `h` papers have received at least `h` citations — in the sorted array this is visible directly: find the largest index `i` (0-based) for which `citations[i] >= i+1`; the answer is `i+1`. Sorting reveals a monotone structure thanks to which the condition can be checked in a single pass instead of trying every possible `h` value separately. `O(n log n)` time because of sorting; since the number of citations is bounded to `[0, n]`, counting-sort bucketing (as in `top-k-frequent-elements`) brings this down to `O(n)`. Pitfall: the h-index is not the same as the maximum citation count — it is a balance between the number of papers and the citations.
</details>

### 01.5.5  Non-overlapping Intervals  ·  LC #435  ·  Medium  ·  array, dynamic-programming, greedy, sorting
<https://leetcode.com/problems/non-overlapping-intervals/>
**Level:** 3
<details><summary>Hint</summary>
Same greedy principle as event selection: sort by end time.
</details>
<details><summary>Key idea & complexity</summary>
Sort by end time, greedy selection, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The question is the minimum number of intervals to remove so that the rest do not overlap — the inverse formulation of the classic "pick as many non-overlapping events as possible" greedy (see the greedy algorithms of the `ioi/algoritmit` section). Sort the intervals by **end time** ascending. Walk through them keeping the end time of the last selected interval; if the next interval starts before that, it overlaps and is counted as removed (not selected, end time not updated); otherwise it is selected and the end time is updated. The earliest-ending interval always leaves the most room for later choices — the same exchange argument as in event selection proves this optimal. `O(n log n)` time. Pitfall: sorting by start time gives the wrong answer — end time is the only correct key.
</details>

### 01.5.6  Minimum Number of Arrows to Burst Balloons  ·  LC #452  ·  Medium  ·  array, greedy, sorting
<https://leetcode.com/problems/minimum-number-of-arrows-to-burst-balloons/>
**Level:** 3
<details><summary>Hint</summary>
Same template as non-overlapping-intervals: sort by end and count the covering arrows.
</details>
<details><summary>Key idea & complexity</summary>
Sort by end time, greedy coverage, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same greedy skeleton as `non-overlapping-intervals`, but interpreted as coverage rather than removal: sort the balloons by end point (`xEnd`). Shoot the first arrow at the end point of the first balloon — it bursts every balloon whose start point is at most that position. When the next balloon starts only after the arrow's position, a new arrow is needed, and its position is updated to the new balloon's end point. Why the end point rather than the start: shooting at the earliest-ending point maximises how many later, partially overlapping balloons are caught by the same shot. `O(n log n)` time because of sorting, `O(1)` extra space. Pitfall: the boundary is `<=`, not `<` — a balloon that starts exactly at the arrow's position still bursts.
</details>

### 01.5.7  Largest Number  ·  LC #179  ·  Medium  ·  array, string, greedy, sorting
<https://leetcode.com/problems/largest-number/>
**Level:** 3
<details><summary>Hint</summary>
Compare two numbers as strings concatenated both ways, not numerically.
</details>
<details><summary>Key idea & complexity</summary>
Custom sort with the `a+b` vs `b+a` comparison, `O(n log n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Ordinary numeric or lexicographic ordering does not produce the largest concatenated number (e.g. "9" > "30" numerically, but "9" before "34" lexicographically gives "934" and does not directly settle the matter). The correct sort key is the **pairwise comparison**: number `a` goes before `b` if the string concatenation `a+b > b+a` (as a string comparison). This custom comparison function is transitive (provable, though not trivially), so it qualifies as a proper ordering criterion. Sort the whole array with this comparator and join the result into a single string. `O(n log n · k)` time (`k` = average string length for the comparisons), `O(n)` space. Pitfall: all zeros (e.g. `[0,0]`) produce "00", which must be normalised to "0" as a special case.
</details>

### 01.5.8  Task Scheduler  ·  LC #621  ·  Medium  ·  array, hash-table, greedy, sorting
<https://leetcode.com/problems/task-scheduler/>
**Level:** 3
<details><summary>Hint</summary>
Sort tasks by frequency and count the idle slots around the most frequent task.
</details>
<details><summary>Key idea & complexity</summary>
Frequency map + sort + formula for the cooldown, `O(n)` time.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
First count every task type's frequency with a hash map, and find (implicitly by sorting by frequency) the largest frequency `f_max` and how many tasks share it. The most frequent task dictates the structure: it needs `f_max - 1` cooldown gaps of length `n+1` each (cooldown plus the execution itself), and the last execution is appended without a cooldown. The formula `(f_max - 1) * (n + 1) + (number of tasks sharing f_max)` gives a lower bound; if there are more tasks in total than this formula (no idling is needed, everything fits without wasted time), the answer is simply the total number of tasks. `O(n)` time for the counting (the alphabet is fixed, 26 letters). Pitfall: the answer is `max(formula, totalTasks)`, not the formula directly — many forget the latter case.
</details>

---

## Unit 6 — Matrix & array manipulation

### 01.6.1  Transpose Matrix  ·  LC #867  ·  Easy  ·  array, matrix, simulation
<https://leetcode.com/problems/transpose-matrix/>
**Level:** 2
<details><summary>Hint</summary>
Row `j`, column `i` of the new matrix is row `i`, column `j` of the old one.
</details>
<details><summary>Key idea & complexity</summary>
Direct index swap `(i,j) -> (j,i)`, `O(mn)` time, `O(mn)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Transposition is by definition `result[j][i] = matrix[i][j]` for every `(i,j)` pair. Because the matrix is generally not square (the number of rows and columns can differ), the result matrix's dimensions are the reverse of the original — worth remembering before allocating the output array. A double loop visits every element once and copies it to its new place; there are no dependencies between elements, so the order does not matter. `O(mn)` time and space. This is the base case of the whole matrix unit, on which `rotate-image` and many other problems rest conceptually — once you understand transposition, a 90-degree rotation is just transposition plus a mirror. Pitfall: if you try to transpose a square matrix in place, do not overwrite values you have not read yet — this only works when you swap the pairs `(i,j)` and `(j,i)` together, for `i < j`.
</details>

### 01.6.2  Toeplitz Matrix  ·  LC #766  ·  Easy  ·  array, matrix
<https://leetcode.com/problems/toeplitz-matrix/>
**Level:** 2
<details><summary>Hint</summary>
A diagonal is constant exactly when `matrix[i][j] == matrix[i-1][j-1]` everywhere.
</details>
<details><summary>Key idea & complexity</summary>
Compare every cell with its upper-left neighbour, `O(mn)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A matrix is Toeplitz if every diagonal descending from left to right is constant. It suffices to check a local condition over the whole matrix: for every cell `(i,j)` with `i > 0` and `j > 0`, we must have `matrix[i][j] == matrix[i-1][j-1]`. This local check is enough as a global proof, because every diagonal is a chain of adjacent `(i-1,j-1) -> (i,j)` links — if all adjacent pairs match, the whole diagonal is constant by transitivity. `O(mn)` time, `O(1)` extra space, because the whole matrix need not be held in memory at once (useful if the matrix is read as a stream). Pitfall: the check must be done for both indices `i > 0` *and* `j > 0` simultaneously; a mere row or column comparison is not enough.
</details>

### 01.6.3  Matrix Diagonal Sum  ·  LC #1572  ·  Easy  ·  array, matrix
<https://leetcode.com/problems/matrix-diagonal-sum/>
**Level:** 2
<details><summary>Hint</summary>
On the main diagonal `i == j`, on the anti-diagonal `i + j == n-1` — beware of double-counting the centre cell.
</details>
<details><summary>Key idea & complexity</summary>
Index formula for both diagonals, `O(n)` time, `O(1)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
In a square `n x n` matrix the main diagonal is `matrix[i][i]` and the anti-diagonal is `matrix[i][n-1-i]`. One loop `i = 0..n-1` sums both at once. The only subtlety: when `n` is odd, the centre cell (`i == n-1-i`, i.e. `i = (n-1)/2`) belongs to both diagonals but is counted only **once** by the problem's definition — check `i != n-1-i` before adding the anti-diagonal term, or subtract the centre cell once at the end. `O(n)` time (not `O(n^2)`, because only `2n` cells are visited rather than the whole matrix), `O(1)` space. Pitfall: forgetting the double-count of the centre cell for odd `n` — the most common mistake in this otherwise trivial problem.
</details>

### 01.6.4  Reshape the Matrix  ·  LC #566  ·  Easy  ·  array, matrix, simulation
<https://leetcode.com/problems/reshape-the-matrix/>
**Level:** 2
<details><summary>Hint</summary>
Convert `(i,j)` to a one-dimensional index `i*n + j`, then split it into the new matrix's shape.
</details>
<details><summary>Key idea & complexity</summary>
Flatten to a one-dimensional index and redistribute, `O(mn)` time, `O(rc)` space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
There is a direct formula between the two-dimensional index `(i,j)` and a one-dimensional index: `k = i * numColumns + j` flattens, and conversely `i = k / newNumColumns, j = k % newNumColumns` redistributes. If the number of elements in the original matrix `r*c` does not match the desired shape `m*n`, the reshape is impossible and the original matrix is returned as is. Otherwise, walk all `rc` elements in one logical (flattened) order and place each into its new `(i,j)` position with the formula above. `O(mn)` time and space. This index arithmetic (flatten and split) is useful more generally whenever a 2-D structure must be processed in 1-D order without an extra stack or queue. Pitfall: check that the total element count matches *before* any index calculations.
</details>

### 01.6.5  Rotate Image  ·  LC #48  ·  Medium  ·  array, math, matrix
<https://leetcode.com/problems/rotate-image/>
**Level:** 3
<details><summary>Hint</summary>
A 90-degree clockwise rotation is a transpose followed by reversing every row.
</details>
<details><summary>Key idea & complexity</summary>
Transpose + mirror rows in place, `O(n^2)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A 90-degree clockwise rotation can be decomposed into two simpler operations: first **transpose** the matrix in place (`swap(matrix[i][j], matrix[j][i])` for all `i < j`), then **mirror every row** horizontally (swap column `j` with column `n-1-j`). This can be proven directly with a coordinate transform: the transpose sends `(i,j) -> (j,i)`, and mirroring the row sends that on to `(j, n-1-i)` — exactly the formula for a 90-degree clockwise rotation. Both steps are done in place on the original matrix, so no extra space is needed. `O(n^2)` time (every cell is handled in constant time), `O(1)` extra space. Pitfall: in the transpose you must only iterate over `i < j` (the upper triangle); otherwise you swap every pair twice and restore the original matrix.
</details>

### 01.6.6  Diagonal Traverse  ·  LC #498  ·  Medium  ·  array, matrix, simulation
<https://leetcode.com/problems/diagonal-traverse/>
**Level:** 3
<details><summary>Hint</summary>
On each diagonal `i + j` is constant; the direction alternates from one diagonal to the next.
</details>
<details><summary>Key idea & complexity</summary>
Simulate the diagonals with alternating direction, `O(mn)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
All cells with the same constant `i + j` form one anti-diagonal. Walk the diagonals in order `i + j = 0, 1, 2, ...`; on even diagonals move up-right, on odd ones down-left (or vice versa, depending on the agreed starting direction) — the direction alternates on every diagonal. The edge cases (when the traversal hits the matrix boundary in the middle of a diagonal) need a separate condition that pushes the index onto the next diagonal by stepping to the correct column or row. `O(mn)` time, because every cell is visited exactly once, `O(1)` extra space besides the output array. This is the unit's hardest index-arithmetic exercise: draw a small `3x3` example on paper before coding. Pitfall: the direction change at the boundary is easy to get wrong — test separately at the matrix's top, bottom, left and right edges.
</details>

### 01.6.7  Spiral Matrix  ·  LC #54  ·  Medium  ·  array, matrix, simulation
<https://leetcode.com/problems/spiral-matrix/>
**Level:** 3
<details><summary>Hint</summary>
Keep four boundary indices and shrink them inward on every round.
</details>
<details><summary>Key idea & complexity</summary>
Four shrinking boundaries (top/bottom/left/right), `O(mn)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Keep four boundaries: `top, bottom, left, right`. Walk in order: the top row left to right (`top` increases), the right column top to bottom (`right` decreases), the bottom row right to left (`bottom` decreases), the left column bottom to top (`left` increases) — and repeat until the boundaries cross. After each direction, check whether the boundaries are still valid (`top <= bottom`, `left <= right`), because a narrow (single-row or single-column) remaining area otherwise produces duplicate cells. Invariant: the area inside the boundaries contains exactly the cells not yet visited. `O(mn)` time, because every cell is visited once, `O(1)` extra space. Pitfall: the third and fourth directions (bottom row, left column) must be skipped if the boundaries have already shrunk to a single row or column — otherwise the same cells are output a second time.
</details>

### 01.6.8  Set Matrix Zeroes  ·  LC #73  ·  Medium  ·  array, hash-table, matrix
<https://leetcode.com/problems/set-matrix-zeroes/>
**Level:** 3
<details><summary>Hint</summary>
Use the matrix's first row and first column as memory for which rows/columns to zero.
</details>
<details><summary>Key idea & complexity</summary>
First row/column as marker state, `O(mn)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The naive solution would store separate sets of zero rows and zero columns (`O(m+n)` space). The in-place version uses the matrix's **first row and first column** for the same purpose: when `matrix[i][j] == 0` is found, mark `matrix[i][0] = 0` and `matrix[0][j] = 0` instead of zeroing immediately (zeroing immediately would destroy information needed by later checks). Because row 0 and column 0 could themselves originally contain a zero, their own status is saved in two separate boolean variables before marking. Once the whole matrix has been marked, a second pass zeroes every cell `(i,j)` (`i, j > 0`) whose row or column marker is zero, and finally row 0 / column 0 are handled separately according to the saved boolean flags. `O(mn)` time, `O(1)` extra space. Pitfall: the original state of row 0 and column 0 must be saved *before* the marking phase, otherwise it gets mixed up with the markers.
</details>

### 01.6.9  Game of Life  ·  LC #289  ·  Medium  ·  array, matrix, simulation
<https://leetcode.com/problems/game-of-life/>
**Level:** 3
<details><summary>Hint</summary>
Encode the old and new state in the same cell with more bits, so the neighbour count stays correct.
</details>
<details><summary>Key idea & complexity</summary>
Two-bit state per cell for the in-place update, `O(mn)` time, `O(1)` extra space.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The problem: all cells should update *simultaneously* based on the current (not already updated) neighbour values, but an in-place overwrite would corrupt the neighbour count of later cells. The solution without an extra matrix: encode into every cell **both the old and the new state** with two bits, for example `cell + 2*newState` (value 0-3). The neighbour count uses `value % 2` to always read the original (old) state, regardless of whether the neighbour has already been "updated" under this encoding. Once the whole matrix has been visited and every cell encoded, a second pass shifts every cell to the correct bit (`cell >>= 1`). `O(mn)` time (constant work per cell for the neighbour count), `O(1)` extra space. Pitfall: in the neighbour count of edge cells, check the index bounds carefully so you do not read outside the matrix.
</details>

---

## Progress

- [ ] 01.1.1 Contains Duplicate (LC #217)
- [ ] 01.1.2 Single Number (LC #136)
- [ ] 01.1.3 Missing Number (LC #268)
- [ ] 01.1.4 Intersection of Two Arrays (LC #349)
- [ ] 01.1.5 Set Mismatch (LC #645)
- [ ] 01.1.6 Find All Duplicates in an Array (LC #442)
- [ ] 01.1.7 Longest Consecutive Sequence (LC #128)
- [ ] 01.1.8 First Missing Positive (LC #41)
- [ ] 01.2.1 Valid Anagram (LC #242)
- [ ] 01.2.2 Ransom Note (LC #383)
- [ ] 01.2.3 Jewels and Stones (LC #771)
- [ ] 01.2.4 Majority Element (LC #169)
- [ ] 01.2.5 Unique Number of Occurrences (LC #1207)
- [ ] 01.2.6 Group Anagrams (LC #49)
- [ ] 01.2.7 Top K Frequent Elements (LC #347)
- [ ] 01.2.8 Valid Sudoku (LC #36)
- [ ] 01.3.1 Design HashMap (LC #706)
- [ ] 01.3.2 Two Sum (LC #1)
- [ ] 01.3.3 Isomorphic Strings (LC #205)
- [ ] 01.3.4 Word Pattern (LC #290)
- [ ] 01.3.5 First Unique Character in a String (LC #387)
- [ ] 01.3.6 4Sum II (LC #454)
- [ ] 01.3.7 Sort Characters By Frequency (LC #451)
- [ ] 01.4.1 Running Sum of 1d Array (LC #1480)
- [ ] 01.4.2 Find the Highest Altitude (LC #1732)
- [ ] 01.4.3 Find Pivot Index (LC #724)
- [ ] 01.4.4 Range Sum Query - Immutable (LC #303)
- [ ] 01.4.5 Product of Array Except Self (LC #238)
- [ ] 01.4.6 Subarray Sum Equals K (LC #560)
- [ ] 01.4.7 Continuous Subarray Sum (LC #523)
- [ ] 01.4.8 Contiguous Array (LC #525)
- [ ] 01.4.9 Subarray Sums Divisible by K (LC #974)
- [ ] 01.5.1 Relative Sort Array (LC #1122)
- [ ] 01.5.2 Merge Intervals (LC #56)
- [ ] 01.5.3 Insert Interval (LC #57)
- [ ] 01.5.4 H-Index (LC #274)
- [ ] 01.5.5 Non-overlapping Intervals (LC #435)
- [ ] 01.5.6 Minimum Number of Arrows to Burst Balloons (LC #452)
- [ ] 01.5.7 Largest Number (LC #179)
- [ ] 01.5.8 Task Scheduler (LC #621)
- [ ] 01.6.1 Transpose Matrix (LC #867)
- [ ] 01.6.2 Toeplitz Matrix (LC #766)
- [ ] 01.6.3 Matrix Diagonal Sum (LC #1572)
- [ ] 01.6.4 Reshape the Matrix (LC #566)
- [ ] 01.6.5 Rotate Image (LC #48)
- [ ] 01.6.6 Diagonal Traverse (LC #498)
- [ ] 01.6.7 Spiral Matrix (LC #54)
- [ ] 01.6.8 Set Matrix Zeroes (LC #73)
- [ ] 01.6.9 Game of Life (LC #289)
