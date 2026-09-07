# Chapter 06 — Problems

How to work these: read the unit's section in `lesson.md` first. Attempt each problem in C in `algorithms_learning/06_graphs/solutions/<slug>.c` (create the folder). Write your own tests in `main()` — a disconnected graph, a non-square grid, a single node, source == target — and run with `-fsanitize=address,undefined`. Only when you're stuck or done, open the hint. Only after that, the key idea. The approach is for after you've solved it, or after 30 honest minutes stuck.

Levels 1-5 are the source's difficulty estimate within this chapter, not LeetCode's tag.

---

## Unit 1 — Grid DFS and flood fill

### 06.1.1  Flood Fill  ·  LC #733  ·  Easy  ·  array, depth-first-search, breadth-first-search, matrix
<https://leetcode.com/problems/flood-fill/>
**Level:** 2
<details><summary>Hint</summary>
Visit every connected neighbour of the same colour and change its colour.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS from the start cell, recolour the same-coloured neighbouring region, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Start from the given cell and spread with DFS (or BFS) in four directions as long as the neighbour cell is within bounds and has **the same original colour** as the start cell. The colour is changed the moment a cell is judged valid — this doubles as the visited mark, so no separate `visited` array is needed.

O(nm) time in the worst case, if the whole grid is one colour.

**Pitfall:** if the new colour happens to equal the old colour, the algorithm can loop forever without a separate check — because "change the colour" no longer changes anything and thus no longer prevents reprocessing. Check this special case right at the start.
</details>

### 06.1.2  Island Perimeter  ·  LC #463  ·  Easy  ·  array, depth-first-search, breadth-first-search, matrix
<https://leetcode.com/problems/island-perimeter/>
**Level:** 2
<details><summary>Hint</summary>
The perimeter grows by one for each side of a land cell that borders water or the edge of the grid.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS or direct counting: for every land cell count its water-or-boundary neighbours, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The simplest solution doesn't even need a real search: iterate over all cells, and for every land cell count how many of its four neighbours are either outside the grid or water — add that number to the perimeter. Because the island is connected and has no holes (the problem's assumption), this gives the correct answer directly without any DFS.

An alternative DFS solution traverses the island and counts the same thing recursively — a useful exercise as a bridge to the real component-DFS problems, though not required here.

O(nm) time either way. **Pitfall:** forgetting to handle land cells on the grid boundary specially — a "neighbour" outside the grid always counts as water.
</details>

### 06.1.3  Number of Islands  ·  LC #200  ·  Medium  ·  array, depth-first-search, breadth-first-search, union-find
<https://leetcode.com/problems/number-of-islands/>
**Level:** 3
<details><summary>Hint</summary>
Sweep the whole grid and launch a new search every time you meet an unvisited land cell — the number of launches is the answer.
</details>
<details><summary>Key idea & complexity</summary>
DFS from every not-yet-visited land cell, count the launches, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The classic component count on a grid: loop over all cells, and every time you meet a not-yet-marked land cell, increment the island counter and launch a DFS that marks the **entire** connected island as visited. Subsequent cells of the same island no longer launch a new search, because they're already marked.

Marking happens the moment a cell is found to be land and taken for processing — not after the recursive call returns — so that the same cell doesn't enter the processing queue several times from different neighbours.

O(nm) time, because every cell is visited and marked exactly once. **Pitfall:** forgetting to sweep the whole grid with the outer loop and relying on a single search — that would count only one island even when there are several separate ones.
</details>

### 06.1.4  Max Area of Island  ·  LC #695  ·  Medium  ·  array, depth-first-search, breadth-first-search, union-find
<https://leetcode.com/problems/max-area-of-island/>
**Level:** 3
<details><summary>Hint</summary>
Let DFS return the island's size from the start cell — compare it against the global maximum on every launch.
</details>
<details><summary>Key idea & complexity</summary>
DFS that returns the size of the subtree (part of the island), maintain a global maximum, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Same skeleton as `number-of-islands`, but the DFS no longer returns a mere boolean — it returns a **size**: one for the current cell plus the sums returned by the recursive calls on its four neighbours. At each new island's launch point the resulting size is compared with the global maximum and it is updated if needed.

This is a good example of grid DFS not being only for boolean returns — the return value can carry any accumulating information, here the node count, exactly as when computing the size of a tree.

O(nm) time, because every cell is processed and marked once. **Pitfall:** forgetting to reset or isolate the counter per island — the size must be returned from the recursion, not collected into a shared global variable, otherwise the sizes of different islands get mixed together.
</details>

### 06.1.5  Number of Closed Islands  ·  LC #1254  ·  Medium  ·  array, depth-first-search, breadth-first-search, union-find
<https://leetcode.com/problems/number-of-closed-islands/>
**Level:** 3
<details><summary>Hint</summary>
An island is not closed if it touches the grid border — eliminate the islands reachable from the border before counting.
</details>
<details><summary>Key idea & complexity</summary>
DFS from the borders first to mark "cannot be closed", then count the remaining islands, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A "closed island" is a land area that doesn't touch the grid border at any point. Two-phase solution: (1) run DFS from every border cell that is land and "sink" (mark as water) the whole connected island starting there — these can never be closed, because they touch the border. (2) Run an ordinary `number-of-islands`-style count on the rest of the grid — every remaining island is automatically closed, because the ones touching the border have already been removed.

This order matters: if you tried to check the closedness condition *during* the DFS (e.g. "did we hit the border mid-search"), you'd still have to finish traversing the whole island before you could be sure — separating the two phases is clearer.

O(nm) time for both phases combined.
</details>

### 06.1.6  Surrounded Regions  ·  LC #130  ·  Medium  ·  array, depth-first-search, breadth-first-search, union-find
<https://leetcode.com/problems/surrounded-regions/>
**Level:** 3
<details><summary>Hint</summary>
First flip the O-cells connected to the border to a temporary safe marker, then convert the remaining O's to X and restore the marked ones back to O.
</details>
<details><summary>Key idea & complexity</summary>
DFS from the border's O-cells marks the safe ones, flip the rest to X, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The same "border first" principle as in closed islands, but inverted: a region that **touches the border** is safe (it cannot be captured), so it is marked by DFS with a temporary symbol (e.g. `'#'`) starting from every `'O'` cell on the border.

Then one sweep over the whole grid: the remaining `'O'` cells (which were never connected to the border) are flipped to `'X'`, and the `'#'`-marked ones are restored back to `'O'`.

O(nm) time — three passes over the grid, but each is linear. **Pitfall:** trying to decide "captured or not" directly in a single pass without the temporary mark — border connectivity has to be established *before* the final decision, because an individual cell cannot know whether its region touches the border without the whole region being traversed first.
</details>

### 06.1.7  Pacific Atlantic Water Flow  ·  LC #417  ·  Medium  ·  array, depth-first-search, breadth-first-search, matrix
<https://leetcode.com/problems/pacific-atlantic-water-flow/>
**Level:** 3
<details><summary>Hint</summary>
Run the search in reverse from each ocean inward (only towards non-decreasing height) and take the intersection.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS from both borders in the reverse direction (ascending height), intersect the sets, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A naive solution would simulate the flow of water from every cell to both oceans separately, O(n²m²). The efficient solution **reverses the direction**: instead of asking "where can I get to from this cell", ask "from which cells can one get here" — and because flow only allows a move from higher to lower (or equal), the reverse search only allows a move to **non-decreasing** height.

Run a DFS/BFS separately from all Pacific border cells and from all Atlantic border cells, in both cases only into neighbours whose height is ≥ the current one. The **intersection** of the two resulting sets of reachable cells is the answer.

O(nm) time, because both searches visit each cell at most once. **Pitfall:** forgetting to flip the direction of the inequality in the reverse search — then the search moves the wrong way and no longer corresponds to the original flow condition.
</details>

---

## Unit 2 — Grid BFS and shortest paths in unweighted graphs

### 06.2.1  Shortest Path in Binary Matrix  ·  LC #1091  ·  Medium  ·  array, breadth-first-search, matrix
<https://leetcode.com/problems/shortest-path-in-binary-matrix/>
**Level:** 3
<details><summary>Hint</summary>
The same BFS as on a four-directional grid, but there are eight neighbours including the diagonals.
</details>
<details><summary>Key idea & complexity</summary>
BFS with 8-directional neighbours, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Ordinary grid BFS, but with eight neighbours (diagonals included), since movement is allowed in any of the eight directions. The queue starts from the start cell at distance 1 (or 0, depending on how you count), and each neighbour's distance is the previous +1 when it is first discovered.

The visited mark is set when enqueuing, not when dequeuing — otherwise the same cell can end up in the queue many times from several directions before the first processing marks it.

O(nm) time, because every cell is processed and enqueued at most once. **Pitfall:** forgetting to check that both the start and the target cell are unobstructed (value 0) before starting the search — if either is an obstacle, the answer is −1 immediately.
</details>

### 06.2.2  Rotting Oranges  ·  LC #994  ·  Medium  ·  array, breadth-first-search, matrix
<https://leetcode.com/problems/rotting-oranges/>
**Level:** 3
<details><summary>Hint</summary>
Put all already-rotten oranges into the queue simultaneously before the first round; don't run a separate BFS for each.
</details>
<details><summary>Key idea & complexity</summary>
Multi-source BFS from all rotten oranges simultaneously, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
There are several start points here (all the initially rotten oranges) which spread **simultaneously** every minute — this is exactly the definition of multi-source BFS. All rotten oranges are added to the queue at distance 0 before the loop starts; the BFS proceeds normally, and after each level (minute) you check whether any fresh oranges remain.

The answer is the number of the last processed level if all fresh oranges got rotten; otherwise −1.

O(nm) time. **Pitfall:** running BFS separately from each rotten orange in sequence — this gives a wrong (too large or otherwise incorrect) minute count, because rotting happens in parallel, not one after another. Second pitfall: forgetting the special case where there are no fresh oranges at all to begin with — the answer is then 0.
</details>

### 06.2.3  Nearest Exit from Entrance in Maze  ·  LC #1926  ·  Medium  ·  array, breadth-first-search, matrix
<https://leetcode.com/problems/nearest-exit-from-entrance-in-maze/>
**Level:** 3
<details><summary>Hint</summary>
BFS automatically finds the nearest border cell first — except when that cell happens to be the entrance itself.
</details>
<details><summary>Key idea & complexity</summary>
BFS from the start cell; the first empty border cell (other than the start) is the answer, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Straight BFS from the start cell: every processed cell is checked for whether it lies on the grid border **and** is not the start cell itself (the exit cannot be the entrance). The first such find in BFS order is the nearest exit, because BFS visits cells in order of distance.

If the queue empties without a valid exit being found, the answer is −1.

O(nm) time. **Pitfall:** forgetting to exclude the start cell itself from the exit candidates even though it is on the border — the problem explicitly requires that the entrance does not count as its own exit, which is easy to forget if the check is done purely on coordinates.
</details>

### 06.2.4  01 Matrix  ·  LC #542  ·  Medium  ·  array, dynamic-programming, breadth-first-search, matrix
<https://leetcode.com/problems/01-matrix/>
**Level:** 3
<details><summary>Hint</summary>
Flip the question: instead of looking for the nearest zero for each one separately, run a single BFS from all zeros at once.
</details>
<details><summary>Key idea & complexity</summary>
Multi-source BFS from all zeros simultaneously, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A naive solution would run a BFS from each 1-cell separately to find the nearest 0, O(n²m²) in the worst case. The efficient solution flips the setup: all 0-cells are already at distance 0, so they are all placed into the queue **simultaneously** before the BFS starts — the same multi-source idea as in rotting oranges.

The BFS proceeds normally from this set, and each 1-cell receives its distance the moment it is first discovered — this is guaranteed to be its shortest distance to any 0-cell, because BFS visits nodes in order of distance.

O(nm) time. **Pitfall:** starting the BFS from only one 0-cell assuming that suffices, or enqueueing the 0-cells one after another as separate searches — both break the parallelism of multi-source BFS and give wrong distances.
</details>

### 06.2.5  Shortest Bridge  ·  LC #934  ·  Medium  ·  array, depth-first-search, breadth-first-search, matrix
<https://leetcode.com/problems/shortest-bridge/>
**Level:** 3
<details><summary>Hint</summary>
First mark the entire first island with DFS, then run BFS from all of its border cells at once until you hit the second island.
</details>
<details><summary>Key idea & complexity</summary>
DFS marks the first island, multi-source BFS from its edge to the second island, O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This problem combines the ideas of this unit and the previous one: DFS finds and marks the whole first island (the same way as `number-of-islands`), and **all** cells of this marked island serve as start points for a multi-source BFS that looks for the shortest route to the second island.

The BFS expands over the water level by level, and the first point where it collides with the second island's land gives the answer (the step count minus one, depending on how you count) — because BFS guarantees this is the shortest possible bridge.

O(nm) time for both phases. **Pitfall:** forgetting that the grid can contain more than two islands — the DFS must stop right after the first island (e.g. with a `return` once the first `'1'` cell has been found and its island marked), so that the second island doesn't get mixed into the first one's marking.
</details>

### 06.2.6  Open the Lock  ·  LC #752  ·  Medium  ·  array, hash-table, string, breadth-first-search
<https://leetcode.com/problems/open-the-lock/>
**Level:** 3
<details><summary>Hint</summary>
Think of each four-digit combination as one "node" with eight neighbours (each wheel one step in either direction).
</details>
<details><summary>Key idea & complexity</summary>
BFS in the "state space" of lock combinations (10⁴ states), O(10⁴).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
BFS is not limited to grids — here a "node" is a four-digit string (combination), and every node has eight neighbours: each of the four wheels can be turned one way or the other. Since the question is the **minimum** number of turns needed, we need a shortest path in an unweighted graph — precisely BFS territory.

The obstacles (`deadends`) are handled like already-visited states: they are marked visited before the search, so the BFS never moves into them. The goal is the `target` combination; if the queue empties without finding it, the answer is −1.

The state space has 10⁴ = 10 000 possible combinations, so the BFS is O(10⁴), effectively constant time. **Pitfall:** marking the start combination `"0000"` as visited only later, even though it might itself be listed in `deadends` — this must be checked before the first step.
</details>

### 06.2.7  Word Ladder  ·  LC #127  ·  Hard  ·  hash-table, string, breadth-first-search, bidirectional-search
<https://leetcode.com/problems/word-ladder/>
**Level:** 4
<details><summary>Hint</summary>
Each word is a node; there is an edge between two words if they differ in exactly one letter.
</details>
<details><summary>Key idea & complexity</summary>
BFS between words, neighbour = a one-letter change, O(n · L · 26).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The words form an unweighted graph in which an edge joins two words if they differ in exactly one letter. The question is the shortest path from `beginWord` to `endWord` — exactly what BFS is for.

Neighbour generation is best done by trying each of the 26 letters at every position and checking whether the result is in the word list (a hash set) — this is faster than comparing every word with every other word. A word is removed from the set as soon as it is discovered, which serves as the visited mark.

Time complexity O(n · L · 26), where n is the number of words and L the word length, because L · 26 candidate neighbours are generated from every word. **Pitfall:** building an explicit neighbour graph in advance over all pairs of words — O(n²L), considerably slower than letter-by-letter generation.
</details>

### 06.2.8  Shortest Path Visiting All Nodes  ·  LC #847  ·  Hard  ·  dynamic-programming, bit-manipulation, breadth-first-search, graph
<https://leetcode.com/problems/shortest-path-visiting-all-nodes/>
**Level:** 4
<details><summary>Hint</summary>
Extend the BFS "state" to cover both the current node and the set of already-visited nodes as a bitmask.
</details>
<details><summary>Key idea & complexity</summary>
BFS whose state is (current node, visited nodes as a bitmask), O(n · 2ⁿ).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This extends the BFS concept beyond a grid or a word chain: because **all** nodes must be visited (not just one target reached), "current node" alone is not enough to describe the state — the same node can be visited at different stages with a different visiting history. So the state is the pair `(node, bitmask)`, where the mask tells which nodes have already been visited.

The BFS starts from every node simultaneously (multi-source) with a state in which only that node is marked visited, and proceeds normally to neighbours while updating the mask. The first state in which the mask is all ones (every node visited) gives the answer.

Time complexity O(n · 2ⁿ), because there are n · 2ⁿ states and each expands to O(n) neighbours. **Pitfall:** marking the `(node, mask)` pair as visited incorrectly — the same node with a different mask is a different state, so the visited table has to be two-dimensional.
</details>

---

## Unit 3 — Connected components on general graphs

### 06.3.1  Find if Path Exists in Graph  ·  LC #1971  ·  Easy  ·  depth-first-search, breadth-first-search, union-find, graph
<https://leetcode.com/problems/find-if-path-exists-in-graph/>
**Level:** 2
<details><summary>Hint</summary>
One search from the source node is enough — check whether the destination appears among the visited nodes.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS from the source node, check whether the destination is reached, O(n+m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The simplest component question: are two nodes in the same component? One DFS or BFS from the source is enough — if the destination turns up among the visited nodes, a path exists; otherwise not.

There is no need to compute the components of the whole graph, since only the relationship between two specific nodes is asked.

O(n+m) time, because the search visits every node and edge at most once. **Pitfall:** forgetting the case where source and destination are the same node — this is always true without any search, and it's worth checking separately before the search or making sure the search handles it naturally.
</details>

### 06.3.2  Keys and Rooms  ·  LC #841  ·  Medium  ·  depth-first-search, breadth-first-search, graph
<https://leetcode.com/problems/keys-and-rooms/>
**Level:** 3
<details><summary>Hint</summary>
Treat keys as edges: when you open a room, its keys take you to the neighbouring rooms.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS from room 0, open the keys you find, check whether every room has been visited, O(n+m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Rooms are nodes and the keys inside them are edges to further rooms — so the task is a direct reachability check from room 0: are all rooms in the same component as room 0?

DFS/BFS from room 0: the keys found in each visited room correspond to the neighbours you move to, if they haven't been visited yet. Finally, check whether the number of visited rooms equals the total number of rooms.

O(n+m) time, where m is the total number of keys (edges). **Pitfall:** trying to open a room again every time the same key is found again — the visited mark must be checked before reprocessing, otherwise the same room is visited needlessly many times (this doesn't break correctness, only efficiency; but if the mark is missing entirely you get an infinite loop).
</details>

### 06.3.3  Employee Importance  ·  LC #690  ·  Medium  ·  array, hash-table, tree, depth-first-search
<https://leetcode.com/problems/employee-importance/>
**Level:** 3
<details><summary>Hint</summary>
A tree- or graph-like structure of reporting relationships — sum the employee's own importance and the importance of all subordinates (recursively).
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS from the employee down to subordinates, sum the importances, O(n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Although the structure resembles a tree (manager–subordinate), it is a more general graph, because the problem doesn't guarantee the absence of cycles or multiple paths as explicitly as a binary tree does — so the processing is the same DFS/BFS component formula: start from the given employee, visit all reachable subordinates (direct and indirect), and sum each one's importance value.

First build a hash table id → employee record for fast lookup, because the input is a list, not a ready adjacency list.

O(n) time, because every employee is processed at most once (the organisation structure is acyclic in practice). **Pitfall:** forgetting to add the employee's own importance to the sum, counting only the subordinates' importance.
</details>

### 06.3.4  Clone Graph  ·  LC #133  ·  Medium  ·  hash-table, depth-first-search, breadth-first-search, graph
<https://leetcode.com/problems/clone-graph/>
**Level:** 3
<details><summary>Hint</summary>
Keep a hash table of the copies already created so you don't create the same node twice when you meet it again through a cycle.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS + a hash table original → copy for mapping nodes, O(n+m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Copying a graph requires traversing the component (DFS or BFS from the given node), plus a hash table that maps an original node to its already-created copy — this is essential because the graph can contain cycles: the same node can be encountered many times via different routes, and it must not be re-created every time.

When a node is met for the first time, its copy is created and stored in the table **before** recursing into its neighbours — this prevents infinite recursion in a cyclic graph, because the next encounter with the same node finds it in the table and doesn't recurse again.

O(n+m) time. **Pitfall:** adding the node to the table only after processing its neighbours — then a cycle causes infinite recursion, because the node isn't found in the table when you return to it in the middle of its own processing.
</details>

### 06.3.5  Number of Provinces  ·  LC #547  ·  Medium  ·  depth-first-search, breadth-first-search, union-find, graph
<https://leetcode.com/problems/number-of-provinces/>
**Level:** 3
<details><summary>Hint</summary>
The same component-count formula as with islands, but the input is an adjacency matrix rather than a grid.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS from every not-yet-visited city, count the launches, O(n²) (adjacency matrix).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A straightforward application of the basic formula: loop over all n cities, and launch a new DFS/BFS every time you meet an unvisited city — the number of launches is the number of provinces (components).

The only difference from many other component problems is the representation: the input is an n × n adjacency matrix `isConnected[i][j]`, so iterating over one node's neighbours costs O(n) rather than O(deg(x)) as with an adjacency list — the whole matrix row must be checked.

O(n²) time in total, because every pair of nodes is examined at most once. **Pitfall:** forgetting that the matrix is symmetric (`isConnected[i][j] == isConnected[j][i]`) — this doesn't break the algorithm, but it's good to notice that no extra duplicate work needs to be avoided separately, because the visited mark takes care of it.
</details>

### 06.3.6  Accounts Merge  ·  LC #721  ·  Medium  ·  array, hash-table, string, depth-first-search
<https://leetcode.com/problems/accounts-merge/>
**Level:** 3
<details><summary>Hint</summary>
Create an edge between two emails whenever they appear in the same account, and treat a connected component as one person.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS in the graph of emails (edge = same account), merge each component into one account, O(n log n) including the sorting.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Each email is a node, and an edge joins two emails whenever they appear in the same original account — this implicit graph is first built as a hash table email → neighbouring emails.

Then an ordinary component count: go through every not-yet-visited email, collect its whole component with DFS/BFS (all addresses belonging to the same person), sort them, and attach the person's name.

Time complexity O(n log n), where n is the total number of emails, because building and traversing the graph is O(n) but sorting the output of each component brings in the log factor.

**Pitfall:** merging accounts based on the name alone — the same name can belong to different people, so merging must be based solely on shared emails, not on the name.
</details>

### 06.3.7  Is Graph Bipartite?  ·  LC #785  ·  Medium  ·  depth-first-search, breadth-first-search, union-find, graph
<https://leetcode.com/problems/is-graph-bipartite/>
**Level:** 3
<details><summary>Hint</summary>
Colour the nodes with two alternating colours during the search — if a neighbour is already coloured with the same colour, the graph is not bipartite.
</details>
<details><summary>Key idea & complexity</summary>
DFS/BFS that colours the nodes with two colours, check for a conflict among the neighbours, O(n+m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Bipartiteness means the nodes can be split into two sets so that no edge ever joins two nodes of the same set — this is checked by extending the basic component search with a colouring state: every visited node gets a colour (0 or 1), always the opposite of its neighbour's.

If during the search you meet an already-coloured neighbour whose colour is the **same** as the current node's, the graph is not bipartite. The outer loop goes through every component, because one component may be bipartite while another isn't — the whole graph is bipartite only if all components are.

O(n+m) time. **Pitfall:** forgetting the outer loop and checking only one component — a detached component that is never visited can hide a conflict.
</details>

### 06.3.8  Evaluate Division  ·  LC #399  ·  Medium  ·  array, string, depth-first-search, breadth-first-search
<https://leetcode.com/problems/evaluate-division/>
**Level:** 3
<details><summary>Hint</summary>
Each equation a/b = k is two edges: a→b with weight k and b→a with weight 1/k — a query is the product of the weights along the path from the source to the target.
</details>
<details><summary>Key idea & complexity</summary>
Build a weighted graph from the equations, DFS/BFS multiplying the weights along the path, O(q · (n+m)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This extends the basic component search to the weighted case: the equation a / b = k is interpreted as two directed edges, a → b with weight k and b → a with weight 1/k. The query c / d is a DFS/BFS from the source node c that multiplies the edge weights along the path and stops when the target node d is found.

If c or d is not in the graph at all, or they are not in the same component, the answer is −1.0 — exactly the same "are they in the same component" check as in the other problems of this unit, but with a weight accumulated along the path.

Time complexity O(q · (n+m)), where q is the number of queries, because each query runs its own search. **Pitfall:** forgetting to add the reverse edge b → a with weight 1/k — without it the search would work in one direction only.
</details>

---

## Unit 4 — Cycle detection and topological sort

### 06.4.1  All Paths From Source to Target  ·  LC #797  ·  Medium  ·  backtracking, depth-first-search, breadth-first-search, graph
<https://leetcode.com/problems/all-paths-from-source-to-target/>
**Level:** 3
<details><summary>Hint</summary>
The graph is guaranteed acyclic (a DAG), so ordinary DFS backtracking cannot get stuck in an infinite loop.
</details>
<details><summary>Key idea & complexity</summary>
DFS with backtracking from node 0 to node n−1, collect all paths, O(2ⁿ · n) worst case.
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Because the problem guarantees an acyclic graph (DAG), ordinary DFS backtracking works directly without a separate visited mark — the same node can never lead back to itself, so the recursion always terminates.

At every node the node is appended to the current path list, and if we're at the target node the path is copied to the result; otherwise we recurse into every neighbour. The node is removed from the path (backtrack) after the recursive call, so that sibling branches see the correct path.

In the worst case there can be exponentially many paths, so the time complexity is O(2ⁿ · n) in a dense DAG. This problem is a good bridge to the actual cycle and topological-order problems: the DAG assumption is exactly what makes plain DFS safe without extra mechanisms.
</details>

### 06.4.2  Course Schedule  ·  LC #207  ·  Medium  ·  depth-first-search, breadth-first-search, graph, topological-sort
<https://leetcode.com/problems/course-schedule/>
**Level:** 3
<details><summary>Hint</summary>
The courses can be completed exactly when the dependency graph has no cycle.
</details>
<details><summary>Key idea & complexity</summary>
Cycle detection with DFS (three states) or Kahn's BFS, O(n+m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Course a depends on course b means an edge b → a (or the reverse, depending on the agreed direction) — the question "can all courses be completed" is exactly "does this directed graph have a cycle".

DFS solution: three states per node (not processed / in progress on the current path / done). If the DFS meets a node that is *in progress*, that's a cycle. BFS solution (Kahn): compute in-degrees, enqueue the zero-degree nodes, and if the number of processed nodes at the end is less than n, a cycle exists.

O(n+m) time either way. **Pitfall:** using a plain simple visited flag in the DFS (two states) — this doesn't distinguish "is on my current path" (cycle) from "already fully processed in another branch" (no cycle), so the algorithm reports false cycles.
</details>

### 06.4.3  Course Schedule II  ·  LC #210  ·  Medium  ·  depth-first-search, breadth-first-search, graph, topological-sort
<https://leetcode.com/problems/course-schedule-ii/>
**Level:** 3
<details><summary>Hint</summary>
The same as course-schedule, but return the order itself — not just a boolean saying whether it exists.
</details>
<details><summary>Key idea & complexity</summary>
Topological order via Kahn or DFS post-order, empty list if there's a cycle, O(n+m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
An extension of the previous one: now the topological order itself must be returned, not just whether there is a cycle.

With Kahn's algorithm the order comes directly from the sequence in which nodes are dequeued (BFS-style, starting from the zero-in-degree nodes). With the DFS-based solution the order is assembled in **post-order** (a node is appended to the result only once all its neighbours have been processed) and the list is **reversed** at the end — because post-order produces the reverse topological order.

If a cycle is found midway with either method, return an empty array instead of a complete order.

O(n+m) time. **Pitfall:** forgetting to reverse the DFS post-order result — without the reversal the list is in exactly the opposite order to the topological order wanted.
</details>

### 06.4.4  Course Schedule IV  ·  LC #1462  ·  Medium  ·  depth-first-search, breadth-first-search, graph, topological-sort
<https://leetcode.com/problems/course-schedule-iv/>
**Level:** 3
<details><summary>Hint</summary>
Process the nodes in topological order and collect, for every node, the (transitive) set of all its prerequisites from its predecessors.
</details>
<details><summary>Key idea & complexity</summary>
Transitive closure in topological order (or DFS+memo from every node), O(n · (n+m)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The question "is course u an (indirect) prerequisite of course v" is asked for many queries — this is a transitive-closure question on a DAG. The most efficient approach processes the nodes in topological order and keeps, for every node, a hash set of all its (direct and indirect) prerequisites: when a node is processed, its prerequisite set is the **union** of all its direct predecessors' prerequisite sets plus the predecessors themselves.

This works because in topological order every predecessor is already fully processed (its own prerequisite set is complete) before the node depending on it is processed — the same "children before parent" principle as in post-order tree recursion.

Time complexity O(n · (n+m)) in the worst case due to the size of the sets. **Pitfall:** trying to answer every query with a separate DFS without a precomputed closure — works, but is needlessly slow with repeated queries.
</details>

### 06.4.5  Find Eventual Safe States  ·  LC #802  ·  Medium  ·  depth-first-search, breadth-first-search, graph, topological-sort
<https://leetcode.com/problems/find-eventual-safe-states/>
**Level:** 3
<details><summary>Hint</summary>
A node is safe if and only if you can never reach a cycle from it — the same three-state DFS as in cycle detection, but remember the result for every node.
</details>
<details><summary>Key idea & complexity</summary>
Cycle detection with three-state DFS, safe = does not lead to a cycle, O(n+m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A node is "safe" if every path leaving it eventually ends in a dead end (a node with no outgoing edges) without ever hitting a cycle. This is solved with the same three-state DFS as cycle detection (not processed / in progress / determined safe), but now every node's result is **remembered** by memoisation, because the same node can be asked about many times from different paths.

A node is safe exactly when it is not currently in the in-progress state (which would mean a cycle) and all its neighbours are recursively determined safe.

O(n+m) time thanks to memoisation, even though a query is made for every node. **Pitfall:** forgetting the memoisation and recomputing the same subtree for every node — leads to exponential time in a dense graph.
</details>

### 06.4.6  Minimum Height Trees  ·  LC #310  ·  Medium  ·  depth-first-search, breadth-first-search, graph, topological-sort
<https://leetcode.com/problems/minimum-height-trees/>
**Level:** 3
<details><summary>Hint</summary>
Peel the tree layer by layer from the leaves inward, like Kahn's algorithm but on a tree — the nodes left at the end are the answer.
</details>
<details><summary>Key idea & complexity</summary>
Repeated leaf removal (topological sort from the tree's outside in), 1–2 central nodes remain, O(n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The minimum-height roots of a tree (an acyclic connected graph) are always at the tree's "centre" — and this centre is found with the same idea as Kahn's algorithm in topological sorting, but applied to a tree without direction: start from all leaves (degree 1) and remove them layer by layer, updating the neighbours' degrees, until 1 or 2 nodes remain.

This works because every removal round reduces the distance from the tree's edges by one on all sides — the remaining nodes are the ones whose longest distance to any leaf is smallest.

O(n) time, because every node and edge is processed a constant number of times. **Pitfall:** trying to run BFS/DFS from every node separately to compute the height — works, but is O(n²), needlessly slow compared with the peeling method.
</details>

### 06.4.7  Sort Items by Groups Respecting Dependencies  ·  LC #1203  ·  Hard  ·  depth-first-search, breadth-first-search, graph, topological-sort
<https://leetcode.com/problems/sort-items-by-groups-respecting-dependencies/>
**Level:** 4
<details><summary>Hint</summary>
Do a topological sort twice: first for the order of the groups, then for the order of the items inside each group, and combine the results.
</details>
<details><summary>Key idea & complexity</summary>
Two levels of topological sort: between groups and within each group, O(n+m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The problem requires two nested topological sorts: (1) the mutual order of the groups, based on the dependencies that cross a group boundary; (2) the order of the items inside each individual group, based on the dependencies that stay within the group.

Items that have no group are treated as their own single-item groups, so that both levels can be handled uniformly with the same algorithm (Kahn or DFS post-order).

The final result is built by walking the groups in their topological order and, at each group, appending its internally sorted items to the result. If either level fails (a cycle), the whole answer is an empty list.

O(n+m) time, because both sorting levels are linear. **Pitfall:** forgetting to handle the group-less items as a special case, in which case they vanish entirely from the group-level sort.
</details>

### 06.4.8  Reconstruct Itinerary  ·  LC #332  ·  Hard  ·  array, string, depth-first-search, graph
<https://leetcode.com/problems/reconstruct-itinerary/>
**Level:** 4
<details><summary>Hint</summary>
Consume each departing flight once you've used it, and append an airport to the result only when it has no unused departures left (post-order); reverse at the end.
</details>
<details><summary>Key idea & complexity</summary>
DFS that appends a node to the result only in post-order (Hierholzer's algorithm), reverse at the end, O(m log m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Although the problem doesn't directly ask for a topological order, the solution uses exactly the same mechanics as DFS-based topological sorting: a node (airport) is appended to the final result only in **post-order** — once all the edges leaving it (chosen in alphabetical order) have already been consumed — and the whole list is reversed at the end. This is called Hierholzer's algorithm for finding an Eulerian path.

Every edge (flight) is used exactly once; when no unused outgoing edge can be found from a node, it is "done" and appended to the result — the same "children before parent" logic as in a tree's post-order traversal, but generalised to a graph where edges are consumed.

Time complexity O(m log m) due to sorting the flights alphabetically. **Pitfall:** greedily picking the alphabetically smallest flight at every step without the ability to backtrack — this can leave the remaining flights unused; the post-order append fixes this automatically by backing up to the right place.
</details>

---

## Unit 5 — Union-Find

### 06.5.1  Redundant Connection  ·  LC #684  ·  Medium  ·  depth-first-search, breadth-first-search, union-find, graph
<https://leetcode.com/problems/redundant-connection/>
**Level:** 3
<details><summary>Hint</summary>
Walk the edges in the given order and find the first one whose two endpoints are already in the same component.
</details>
<details><summary>Key idea & complexity</summary>
Union-Find: add the edges in order, return the first one that would join already-equal roots, O(n α(n)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A tree has n−1 edges; the problem's graph has n edges, so exactly one edge closes a cycle. Union-Find finds it directly: walk the edges in the given order, and for every edge `(u, v)` check `find(u) == find(v)` before merging — if the roots are already the same, this edge is the redundant one (it would close a cycle), and it is the answer.

If the roots differ, the edge is merged normally with `union(u, v)`.

O(n α(n)) time thanks to path compression and union by size, practically almost linear.

**Pitfall:** returning the first cycle edge found in the wrong order — the problem specifically requires *the redundant edge that appears last in the list*, so the edges must be processed in exactly the given order, not e.g. sorted.
</details>

### 06.5.2  Satisfiability of Equality Equations  ·  LC #990  ·  Medium  ·  array, string, union-find, graph
<https://leetcode.com/problems/satisfiability-of-equality-equations/>
**Level:** 3
<details><summary>Hint</summary>
Process all the "==" equations first, merging the variables into the same component, and only then check whether any "!=" equation is contradicted.
</details>
<details><summary>Key idea & complexity</summary>
Union-Find: merge the == pairs first, check the != pairs afterwards, O(n α(n)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The two-phase solution is essential here: first go through **all** the `==` equations and merge the variables involved into the same union-find component — this must be done completely before the second phase, because equality is transitive (`a=b`, `b=c` ⇒ `a=c`) and the order must not matter.

In the second phase go through all the `!=` equations and check, for each, whether its two variables are **already** in the same component (`find(a) == find(b)`) — if so, the set of equations is contradictory, because the same variable is claimed both equal and unequal.

O(n α(n)) time. **Pitfall:** mixing up the order of the phases, e.g. checking `!=` conditions before all the `==` merges are done — then some transitive equalities are not yet visible and a contradiction can go unnoticed.
</details>

### 06.5.3  Number of Operations to Make Network Connected  ·  LC #1319  ·  Medium  ·  depth-first-search, breadth-first-search, union-find, graph
<https://leetcode.com/problems/number-of-operations-to-make-network-connected/>
**Level:** 3
<details><summary>Hint</summary>
Every extra cable (one that would join nodes already in the same component) can be moved to join two different components — just count how many are available.
</details>
<details><summary>Key idea & complexity</summary>
Union-Find counts the number of components k, the answer is k−1 if there are enough cables, O(n α(n)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Merge all the given cables with union-find and count the number of components k that result. The minimum number of moves to connect the network is k−1 (the same proof as in the general component-merging problem: each move can reduce the number of components by at most one).

However, this is only possible if there are at least k−1 extra (redundant) cables — the number of extra cables is computed by counting how many `union` calls failed (because both ends were already in the same component). If there are fewer than n−1 cables in total, connecting is impossible purely on the node count, and the answer is −1.

O(n α(n)) time. **Pitfall:** forgetting to check that the total number of cables is sufficient before counting components.
</details>

### 06.5.4  Most Stones Removed with Same Row or Column  ·  LC #947  ·  Medium  ·  hash-table, depth-first-search, union-find, graph
<https://leetcode.com/problems/most-stones-removed-with-same-row-or-column/>
**Level:** 3
<details><summary>Hint</summary>
Merge stones that share a row or a column into the same component — from every component all but one stone can be removed.
</details>
<details><summary>Key idea & complexity</summary>
Union-Find over rows and columns, the answer is n − (number of components), O(n α(n)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Two stones are "connectable" if they share a row or a column — this forms a union-find structure in which every stone is merged into the same component as its row- and column-mates (in practice, e.g. the row index and the column index + offset are merged into the same union-find space, so the stones get linked to each other through them).

From every connected component all stones but one can be removed (the last stone can no longer leave, because removal requires a remaining row/column mate) — so the maximum number of removals is n − (number of components).

O(n α(n)) time. **Pitfall:** trying to merge stones directly in pairs by comparing every pair of stones, O(n²) — the more efficient way is to merge every stone directly with the union-find node representing its row and column number.
</details>

### 06.5.5  Regions Cut By Slashes  ·  LC #959  ·  Medium  ·  array, hash-table, depth-first-search, breadth-first-search
<https://leetcode.com/problems/regions-cut-by-slashes/>
**Level:** 3
<details><summary>Hint</summary>
Split every cell into four triangles (top, bottom, left, right) and merge them according to both the internal routes the slashes allow and the neighbouring cells' adjacent triangles.
</details>
<details><summary>Key idea & complexity</summary>
Union-Find, split every cell into four triangles and merge the neighbours the lines allow, O(n² α(n)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Ordinary grid DFS doesn't work directly, because the `/` and `\` lines split a single cell into two separate regions — so a cell cannot be treated as one union-find node. The solution splits every cell into **four triangles** (top, bottom, left, right) as their own union-find nodes.

Inside a cell the triangles are merged depending on which line (or empty cell) is present: an empty cell merges all four; `/` merges the top–left and bottom–right pairs separately; `\` merges the top–right and bottom–left pairs separately. Between cells the adjacent triangles are always merged (e.g. this cell's right triangle with the next cell's left triangle).

The answer is the final number of components in the whole 4n²-node union-find structure. O(n² α(n)) time. **Pitfall:** forgetting to merge the triangles between cells, in which case every cell remains its own separate islet.
</details>

### 06.5.6  Min Cost to Connect All Points  ·  LC #1584  ·  Medium  ·  array, union-find, graph, minimum-spanning-tree
<https://leetcode.com/problems/min-cost-to-connect-all-points/>
**Level:** 3
<details><summary>Hint</summary>
Build all O(n²) possible edges (Manhattan distances), sort by weight, and use union-find to add only those that don't close a cycle.
</details>
<details><summary>Key idea & complexity</summary>
Kruskal's algorithm: sort the edges by weight, union-find prunes the cycles, O(n² log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A minimum spanning tree (MST) connects all the nodes with the smallest possible total weight using exactly n−1 edges. Kruskal's algorithm builds it greedily: sort all possible edges (here O(n²) pairs, weight = Manhattan distance) in ascending weight order, and walk through them, adding with union-find every edge that would **not** join nodes already in the same component — this is exactly the same cycle check as in `redundant-connection`, but now the accepted edges are collected along with their weights.

The algorithm stops when n−1 edges have been accepted (all nodes in one component).

Time complexity O(n² log n), dominated by sorting all the pairs. **Pitfall:** forgetting that the greedy choice (always the smallest remaining edge) works only because union-find prevents cycles — without it, greed would not guarantee a tree.
</details>

### 06.5.7  Swim in Rising Water  ·  LC #778  ·  Hard  ·  array, binary-search, depth-first-search, breadth-first-search
<https://leetcode.com/problems/swim-in-rising-water/>
**Level:** 4
<details><summary>Hint</summary>
Add the grid's edges (cell–neighbour pairs) to union-find in ascending height order, and stop as soon as the start and target cells are in the same component.
</details>
<details><summary>Key idea & complexity</summary>
Union-Find over edges in ascending height order (Kruskal-style), stop when start and target merge, O(n² log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The time asked for is the smallest height at which you can cross the grid using only cells of that height or lower — this is a Kruskal-style union-find application: process the grid's cells (or the edges between neighbours) in **ascending height order** and merge with union-find whenever both neighbouring cells are already "under water" (processed).

The answer is the height at which the start cell (0,0) and the target cell (n−1,n−1) first end up in the same component — the algorithm can stop right at that moment.

An alternative solution uses binary search over the height combined with BFS/DFS at each trial height; the union-find version is more direct because it doesn't need the binary search's outer loop.

O(n² log n) time for sorting the cells. **Pitfall:** forgetting that the start cell (0,0) itself also sets a lower bound on the answer — the answer cannot be smaller than `grid[0][0]`.
</details>

---

## Unit 6 — Dijkstra and weighted shortest paths

### 06.6.1  Network Delay Time  ·  LC #743  ·  Medium  ·  depth-first-search, breadth-first-search, graph, heap-priority-queue
<https://leetcode.com/problems/network-delay-time/>
**Level:** 3
<details><summary>Hint</summary>
Run ordinary Dijkstra from the source node and check at the end whether every node reached a finite distance.
</details>
<details><summary>Key idea & complexity</summary>
Dijkstra from the source node, the answer is the maximum of all reached distances, O((n+m) log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The basic use case for Dijkstra: run the algorithm from the source node `k`, and the answer is the **maximum** of all nodes' final distances — because the signal has "arrived" only once every node has received it, and the slowest route determines the total time.

If some node remains unreachable (its distance stays infinite), the answer is −1, because the network is not connected from the source node.

The priority queue ensures that every node is processed at its final (smallest) distance exactly once; stale, worse queue entries are skipped when they are popped.

O((n+m) log n) time with a binary heap. **Pitfall:** forgetting to check on pop whether the node has already been processed with a better distance — without this check the same node can be needlessly processed many times (correctness is preserved, but efficiency suffers).
</details>

### 06.6.2  Path with Maximum Probability  ·  LC #1514  ·  Medium  ·  array, graph, heap-priority-queue, shortest-path
<https://leetcode.com/problems/path-with-maximum-probability/>
**Level:** 3
<details><summary>Hint</summary>
Flip Dijkstra's direction: instead of minimising a sum you maximise a product — use a max-heap instead of a min-heap.
</details>
<details><summary>Key idea & complexity</summary>
Dijkstra where "distance" is the product of probabilities, maximise instead of minimise, O((n+m) log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
A variant of basic Dijkstra: instead of minimising the sum of weights, maximise the **product** of the weights (probabilities) — but the algorithm's skeleton stays the same, because the greedy argument works equally well from either direction: the best known value for each node is final as soon as it is popped from the priority queue in the correct (now reversed) order.

The priority queue is ordered in descending probability (or a min-heap is used with negated/inverted values), and the relaxation is `dist[u] * w > dist[v]` instead of a sum.

This substitution works because probabilities are always in [0,1] (non-negative), just as Dijkstra requires non-negative weights.

O((n+m) log n) time. **Pitfall:** using an ordinary min-heap without flipping the sign, so the algorithm would find the *smallest* probability path rather than the largest.
</details>

### 06.6.3  Path With Minimum Effort  ·  LC #1631  ·  Medium  ·  array, binary-search, depth-first-search, breadth-first-search
<https://leetcode.com/problems/path-with-minimum-effort/>
**Level:** 3
<details><summary>Hint</summary>
Minimise the worst single step on the route (the maximum edge weight on the path), not the sum of the steps — the same Dijkstra skeleton with a different combining operation.
</details>
<details><summary>Key idea & complexity</summary>
Dijkstra where "distance" is the largest single height difference on the route, O(nm log(nm)).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Another variant: an edge's "cost" on a path is not a sum but **the largest single step on the route** — the goal is to minimise this worst step across the whole route, not the total distance. Dijkstra's skeleton stays the same, but the relaxation changes: the new candidate value for node v is `max(dist[u], |height(u) − height(v)|)` instead of a sum, and the update is made if this is smaller than the current `dist[v]`.

The grid is treated as a graph in which every cell is a node and the four neighbours give edges weighted by their height difference.

The greedy argument works for the same reason as in ordinary Dijkstra: once a node is popped from the priority queue with the smallest "worst step" value, it can no longer be improved later, because any other route would pass through an already-worse known node.

O(nm log(nm)) time. **Pitfall:** summing the height differences as in ordinary Dijkstra — that would answer a different question (total climb, not the worst single step).
</details>

### 06.6.4  Cheapest Flights Within K Stops  ·  LC #787  ·  Medium  ·  dynamic-programming, depth-first-search, breadth-first-search, graph
<https://leetcode.com/problems/cheapest-flights-within-k-stops/>
**Level:** 3
<details><summary>Hint</summary>
Ordinary Dijkstra stops too early because it doesn't know how many stops have been used on the route — limit the number of relaxation rounds to k+1, or make the stop count part of the state.
</details>
<details><summary>Key idea & complexity</summary>
Bounded Bellman-Ford (k+1 relaxation rounds) or Dijkstra whose state includes the number of stops, O(k · m).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is a classic pitfall problem precisely because ordinary Dijkstra **fails** here: it marks a node final as soon as it is first popped from the queue, but the cheapest route whose stop count doesn't exceed k may well pass through a more expensive intermediate stage if that saves stops later.

Two working fixes: (1) Bellman-Ford-style bounded relaxation, exactly k+1 rounds over the whole edge list, so that each round corresponds to one more stop and routes with more stops can never contribute; (2) Dijkstra whose state is `(node, stops used)` rather than just the node, so the same node can be processed again with different stop counts.

O(k · m) time with the Bellman-Ford version. **Pitfall:** using ordinary Dijkstra directly, marking nodes final on their first visit — this misses the cheaper routes that stay within the stop limit.
</details>

### 06.6.5  Number of Ways to Arrive at Destination  ·  LC #1976  ·  Medium  ·  dynamic-programming, graph, topological-sort, shortest-path
<https://leetcode.com/problems/number-of-ways-to-arrive-at-destination/>
**Level:** 3
<details><summary>Hint</summary>
Extend Dijkstra to count, for every node, how many different ways reach its best (shortest) distance.
</details>
<details><summary>Key idea & complexity</summary>
Dijkstra that also counts the number of routes for every distance, O((n+m) log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
Basic Dijkstra is extended to keep track not only of the shortest distance but also of **how many different routes** achieve that same shortest distance. When an edge is relaxed and a **strictly better** distance is found for node v, its route counter is replaced by node u's counter; when an **equally good** distance is found along a different route, the counters are **added** together instead of one being ignored.

This works because Dijkstra's guarantee (a node is final when popped from the queue) concerns the distance, but the route counter accumulates correctly as long as all routes achieving the shortest distance are processed before the node's final pop from the queue.

O((n+m) log n) time. **Pitfall:** forgetting to reset the counter when a strictly better (not just equally good) distance is found — otherwise the old counters, corresponding to a worse route, are wrongly left in the sum.
</details>

### 06.6.6  Find the City With the Smallest Number of Neighbors at a Threshold Distance  ·  LC #1334  ·  Medium  ·  dynamic-programming, graph, shortest-path, dijkstra
<https://leetcode.com/problems/find-the-city-with-the-smallest-number-of-neighbors-at-a-threshold-distance/>
**Level:** 3
<details><summary>Hint</summary>
Run a shortest-path search from every city separately and count how many other cities lie within the threshold distance.
</details>
<details><summary>Key idea & complexity</summary>
Dijkstra (or Floyd-Warshall) from every node, count the neighbours within the threshold, O(n · (n+m) log n).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
The distances between all pairs of cities are needed, so Dijkstra is run with **every** node as the source (n times), or alternatively Floyd-Warshall is used in a single O(n³) run when n is small.

For each city, count how many other cities are at distance ≤ the threshold, and pick the city for which this count is smallest — in a tie, the city with the largest index wins (the problem's special condition).

n runs of Dijkstra give O(n · (n+m) log n), which is better than Floyd-Warshall on a sparse graph but worse on a dense one with small n — the choice depends on the graph's density.

**Pitfall:** forgetting the tie-break rule (largest city index wins) and returning the wrong city when several achieve the same minimum count.
</details>

### 06.6.7  Minimum Obstacle Removal to Reach Corner  ·  LC #2290  ·  Hard  ·  array, breadth-first-search, graph, heap-priority-queue
<https://leetcode.com/problems/minimum-obstacle-removal-to-reach-corner/>
**Level:** 4
<details><summary>Hint</summary>
The edge weights are only 0 or 1 — instead of an ordinary priority queue use a double-ended queue (deque): weight-0 moves to the front, weight-1 moves to the back.
</details>
<details><summary>Key idea & complexity</summary>
0-1 BFS (deque) or Dijkstra, weights 0 (free) and 1 (obstacle), O(nm).
</details>
<details><summary>Approach (read after solving or after 30 min stuck)</summary>
This is a special case of Dijkstra in which every weight is either 0 (a free cell) or 1 (an obstacle to remove) — then a full priority queue is needless overhead, and the more efficient solution is **0-1 BFS**: use a double-ended queue (deque), push weight-0 moves to the **front** of the queue and weight-1 moves to the **back**. This keeps the queue ordered by non-decreasing distance without a heap.

The same relaxation principle as in Dijkstra: a node's distance is final when it is first popped from the front of the queue and processed.

O(nm) time, because every cell and edge is processed a constant number of times — faster than general Dijkstra's O(nm log(nm)), because deque operations are O(1) compared with the heap's O(log n).

**Pitfall:** using ordinary BFS without realising that removing an obstacle costs "more" than a free move — ordinary BFS doesn't distinguish weights, so it would give the wrong answer.
</details>

---

## Progress

- [ ] 06.1.1 Flood Fill (LC #733)
- [ ] 06.1.2 Island Perimeter (LC #463)
- [ ] 06.1.3 Number of Islands (LC #200)
- [ ] 06.1.4 Max Area of Island (LC #695)
- [ ] 06.1.5 Number of Closed Islands (LC #1254)
- [ ] 06.1.6 Surrounded Regions (LC #130)
- [ ] 06.1.7 Pacific Atlantic Water Flow (LC #417)
- [ ] 06.2.1 Shortest Path in Binary Matrix (LC #1091)
- [ ] 06.2.2 Rotting Oranges (LC #994)
- [ ] 06.2.3 Nearest Exit from Entrance in Maze (LC #1926)
- [ ] 06.2.4 01 Matrix (LC #542)
- [ ] 06.2.5 Shortest Bridge (LC #934)
- [ ] 06.2.6 Open the Lock (LC #752)
- [ ] 06.2.7 Word Ladder (LC #127)
- [ ] 06.2.8 Shortest Path Visiting All Nodes (LC #847)
- [ ] 06.3.1 Find if Path Exists in Graph (LC #1971)
- [ ] 06.3.2 Keys and Rooms (LC #841)
- [ ] 06.3.3 Employee Importance (LC #690)
- [ ] 06.3.4 Clone Graph (LC #133)
- [ ] 06.3.5 Number of Provinces (LC #547)
- [ ] 06.3.6 Accounts Merge (LC #721)
- [ ] 06.3.7 Is Graph Bipartite? (LC #785)
- [ ] 06.3.8 Evaluate Division (LC #399)
- [ ] 06.4.1 All Paths From Source to Target (LC #797)
- [ ] 06.4.2 Course Schedule (LC #207)
- [ ] 06.4.3 Course Schedule II (LC #210)
- [ ] 06.4.4 Course Schedule IV (LC #1462)
- [ ] 06.4.5 Find Eventual Safe States (LC #802)
- [ ] 06.4.6 Minimum Height Trees (LC #310)
- [ ] 06.4.7 Sort Items by Groups Respecting Dependencies (LC #1203)
- [ ] 06.4.8 Reconstruct Itinerary (LC #332)
- [ ] 06.5.1 Redundant Connection (LC #684)
- [ ] 06.5.2 Satisfiability of Equality Equations (LC #990)
- [ ] 06.5.3 Number of Operations to Make Network Connected (LC #1319)
- [ ] 06.5.4 Most Stones Removed with Same Row or Column (LC #947)
- [ ] 06.5.5 Regions Cut By Slashes (LC #959)
- [ ] 06.5.6 Min Cost to Connect All Points (LC #1584)
- [ ] 06.5.7 Swim in Rising Water (LC #778)
- [ ] 06.6.1 Network Delay Time (LC #743)
- [ ] 06.6.2 Path with Maximum Probability (LC #1514)
- [ ] 06.6.3 Path With Minimum Effort (LC #1631)
- [ ] 06.6.4 Cheapest Flights Within K Stops (LC #787)
- [ ] 06.6.5 Number of Ways to Arrive at Destination (LC #1976)
- [ ] 06.6.6 Find the City With the Smallest Number of Neighbors at a Threshold Distance (LC #1334)
- [ ] 06.6.7 Minimum Obstacle Removal to Reach Corner (LC #2290)
