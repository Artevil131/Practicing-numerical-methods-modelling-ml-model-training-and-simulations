# Chapter 17 — Exercises

Write each exercise as `ex17_K.c` in this folder (`ex17_1.c`, `ex17_2.c`, ...). Compile with
`cc -Wall -Wextra -std=c11 -O2 -o ex17_K ex17_K.c -lm`. Zero warnings. Start every file with
`#define _POSIX_C_SOURCE 200809L` so it also builds on Linux. When something hangs, run it under
`sudo dtruss -f ./ex17_K` (Linux: `strace -f`) and look at the last syscall — that's where it's
blocked. Check **every** return value; `errno == EINTR` is a retry, not an error.

---

### 17.1 — **Count the syscalls**

Write a program that writes the numbers 1..10000, one per line, to a file three ways: (a) `fprintf` through a `FILE*`, (b) `snprintf` into a small buffer then `write(2)` per line, (c) `snprintf` into a 64 KiB buffer and one `write_all` at the end. Time each with `CLOCK_MONOTONIC`. Then run each variant under `dtruss`/`strace -c` and paste the number of `write` syscalls into a comment next to the timing. Print `sysconf(_SC_PAGESIZE)` and the stdio buffer size you infer from (a)'s syscall count.

Example:
```
(a) fprintf        :   0.41 ms   12 write() calls
(b) write per line :   2.30 ms   10000 write() calls
(c) one write_all  :   0.19 ms   1 write() call
page size 16384
```

<details><summary>Hint</summary>
Your `write_all` must loop on partial writes and `EINTR`. For (a), 48894 bytes / 12 calls ≈ 4 KiB suggests the stdio buffer size; `BUFSIZ` in `<stdio.h>` confirms it. `dtruss` needs `sudo`; if it isn't allowed, `fs_usage -w -f filesys` works too.
</details>

### 17.2 — **`cat` and `cp` with raw fds**

Implement `ex17_2 src dst` that copies `src` to `dst` using only `open/read/write/close` (no stdio), a 64 KiB buffer, `read_all`/`write_all`-style loops, and creates `dst` with the same permission bits as `src` (`fstat` + the `mode` argument to `open`). Refuse to overwrite an existing `dst` (`O_EXCL`). Print the byte count. Verify with `cmp src dst`. Then make `dst` optional: with one argument, copy to stdout (`STDOUT_FILENO`) — that's `cat`.

Example:
```
$ ./ex17_2 example.c /tmp/copy.c
copied 22652 bytes
$ ./ex17_2 example.c /tmp/copy.c
open /tmp/copy.c: File exists
```

<details><summary>Hint</summary>
`write` to a pipe (`./ex17_2 f | head -1`) will return partial counts and eventually `EPIPE` — handle both without crashing (`signal(SIGPIPE, SIG_IGN)` then treat `EPIPE` as "stop"). `st.st_mode & 0777` for the permission bits.
</details>

### 17.3 — **Directory walker with sizes**

Write `ex17_3 [-d depth] path` that recursively lists regular files under `path` with their sizes, skipping symlinks (`lstat`, `S_ISLNK`), never descending past `depth` levels (default unlimited), and prints a total at the end. Use `getopt` for `-d`. Sort each directory's entries alphabetically before printing (`readdir` order is not sorted). Every `snprintf` that builds a path must check for truncation.

Example:
```
$ ./ex17_3 -d 1 ..
     22652 ../17_posix_systems_programming/example.c
     47741 ../17_posix_systems_programming/lesson.md
...
total 1204513 bytes in 61 files
```

<details><summary>Hint</summary>
Collect `d_name`s into a dynamic array (chapter 10), `qsort` with `strcmp`, then process. Set `errno = 0` before `readdir` and check it after the loop to tell EOF from error. `PATH_MAX` is in `<limits.h>`.
</details>

### 17.4 — **A shell that does `a | b | c`**

Write a minimal shell: read a line from stdin, split it on `|` then on spaces (no quoting needed), and run the pipeline with one `fork` per stage, `pipe`/`dup2` between stages, `execvp` for each, then `waitpid` all children and print each exit status. Support `exit`. Must not hang on `ls | wc -l` or `cat /dev/urandom | head -c 10 | wc -c` (the middle process must get `SIGPIPE`/EOF correctly).

Example:
```
sh> ls | wc -l
       4
[ls: exit 0] [wc: exit 0]
sh> echo hello | tr a-z A-Z | rev
OLLEH
```

<details><summary>Hint</summary>
For N stages you need N-1 pipes; child *i* dups pipe *i-1*'s read end to 0 and pipe *i*'s write end to 1, then closes **all** pipe fds it inherited (loop over the whole array). The parent closes all its ends before waiting.
</details>

### 17.5 — **Graceful shutdown + periodic checkpoint via signals**

Write a loop that increments a 64-bit counter as fast as possible. Install with `sigaction`: `SIGINT`/`SIGTERM` → set a stop flag; `SIGALRM` (from `setitimer`, every 500 ms) → set a "report" flag. In the main loop, when the report flag is set, print the counter and the increments/second since the last report (`CLOCK_MONOTONIC`), then clear the flag. On stop, print the final count and exit 0. The handlers must contain nothing but flag assignments. Verify with Ctrl-C and with `kill -TERM $(pgrep ex17_5)` from another terminal. Then run with `-O2` and *remove* `volatile` from the flags: describe what happens and why.

Example:
```
t=0.50 s  count=612000000  rate=1.22e9/s
t=1.00 s  count=1224000000 rate=1.22e9/s
^C
stopped cleanly at count=1409112031
```

<details><summary>Hint</summary>
`volatile sig_atomic_t stop, report;` — two separate flags. Use `sa.sa_flags = SA_RESTART` and see that `nanosleep`, if you add one, still returns `EINTR` — it's on the never-restarted list. Without `volatile`, clang hoists the load and the loop never exits (look at `cc -O2 -S` for a `b` to itself).
</details>

### 17.6 — **`poll`-based line multiplexer**

Fork two children: child A writes `"A tick N\n"` to a pipe every 300 ms, child B writes `"B tick N\n"` every 700 ms, each for 3 s then exits. The parent `poll`s both pipes plus `stdin`, prints each line prefixed with a monotonic timestamp as it arrives, echoes anything typed on stdin, and exits when both pipes report `POLLHUP`. Never block on one pipe while the other has data.

Example:
```
[0.300] A tick 1
[0.600] A tick 2
[0.700] B tick 1
[0.900] A tick 3
...
[3.001] A closed
[3.002] B closed
```

<details><summary>Hint</summary>
Pipe writes ≤ `PIPE_BUF` (512+) bytes are atomic, so each `write` is one complete line — but `read` may still return several lines at once; split on `\n`. Make the parent close its write ends or `POLLHUP` never comes.
</details>

### 17.7 — **TCP echo server + timing client**

Write `ex17_7 server PORT` and `ex17_7 client HOST PORT N`. The server uses `getaddrinfo` (`AI_PASSIVE`), `SO_REUSEADDR`, `listen`, and handles each client in a forked child (reap with a `SIGCHLD` handler using `waitpid(-1, ..., WNOHANG)` in a loop), echoing bytes until EOF. The client connects, sends N messages `"msg K\n"`, reads back each full line (a `recv` may return a partial line!), verifies the echo, and prints the round-trip latency distribution (min / median / p99 in µs). Test with `nc localhost PORT` too.

Example:
```
$ ./ex17_7 server 8080 &
$ ./ex17_7 client localhost 8080 10000
10000/10000 echoed correctly
rtt  min 18 us   median 27 us   p99 61 us
```

<details><summary>Hint</summary>
Accumulate `recv` results in a buffer and consume up to the newline — TCP has no message boundaries. `accept` returns `EINTR` when `SIGCHLD` arrives; loop. Ignore `SIGPIPE` in the server. If `bind` says `Address already in use` right after a restart, you forgot `SO_REUSEADDR`.
</details>

### 17.8 — **Zero-copy MNIST via `mmap`** (ML — feeds P04)

Download MNIST (`train-images-idx3-ubyte`, 47 040 016 bytes; `train-labels-idx1-ubyte`) or generate a fake IDX file with the right header. Write `mnist_map()` that `mmap`s the images file `PROT_READ`, validates magic `0x00000803`, reads `n/rows/cols` from the big-endian header **by assembling bytes**, checks `16 + n*rows*cols == file size` (reject otherwise — a truncated file must not become an OOB read), and returns a `const uint8_t *pixels`. Compute the global mean pixel and the per-image mean of image 0. Time (a) `mnist_map` + first full pass, (b) a second full pass, (c) `fread` into `malloc` + pass, with `CLOCK_MONOTONIC`. Explain the three numbers in a comment (page faults, page cache, copies). Then `madvise(MADV_SEQUENTIAL)` and see whether (a) changes.

Example:
```
n=60000 rows=28 cols=28  file 47040016 bytes
mmap+pass1: 21.3 ms   pass2: 9.8 ms   fread+pass: 17.9 ms
mean pixel 33.318   image0 mean 35.108
```

<details><summary>Hint</summary>
`(uint64_t)n * rows * cols` — do the multiply in 64 bits before comparing. `munmap` the whole length you mapped, not the pixel pointer. Page faults on Apple Silicon are per 16 KiB page: 47 MB / 16 KiB ≈ 2870 minor faults ≈ a few ms.
</details>

### 17.9 — **Resumable N-body with a memory-mapped checkpoint** (sims — feeds P15)

Take your N-body integrator (or a 2-D harmonic lattice if you don't have one yet) and store the *entire* state — `struct { uint32 magic, version; uint64 step; double t; uint32 n; double pos[], vel[] }` — in a file via `MAP_SHARED` `mmap`. On start: if the file exists and the header validates, resume from `step`; else initialize. Integrate until `-s STEPS` (via `getopt`) or `SIGINT`/`SIGTERM`; `msync(MS_SYNC)` every `-c K` steps and at exit. Print energy every 1000 steps. Kill it mid-run with Ctrl-C, restart, and show the energy series continues without a jump. Finally: run to completion, delete the checkpoint, rerun from scratch with the same `-s` and confirm the final positions are bit-identical (`cmp` two dumps).

Example:
```
$ ./ex17_9 -s 200000 -c 5000 ckpt.bin
init n=256
step 1000  E=-1.2531e+00
...
^C
stopping: step 87322 checkpointed
$ ./ex17_9 -s 200000 -c 5000 ckpt.bin
resumed at step 87322
step 88000  E=-1.2531e+00
```

<details><summary>Hint</summary>
Compute `len` from `n` before `ftruncate`; the flexible-array trick needs `pos`/`vel` laid out after the header — two `double *` computed from `(char *)base + sizeof header` is simplest. Update `step` **after** the arrays each iteration. Bit-identical reruns require the same operation order — no OpenMP reductions here.
</details>

### 17.10 — **Fork-based hyperparameter sweep with a results pipe** (ML — feeds P07/P09)

Write a driver that loads a dataset once (mmap from 17.8, or a synthetic linear-regression set), then forks one child per configuration in a grid of learning rates × epochs (e.g. 4 × 3 = 12), with at most `sysconf(_SC_NPROCESSORS_ONLN)` running at once. Each child trains (your logistic/linear regression from P05/P06 or a 50-line stand-in), writes one binary record `{lr, epochs, final_loss, seconds}` to a shared pipe, and `_exit`s. The parent `read_all`s records as children finish, `waitpid`s each, reports any that crashed (`WIFSIGNALED`), and prints a sorted table plus the total wall time vs the sum of child CPU times (the speedup). Explain in a comment why copy-on-write made "load once, fork many" free, and why this is safer than threads for this job.

Example:
```
12 configs, 10 workers
lr=0.100 ep=30  loss=0.2211  2.31 s
lr=0.030 ep=30  loss=0.2458  2.29 s
...
1 config crashed: lr=1.000 ep=30 (signal 8 SIGFPE)
wall 4.7 s, cpu 27.6 s, speedup 5.9x
```

<details><summary>Hint</summary>
Records ≤ `PIPE_BUF` bytes are written atomically, so a fixed 32-byte struct never interleaves. Close the parent's write end *after* the last fork or `read` never sees EOF. `getrusage(RUSAGE_CHILDREN)` gives total child CPU time. Throttle with a running-count and `wait(NULL)` before forking the next.
</details>
