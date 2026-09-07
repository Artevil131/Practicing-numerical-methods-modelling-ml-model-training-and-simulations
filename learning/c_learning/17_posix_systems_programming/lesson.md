# Chapter 17 — POSIX Systems Programming

## What you'll be able to do after this chapter

- Explain what a system call is, what it costs (numbers), and trace exactly which ones your program makes with `dtruss` (macOS) or `strace` (Linux).
- Use file descriptors directly — `open/read/write/close/lseek/fstat` — with correct `EINTR` and partial-write handling, and know when `FILE*` is the better tool.
- `mmap` a 47 MB MNIST file and hand a `const uint8_t *` to your training loop with zero copies; checkpoint a simulation by writing through a `MAP_SHARED` mapping.
- Create processes (`fork/exec/wait`), wire them together with pipes and `dup2`, and implement `a | b` yourself.
- Handle signals safely (`sigaction`, the `volatile sig_atomic_t` flag), so Ctrl-C saves your N-body state instead of losing 6 hours of work.
- Write a TCP echo server/client with `getaddrinfo`, multiplex descriptors with `poll`, time with `CLOCK_MONOTONIC`, parse options with `getopt`, walk directories, load plugins with `dlopen`, and do all of it with syscall-grade error handling and basic security hygiene.

## Why this matters for ML / numerics / sims

Everything below `printf` and `fopen` is a system call. The MNIST loader from `../08_file_io/lesson.md` `fread`s 47 MB through a 4 KiB stdio buffer — thousands of copies from kernel to user space; `mmap` makes the file *be* your array. A PIC or FDTD run that takes hours must survive Ctrl-C, an `SIGALRM` budget, or a laptop lid closing: that is signals plus memory-mapped checkpoints. A hyperparameter sweep is `fork` + pipes (one process per config, no shared-memory bugs). Streaming a loss curve to a plotting script on another machine is 40 lines of sockets. A `dlopen` plugin lets you swap activation functions or force kernels without recompiling the simulator. And every one of these APIs reports errors the same way — a `-1` and `errno` — so the discipline you learn here is the discipline that keeps a 10,000-line codebase debuggable. Chapter 16 (`../16_concurrency_pthreads_and_atomics/lesson.md`) gave you parallelism *inside* a process; this chapter is the operating system *around* it.

**Portability note, read first.** POSIX is a separate standard from ISO C (POSIX.1-2017 = IEEE Std 1003.1-2017 = The Open Group Base Specifications Issue 7). `-std=c11` asks for *strict* ISO C. On Linux/glibc that hides every POSIX declaration (`sigaction`, `getopt`, `clock_gettime`, `mkstemp`, ...) until you define a feature-test macro **before the first `#include`**:

```c
#define _POSIX_C_SOURCE 200809L   /* POSIX.1-2008 + XSI extensions you actually use */
/* or #define _GNU_SOURCE for everything glibc has */
#include <unistd.h>
```

Apple's headers expose POSIX under `-std=c11` anyway (`_DARWIN_C_SOURCE` is the equivalent knob for macOS-only extras). Put the define in every file regardless — it costs nothing and your code compiles on Linux. Docs: `man 7 feature_test_macros` (Linux), `man 2 intro` (macOS).

---

## 1. The syscall boundary

Your program runs in *user mode*: it can compute, but it cannot touch a disk, a network card, or another process's memory. To do those it asks the kernel via a **system call**: put arguments in registers, execute a trap instruction (`svc #0x80` on arm64, `syscall` on x86-64), the CPU switches to kernel mode, the kernel does the work, switches back. Every libc I/O function bottoms out in one: `fopen → open`, `printf → write`, `malloc` (for large blocks) `→ mmap`.

The cost is not the instruction count, it's the mode switch, the cache/TLB disturbance, and the kernel-side work. Measured on this machine (Apple M-series, macOS, 2 000 000 iterations each, `clock_gettime(CLOCK_MONOTONIC)` around the loop):

| Call                      | ns / call | What it tells you                                             |
|---------------------------|----------:|---------------------------------------------------------------|
| `getpid()`                |       1.8 | libSystem caches it — **not** a syscall after the first call  |
| `clock_gettime(MONOTONIC)`|      14   | reads a shared kernel page (commpage / vDSO) — no trap        |
| `getppid()`               |      82   | the cheapest real trap: round-trip cost floor                 |
| `lseek(fd, 0, SEEK_SET)`  |     164   | trap + fd table lookup                                        |
| `read(fd, buf, 0)`        |     229   | trap + fd lookup + vfs dispatch, zero bytes moved             |

Linux on a modern x86 server is roughly 2× cheaper (~50-100 ns for `getppid`) but the shape is identical. A `read` of 1 byte costs the same ~230 ns as a `read` of 4 KiB; that's why `stdio` buffers (chapter 08) and why `write(fd, &byte, 1)` in a loop is a bug.

**See the syscalls your program makes:**

```sh
# macOS (needs sudo; SIP may require disabling for system binaries, works on your own):
sudo dtruss -f ./ex_demo 2>&1 | head -60        # -f: follow forked children
sudo fs_usage -w -f filesys ./ex_demo           # file-system calls with timing
lsof -p $(pgrep ex_demo)                         # every open fd of a running process
# Linux:
strace -f -T ./ex_demo                           # -T: time spent in each call
strace -c ./ex_demo                              # histogram: which calls, how many, how long
ltrace ./ex_demo                                 # library calls (fopen, malloc) instead of syscalls
```

Excerpt from `dtruss` on `example.c`:

```
open("/tmp/ch17_demo_XXXXXX\0", 0xA02, 0x180)  = 3 0
write(0x3, "\0\0\0\0\0\0\0\0...", 0x1F40)     = 8000 0
fstat64(0x3, 0x16FDFE2E8, 0x0)                 = 0 0
lseek(0x3, 0xFA0, 0x0)                         = 4000 0
read(0x3, "\0\0\0\0\0@o@\0", 0x8)              = 8 0
```

The manual for a syscall is in **section 2**: `man 2 open`, `man 2 mmap`, `man 2 fork`. Library functions are section 3: `man 3 getaddrinfo`, `man 3 printf`. `man 7 signal` (Linux) / `man 2 sigaction` (macOS) for overviews. Online: <https://pubs.opengroup.org/onlinepubs/9699919799/> is the actual POSIX text — dry, but authoritative when man pages disagree across OSes.

Python equivalent: the `os` module is a thin wrapper over exactly these calls (`os.open`, `os.read`, `os.fork`, `os.pipe`, `os.dup2`). `subprocess.Popen` is §6-§7 of this chapter written for you.

## 2. File descriptors vs `FILE*`

A **file descriptor** is a small non-negative `int` indexing the kernel's per-process open-file table. 0, 1, 2 are stdin/stdout/stderr (`STDIN_FILENO`, ...). A `FILE*` is a libc struct *containing* an fd plus a user-space buffer, position, and error flags (C11 §7.21). `fileno(fp)` gets the fd out; `fdopen(fd, "r")` wraps one.

```
  user space                             kernel
  ┌─────────────────────┐    ┌──────────────────────────────┐
  │ FILE* fp            │    │ fd table (per process)        │
  │  ├ int fd = 3 ──────┼───▶│ [0] tty  [1] tty  [2] tty     │
  │  ├ char buf[4096]   │    │ [3] ──▶ open file description │──▶ inode / socket / pipe
  │  └ pos, flags       │    │         (offset, O_ flags)    │
  └─────────────────────┘    └──────────────────────────────┘
```

### `open`, `read`, `write`, `close`, `lseek`, `fstat`

```c
#include <fcntl.h>      /* open, O_* flags */
#include <unistd.h>     /* read, write, close, lseek */
#include <sys/stat.h>   /* fstat, struct stat, S_IS* */

int fd = open("weights.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);   /* mode only with O_CREAT */
if (fd < 0) { perror("open weights.bin"); return -1; }
ssize_t n = write(fd, w, nbytes);      /* may write LESS than nbytes (see below) */
struct stat st;
fstat(fd, &st);                        /* st.st_size, st.st_mode, st.st_mtime, st.st_ino */
off_t pos = lseek(fd, 0, SEEK_END);    /* another way to get the size; also SEEK_SET/SEEK_CUR */
close(fd);                             /* returns -1 on error too — NFS, quota; check it once */
```

`O_` flags (`man 2 open`): exactly one of `O_RDONLY`/`O_WRONLY`/`O_RDWR`, OR-ed with `O_CREAT` (create if missing; then the third `mode` arg is required), `O_EXCL` (with `O_CREAT`: fail if it exists — atomic, the basis of lock files), `O_TRUNC`, `O_APPEND` (every write goes to the end atomically — safe for logs from many processes), `O_NONBLOCK` (§9), `O_CLOEXEC` (close on `exec`, §6). The `mode` is masked by the process `umask` (usually `022`), so `0666 & ~022 = 0644`.

### `errno`, `EINTR`, partial writes: the loop every program needs

Three facts the base course glossed over:

1. Syscalls report failure by returning `-1` and setting the thread-local `int errno` (`<errno.h>`). `errno` is only meaningful *immediately after a failing call* — a successful call may leave garbage in it. Never test `errno` without first testing the return value.
2. A blocking syscall interrupted by a signal handler (§7) returns `-1` with `errno == EINTR` having done *nothing* — you must retry.
3. `write` on a pipe, socket, or when a signal arrives *after* some bytes moved may return fewer bytes than requested. `read` returns fewer at EOF and *routinely* on sockets/pipes.

```c
static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        p += n; len -= (size_t)n;
    }
    return 0;
}
```

`example.c` has `read_all` too. Wrap once, use everywhere. Python equivalent: `os.write` has the same partial-write behaviour; `file.write` hides it the way `fwrite` does.

### When to use which

| Need                                         | Use                              | Why                                                     |
|----------------------------------------------|----------------------------------|---------------------------------------------------------|
| Text, formatted, small records                | `FILE*` (`fprintf`, `fgets`)      | buffering turns 1000 tiny writes into one syscall        |
| One big binary blob (weights, image)          | `write_all`/`read_all` on an fd   | one syscall anyway; no double buffering                  |
| Read-only access to a large file              | `mmap` (§3)                       | zero copies, kernel does the paging                      |
| Pipes, sockets, `poll`, `dup2`, `O_NONBLOCK`  | fd                                | `FILE*` can't do any of these correctly                  |
| Need `errno`-precise error on every byte      | fd                                | `fwrite` errors surface at `fflush`/`fclose`, too late   |

Buffered vs unbuffered in numbers: 1 000 000 one-byte `write(2)` calls ≈ 0.23 s; the same through `fputc` ≈ 4 ms (≈ 250 flushes of a 4 KiB buffer). `stdout` is line-buffered to a terminal and *fully* buffered to a pipe — which is why `printf("got here\n")` vanishes when the program crashes with output redirected (chapter 13 §3), and why you must `fflush(stdout)` before `fork` (§5).

## 3. `mmap`: the file becomes memory

`mmap` (`man 2 mmap`) asks the kernel to map a range of a file (or nothing — "anonymous") into your address space. Reading the pointer reads the file; the kernel pages data in on first touch, straight from the page cache, with no `read()` copy.

```c
#include <sys/mman.h>
int fd = open(path, O_RDONLY);
struct stat st; fstat(fd, &st);
void *base = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
if (base == MAP_FAILED) { perror("mmap"); ... }   /* NOT NULL — MAP_FAILED is (void*)-1 */
close(fd);                                        /* mapping stays valid */
/* ... use base ... */
munmap(base, (size_t)st.st_size);
```

Arguments: hint address (`NULL` = you choose), length, protection (`PROT_READ|PROT_WRITE|PROT_EXEC`), flags (`MAP_PRIVATE`: copy-on-write, writes don't reach the file; `MAP_SHARED`: writes do; `MAP_ANONYMOUS`/`MAP_ANON`: no file, `fd = -1`), fd, offset (**must be a multiple of the page size**: `sysconf(_SC_PAGESIZE)` = 16384 on Apple Silicon, 4096 on x86 and most Linux ARM — never hard-code it). The returned pointer is page-aligned, so it satisfies every type's alignment.

`madvise(base, len, MADV_SEQUENTIAL | MADV_WILLNEED)` are hints (read-ahead aggressively / prefetch); `MADV_RANDOM` disables read-ahead for random access. Safe to ignore their return value.

### MNIST at zero copy

The IDX format (chapter 08): big-endian `uint32` magic `0x00000803`, count, rows, cols, then `count*rows*cols` bytes of pixels. The file is 47 040 016 bytes for 60 000 training images. With `mmap`, "loading" is parsing 16 bytes:

```c
static uint32_t be32(const uint8_t *p) {           /* assemble bytes: no alignment or endian UB */
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
typedef struct { const uint8_t *pixels; uint32_t n, rows, cols; void *map; size_t len; } Mnist;

int mnist_map(Mnist *m, const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return -1; }
    m->len = (size_t)st.st_size;
    m->map = mmap(NULL, m->len, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (m->map == MAP_FAILED) return -1;
    const uint8_t *p = m->map;
    if (m->len < 16 || be32(p) != 0x00000803u) { munmap(m->map, m->len); errno = EINVAL; return -1; }
    m->n = be32(p + 4); m->rows = be32(p + 8); m->cols = be32(p + 12);
    if ((uint64_t)m->n * m->rows * m->cols + 16 != m->len) { munmap(m->map, m->len); errno = EINVAL; return -1; }
    m->pixels = p + 16;                            /* image i is pixels + i*rows*cols */
    madvise(m->map, m->len, MADV_SEQUENTIAL);
    return 0;
}
/* mnist_map: 0.05 ms. fread-into-malloc of the same file: ~15 ms cold, ~6 ms warm. */
```

Then `pixels[i * 784 + j] / 255.0f` in the batch loop — the first touch of each 16 KiB page costs a minor fault (~1 µs), after which it's ordinary memory. Note the `const`: writing through a `PROT_READ` mapping is a `SIGBUS`/`SIGSEGV`, not UB you can ignore. Also note you validated the size before trusting `n*rows*cols` — a truncated file must not become an out-of-bounds read (chapter 15).

### Anonymous mappings

```c
size_t bytes = n * sizeof(double);
double *a = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
/* zero-filled, page-aligned; pages are allocated lazily on first write */
munmap(a, bytes);
```

This is what `malloc` does for blocks above its threshold (128 KiB in glibc; macOS's allocator is tiered similarly). Useful directly when you want a huge zero-initialized array whose untouched parts cost nothing (a sparse grid), or `MAP_SHARED|MAP_ANONYMOUS` memory shared with `fork`ed children (§5).

Python equivalent: `numpy.memmap(path, dtype=np.uint8, mode='r', offset=16)` is exactly `mnist_map`.

## 4. Memory-mapped checkpointing

Flip the mapping to `MAP_SHARED` and `PROT_WRITE`, and the file *is* your simulation state. No serialize step; a crash loses at most the pages not yet written back.

```c
#define CKPT_MAGIC 0x54504B43u
typedef struct {
    uint32_t magic, version;   /* validate on resume */
    uint64_t step;
    double   t;
    double   state[];          /* C11 flexible array member: n = (file_size - header)/8 */
} Checkpoint;

int fd = open("run.ckpt", O_RDWR | O_CREAT, 0644);
size_t len = sizeof(Checkpoint) + n * sizeof(double);
ftruncate(fd, (off_t)len);                     /* a mapping can't grow a file: size it first */
Checkpoint *ck = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
if (ck->magic != CKPT_MAGIC) { ck->magic = CKPT_MAGIC; ck->version = 1; ck->step = 0; /* init */ }
for (; ck->step < max_steps && !g_stop; ck->step++) {
    integrate(ck->state, n, dt);               /* writes go straight to the file's pages */
    ck->t += dt;
    if (ck->step % 1000 == 0) msync(ck, len, MS_SYNC);   /* force to disk every 1000 steps */
}
msync(ck, len, MS_SYNC);
munmap(ck, len); close(fd);
```

Rules: (1) write the header *after* the data in each step so a resume sees a consistent `(step, state)` pair; (2) `msync(MS_SYNC)` blocks until the dirty pages are on disk — ~ms for MBs — so do it every N steps, not every step; without it the kernel writes back within ~30 s and on process crash (not power loss) the data survives anyway; (3) the layout is your compiler's `struct` layout — same machine, same compiler, fine; for a portable file format write explicit little-endian fields (chapter 15 §endianness). `example.c` §3 runs this loop, reopens the file, and verifies `step == 1000`.

## 5. `fork`, `exec`, `wait`: the process model

A **process** is an address space + fd table + one or more threads. `fork()` (`man 2 fork`) duplicates the calling process: the child is a copy-on-write clone — same code, same variables with the same values, same open fds (sharing the *same* file offsets), different `pid`. `fork` returns twice: the child's pid in the parent, 0 in the child, -1 on failure.

```c
fflush(stdout);                 /* or the buffered text is duplicated into both processes */
pid_t pid = fork();
if (pid < 0) { perror("fork"); return -1; }
if (pid == 0) {                 /* child */
    do_work();
    _exit(code);                /* _exit: no atexit handlers, no stdio flush — parent owns those */
}
int status;                     /* parent */
if (waitpid(pid, &status, 0) < 0) perror("waitpid");
if (WIFEXITED(status))   printf("exit code %d\n", WEXITSTATUS(status));
if (WIFSIGNALED(status)) printf("killed by signal %d\n", WTERMSIG(status));
```

**Zombies.** A child that exits stays in the process table (as a *zombie*, `Z` in `ps`) until the parent calls `wait`/`waitpid` to collect its status. A parent that forks in a loop and never waits leaks pids until `fork` fails with `EAGAIN`. Either `waitpid` every child, or install a `SIGCHLD` handler that loops `while (waitpid(-1, &st, WNOHANG) > 0)`, or set `SIG_IGN` for `SIGCHLD` (children are auto-reaped, you lose the status). If the *parent* dies first, the child is re-parented to `launchd`/`init` (pid 1), which reaps it.

**`exec` replaces the program.** `execvp(file, argv)` searches `PATH` for `file` and replaces the current process image with it — same pid, same fds (unless `O_CLOEXEC`), new code and memory. It only returns on failure. `argv[0]` is by convention the program name and `argv` must end with `NULL`:

```c
char *argv[] = { "ls", "-l", "/tmp", NULL };
execvp("ls", argv);
perror("execvp");          /* only reached if ls wasn't found or not executable */
_exit(127);                /* shell convention: 127 = command not found */
```

Variants: `execv` (exact path, no PATH search), `execl/execlp` (varargs), `execve` (explicit environment). `fork` + `exec` + `waitpid` is `subprocess.run`.

**`fork` in a multithreaded program** copies only the calling thread. Any mutex another thread held is now locked forever in the child; `malloc` may be one of those mutexes. POSIX says the child may only call async-signal-safe functions (§7) until it `exec`s. Rule: fork before you spawn threads, or fork-then-exec immediately, or use `posix_spawn`.

**Copy-on-write in numbers.** `fork` of a process with a 1 GB heap copies page *tables* (~2 ms), not the heap. Pages are copied only when either side writes — 16 KiB at a time. A hyperparameter sweep that forks after loading MNIST shares the read-only 47 MB across all children for free.

## 6. Pipes and redirection: `a | b`

A **pipe** (`man 2 pipe`) is a one-way byte channel in the kernel with a 64 KiB buffer (measured; Linux default is also 64 KiB, `fcntl(F_SETPIPE_SZ)` can change it). `pipe(int p[2])` gives `p[0]` (read end) and `p[1]` (write end). A write blocks when the buffer is full; a read blocks when it's empty and returns 0 (EOF) only when **every** write end in every process is closed.

`dup2(oldfd, newfd)` closes `newfd` and makes it refer to the same open file as `oldfd`. Redirecting stdout means `dup2(p[1], 1)`. Now the shell's `a | b`:

```c
int p[2]; pipe(p);
if (fork() == 0) {                    /* a */
    dup2(p[1], STDOUT_FILENO);
    close(p[0]); close(p[1]);         /* close BOTH: the dup keeps fd 1 alive */
    execvp(a_argv[0], a_argv); _exit(127);
}
if (fork() == 0) {                    /* b */
    dup2(p[0], STDIN_FILENO);
    close(p[0]); close(p[1]);
    execvp(b_argv[0], b_argv); _exit(127);
}
close(p[0]); close(p[1]);             /* parent too, or b never sees EOF */
while (wait(NULL) > 0) {}
```

The single most common bug: forgetting to close the write end in a process that doesn't write. `b` then blocks forever on `read` because *someone* still holds `p[1]`. `example.c` §5 runs `/bin/echo ... | /usr/bin/tr a-z A-Z` this way.

Redirection to a file is the same idea without a pipe: `int fd = open("out.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644); dup2(fd, 1); close(fd);` before `exec`.

**`SIGPIPE`.** Writing to a pipe or socket whose read end is closed delivers `SIGPIPE`, which kills the process by default (that's why `yes | head` terminates). Servers and any program writing to a pipe it doesn't control should `signal(SIGPIPE, SIG_IGN)` and handle `write == -1 && errno == EPIPE` instead. `head` closing early is not an error worth dying for.

Python equivalent: `subprocess.Popen(a, stdout=PIPE)` then `Popen(b, stdin=pa.stdout)` — with the same "close the parent's copy" footgun (`pa.stdout.close()`).

## 7. Signals

A **signal** is an asynchronous notification delivered to a process: `SIGINT` (Ctrl-C), `SIGTERM` (`kill` default), `SIGALRM` (timer), `SIGCHLD` (child exited), `SIGSEGV`/`SIGBUS` (your bug), `SIGPIPE` (§6), `SIGUSR1/2` (yours). Each has a default action (terminate, ignore, core dump) and can be caught by a handler that runs *in between any two instructions* of your program, on the same thread.

### Use `sigaction`, not `signal`

`signal()` has implementation-defined semantics (C11 §7.14.1.1: whether the handler is reset after one delivery, whether syscalls restart). `sigaction` (`man 2 sigaction`) is precise:

```c
static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }   /* set a flag. That's ALL. */

struct sigaction sa = {0};
sa.sa_handler = on_sigint;
sigemptyset(&sa.sa_mask);     /* signals blocked while the handler runs (none extra) */
sa.sa_flags = 0;              /* or SA_RESTART: restart interrupted read/write instead of EINTR */
sigaction(SIGINT, &sa, NULL);
sigaction(SIGTERM, &sa, NULL);

while (!g_stop && step < max) { integrate(); step++; }
/* here, on the main thread, with everything consistent: */
save_checkpoint(); close_files(); printf("stopped at step %lu\n", step);
```

### Why the flag pattern

A handler may interrupt `malloc` halfway through updating its free list, or `printf` halfway through its buffer. Calling either from the handler corrupts the heap or deadlocks. POSIX lists the functions that are **async-signal-safe** (`man 2 sigaction` on macOS, `man 7 signal-safety` on Linux): `write`, `_exit`, `kill`, `sigaction`, `read`, `open`, `close`, `waitpid`, `time`, `clock_gettime` — roughly: raw syscalls, no stdio, no `malloc`, no `exit`, no locks. So the handler sets a flag and the main loop does the work.

`volatile sig_atomic_t` is the one type C guarantees you can read/write atomically with respect to signal delivery (C11 §7.14 — `sig_atomic_t`; §5.1.2.3 — the handler may only read/write `volatile sig_atomic_t` or lock-free atomics). `volatile` stops the optimizer hoisting the load out of the loop; without it, `-O2` legitimately compiles `while (!g_stop)` into `while (1)` because nothing *in the loop* changes `g_stop` (chapter 15 §volatile). `volatile` is *not* a substitute for atomics between threads (chapter 16) — signals are same-thread interruptions, which is exactly the case it was designed for.

### Timers

```c
struct itimerval it = {0};
it.it_value.tv_sec = 60;                 /* first SIGALRM in 60 s */
it.it_interval.tv_sec = 60;              /* then every 60 s (0 = one-shot) */
setitimer(ITIMER_REAL, &it, NULL);       /* <sys/time.h>; alarm(60) is the whole-seconds version */
/* handler sets g_checkpoint = 1; main loop msyncs when it sees it */
```

`ITIMER_VIRTUAL` counts only user CPU time (a compute budget); `ITIMER_PROF` user+system. POSIX `timer_create` is more flexible but unavailable on macOS — use `setitimer`, or `dispatch_source` (Grand Central Dispatch), or a thread that sleeps.

### `SA_RESTART` and `EINTR`

With `sa_flags = 0`, a signal arriving during a blocking `read`/`write`/`accept`/`nanosleep` makes it return `-1`/`EINTR` — good when you *want* the loop to notice the flag promptly. With `SA_RESTART` the kernel transparently restarts most calls — convenient, but `nanosleep`, `select`, `poll` are never restarted regardless. Your `write_all` loop is correct either way. Sending signals: `kill(pid, SIGTERM)`, `raise(SIGUSR1)`; from the shell `kill -USR1 <pid>`.

Blocking: `sigprocmask(SIG_BLOCK, &set, &old)` defers delivery while you're inside a critical section; `sigsuspend` atomically unblocks and waits. In multithreaded programs signals go to *some* thread that hasn't blocked them; the clean design is to block everything in all threads and have one thread `sigwait`.

`example.c` §7 arms a 200 ms `setitimer`, runs a leapfrog oscillator until the flag flips, and prints the step count (~10⁸ steps on this machine at -O2).

## 8. `select` / `poll` / `kqueue` / `epoll`

Blocking on one fd is easy: call `read`. Blocking on *several* — a socket and a pipe from a child and stdin — requires the kernel to wake you when *any* is ready. `poll` (`man 2 poll`) is the portable, sane one:

```c
#include <poll.h>
struct pollfd fds[2] = {
    { .fd = sock,  .events = POLLIN },
    { .fd = pipe0, .events = POLLIN },
};
int r = poll(fds, 2, 1000);              /* timeout in ms; -1 = forever; 0 = just check */
if (r < 0) { if (errno == EINTR) continue; perror("poll"); }
if (r == 0) puts("timeout");
if (fds[0].revents & POLLIN) handle_socket();
if (fds[1].revents & (POLLIN | POLLHUP)) handle_pipe();   /* POLLHUP: writer closed */
```

`select` (`man 2 select`) is the 1983 original: fixed `fd_set` bitmaps limited to `FD_SETSIZE` (1024) descriptors, and it *modifies* its arguments and the timeout — you rebuild them every iteration. Use `poll` unless porting old code. Both are O(n) per call: the kernel scans every fd you pass. For thousands of connections use the OS-specific ready-list APIs: **`kqueue`/`kevent`** on macOS/BSD (`man 2 kqueue`) and **`epoll`** on Linux (`man 7 epoll`); libraries like libuv/libevent wrap both. For ≤ dozens of fds — a simulation talking to a plotting client — `poll` is all you need. `example.c` §6 shows the three states: timeout, `POLLIN`, `POLLHUP` after the writer closes.

## 9. Non-blocking I/O

By default `read` on an empty pipe/socket blocks. With `O_NONBLOCK` it returns `-1`/`EAGAIN` (POSIX allows `EWOULDBLOCK`; same value on macOS and Linux, test both to be pedantic):

```c
int fl = fcntl(fd, F_GETFL);
fcntl(fd, F_SETFL, fl | O_NONBLOCK);
ssize_t n = read(fd, buf, sizeof buf);
if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { /* nothing yet; do other work */ }
```

`write` similarly returns a partial count or `EAGAIN` when the buffer is full (measure the pipe: 65 536 bytes then `EAGAIN`). Non-blocking + `poll` is the classic single-threaded server structure: poll for readiness, then read/write until `EAGAIN`, never block. `connect` on a non-blocking socket returns `EINPROGRESS`; you `poll` for `POLLOUT` and check `getsockopt(SO_ERROR)`. Use it when a slow peer must not stall your simulation step.

## 10. Sockets

A socket is an fd you `read`/`write` like any other, connected to another process on this or another machine. TCP (`SOCK_STREAM`) gives a reliable in-order byte stream with no message boundaries — you frame messages yourself (length prefix or newline). UDP (`SOCK_DGRAM`) gives unreliable, unordered, bounded datagrams with boundaries preserved.

### Server

```c
#include <netdb.h>          /* getaddrinfo */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE };
struct addrinfo *res;
int err = getaddrinfo(NULL, "8080", &hints, &res);    /* NULL host + AI_PASSIVE = all interfaces */
if (err) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); return -1; }   /* NOT errno */

int srv = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
int one = 1;
setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);   /* restart without 60 s TIME_WAIT wait */
if (bind(srv, res->ai_addr, res->ai_addrlen) < 0) { perror("bind"); return -1; }
freeaddrinfo(res);
listen(srv, 16);                                                /* backlog: queued, un-accepted connections */

for (;;) {
    struct sockaddr_storage peer; socklen_t plen = sizeof peer;
    int c = accept(srv, (struct sockaddr *)&peer, &plen);      /* blocks; returns a NEW fd per client */
    if (c < 0) { if (errno == EINTR) continue; perror("accept"); break; }
    char buf[4096]; ssize_t n;
    while ((n = recv(c, buf, sizeof buf, 0)) > 0)               /* recv == read for sockets (+flags) */
        if (write_all(c, buf, (size_t)n) < 0) break;            /* echo */
    close(c);
}
```

`getaddrinfo` (`man 3 getaddrinfo`) does name resolution and fills `sockaddr` for IPv4 or IPv6 — never hand-build `sockaddr_in` for real hosts. `htons/htonl/ntohs/ntohl` convert port/address to **network byte order** (big-endian); the kernel expects them that way; forgetting `htons(8080)` binds port 36895. `accept` returns a new connected fd; the listening fd keeps listening. To serve many clients: `fork` per client (simple, isolated), a thread per client (chapter 16), or `poll` over all of them (§8).

### Client

```c
struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM }, *res;
getaddrinfo("localhost", "8080", &hints, &res);
int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
if (connect(s, res->ai_addr, res->ai_addrlen) < 0) { perror("connect"); }
freeaddrinfo(res);
write_all(s, "hello\n", 6);
char buf[64]; ssize_t n = recv(s, buf, sizeof buf - 1, 0);      /* may return fewer bytes than sent! */
close(s);
```

`example.c` §11 forks a server on `127.0.0.1` port 0 (kernel-chosen; read it back with `getsockname`), connects from the parent, and verifies the echo.

### A tiny HTTP GET

HTTP/1.1 is text over TCP. The exact bytes for `GET /` from `example.com`:

```c
const char *req = "GET / HTTP/1.1\r\n"
                  "Host: example.com\r\n"
                  "Connection: close\r\n"      /* server closes after the response: EOF = done */
                  "\r\n";                      /* blank line ends the header */
getaddrinfo("example.com", "80", &hints, &res); ... connect ...
write_all(s, req, strlen(req));
while ((n = recv(s, buf, sizeof buf, 0)) > 0) fwrite(buf, 1, (size_t)n, stdout);
/* HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n...\r\n\r\n<!doctype html>... */
```

`\r\n` line endings are mandatory. Split headers from body at the first `\r\n\r\n`; `Content-Length` tells you how many body bytes follow. That's enough to fetch a dataset or post a metrics JSON to a local dashboard.

### UDP briefly

```c
int u = socket(AF_INET, SOCK_DGRAM, 0);
sendto(u, msg, len, 0, res->ai_addr, res->ai_addrlen);           /* no connect; each call = 1 datagram */
ssize_t n = recvfrom(u, buf, sizeof buf, 0, (struct sockaddr *)&from, &fromlen);  /* 1 datagram, or truncated */
```

Datagrams up to ~1472 bytes fit one Ethernet frame; anything larger fragments and is more likely lost. Good for fire-and-forget telemetry (send every 100th step's energy to a plotter); wrong for anything that must arrive.

## 11. Time

```c
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);   /* since an arbitrary epoch (boot); never goes backwards */
clock_gettime(CLOCK_REALTIME,  &ts);   /* since 1970-01-01 UTC; NTP and the user can JUMP it */
double t = ts.tv_sec + ts.tv_nsec * 1e-9;
```

Rules: durations → `CLOCK_MONOTONIC` (a realtime clock jumping backwards gives negative elapsed time and a divide-by-zero in your GFLOP/s calculation); timestamps for humans/logs → `CLOCK_REALTIME`. Resolution reported by `clock_getres` is 1000 ns on macOS (Linux typically 1 ns); actual call cost is ~14 ns because it reads a shared kernel page without a trap. macOS also offers `mach_absolute_time()` (ticks; convert with `mach_timebase_info`) and `clock_gettime_nsec_np(CLOCK_UPTIME_RAW)` — you don't need them; `CLOCK_MONOTONIC` is portable and equally cheap. `CLOCK_PROCESS_CPUTIME_ID` measures CPU time consumed by your process (all threads) — the right clock for "how much work" as opposed to "how long did I wait".

Sleeping: `nanosleep(&req, &rem)` (`man 2 nanosleep`) sleeps at least `req`; if interrupted by a signal it returns `-1`/`EINTR` with the remainder in `rem` — loop on it. Measured: `nanosleep(20 ms)` actually slept 25 ms; the scheduler's timer slack is real. `sleep(1)` and `usleep(µs)` are coarser wrappers (`usleep` is obsolescent). For a fixed-rate loop (60 fps donut), sleep until an absolute deadline computed from `CLOCK_MONOTONIC`, not for a fixed duration — otherwise drift accumulates.

## 12. Environment and arguments

```c
const char *th = getenv("OMP_NUM_THREADS");        /* NULL if unset; do not free; do not modify */
int nthreads = th ? atoi(th) : (int)sysconf(_SC_NPROCESSORS_ONLN);
setenv("MPLBACKEND", "Agg", 1);                    /* overwrite=1; affects children after exec */
unsetenv("DEBUG");
extern char **environ;                             /* the whole table, "KEY=VALUE" strings, NULL-terminated */
```

`getopt` (`man 3 getopt`) parses `-x`, `-x value`, `-xvalue`, and stops at `--` or the first non-option:

```c
#include <unistd.h>
int opt; int n = 1000; double dt = 1e-3; const char *out = NULL; int verbose = 0;
while ((opt = getopt(argc, argv, "n:d:o:v")) != -1) {   /* ':' = takes an argument */
    switch (opt) {
    case 'n': n = atoi(optarg); break;
    case 'd': dt = strtod(optarg, NULL); break;
    case 'o': out = optarg; break;
    case 'v': verbose = 1; break;
    default:  fprintf(stderr, "usage: %s [-n steps] [-d dt] [-o file] [-v] input\n", argv[0]); return 2;
    }
}
/* argv[optind] .. argv[argc-1] are the positional arguments */
if (optind >= argc) { fprintf(stderr, "missing input\n"); return 2; }
const char *input = argv[optind];
```

`optarg` points into `argv` (don't free); `optind` is the index of the first non-option; `opterr = 0` silences getopt's own messages; `optopt` holds the offending character. Long options (`--steps=1000`) are `getopt_long` — GNU, but available on macOS in `<getopt.h>`. Prefer `strtol`/`strtod` with end-pointer checks over `atoi` when the input is untrusted (chapter 12).

Python equivalent: `os.environ`, `argparse`.

## 13. Directories and file metadata

```c
#include <dirent.h>
#include <sys/stat.h>
DIR *d = opendir(path);
if (!d) { perror(path); return -1; }
struct dirent *e;
while ((e = readdir(d)) != NULL) {                 /* NULL at end AND on error: set errno=0 first to tell */
    if (e->d_name[0] == '.') continue;             /* skip ., .., dotfiles */
    char full[PATH_MAX];
    int n = snprintf(full, sizeof full, "%s/%s", path, e->d_name);
    if (n < 0 || (size_t)n >= sizeof full) continue;     /* truncated: don't use it */
    struct stat st;
    if (lstat(full, &st) < 0) continue;            /* lstat: don't follow symlinks; stat: do */
    if (S_ISDIR(st.st_mode)) walk(full);           /* recurse */
    else if (S_ISREG(st.st_mode)) printf("%10lld %s\n", (long long)st.st_size, full);
}
closedir(d);
```

`struct dirent` portably has only `d_name` (and `d_ino`). `d_type` (`DT_REG`, `DT_DIR`) exists on macOS and Linux but is *not* POSIX and may be `DT_UNKNOWN` on some filesystems — use `stat` when correctness matters. `struct stat` fields you'll use: `st_size`, `st_mode` (type bits `S_ISREG/S_ISDIR/S_ISLNK` + permission bits), `st_mtime` (modification time; on macOS the `struct timespec` is `st_mtimespec`, Linux `st_mtim` — `st_mtime` is the portable seconds field), `st_nlink`, `st_ino`/`st_dev` (identity: two paths name the same file iff both match). `readdir` order is filesystem order, not sorted — sort the names yourself if it matters (data loaders that must be deterministic!). `mkdir(path, 0755)`, `rename` (atomic replace — write `out.tmp` then `rename` it over `out` so a crash never leaves a half-written file), `unlink`, `rmdir`, `realpath` (§16).

## 14. `dlopen` / `dlsym`: plugins

A shared library can be loaded *at run time* and its symbols looked up by name (`man 3 dlopen`). This is how a simulator loads force kernels or an MLP loads activation functions without recompiling:

```c
/* plugin: relu.c  →  cc -shared -fPIC -o librelu.dylib relu.c     (macOS: -dynamiclib also works)
 *                    cc -shared -fPIC -o librelu.so    relu.c     (Linux) */
#include <math.h>
double act(double x)  { return x > 0 ? x : 0; }
double dact(double x) { return x > 0 ? 1 : 0; }
const char *act_name = "relu";
```

```c
#include <dlfcn.h>
typedef double (*act_fn)(double);
void *h = dlopen("./librelu.dylib", RTLD_NOW);      /* RTLD_NOW: resolve all symbols now, fail early */
if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return -1; }   /* dlerror, not errno */
act_fn act;
*(void **)&act = dlsym(h, "act");                   /* POSIX-blessed cast: see below */
if (!act) { fprintf(stderr, "dlsym: %s\n", dlerror()); }
const char **name = dlsym(h, "act_name");
printf("%s(−1) = %g, %s(2) = %g\n", *name, act(-1), *name, act(2));
dlclose(h);                                         /* act is now dangling — don't call it */
```

ISO C says converting `void *` to a function pointer is not defined (C11 §6.3.2.3 only covers object pointers); POSIX requires it to work for `dlsym` and the `*(void **)&fp = dlsym(...)` idiom is the form the standard's rationale recommends (`memcpy(&fp, &p, sizeof fp)` is the other clean option; chapter 15). Link the host with `-ldl` on Linux (macOS: built into libSystem). macOS names: `.dylib`, `-dynamiclib`, `install_name_tool`; Linux: `.so`, `-shared`, `LD_LIBRARY_PATH`/rpath. Pick the extension with `#ifdef __APPLE__`. A safe plugin ABI: one exported `const PluginAPI *plugin_get(void)` returning a struct of function pointers and a version number (chapter 11's vtable), so adding a function doesn't break old plugins.

## 15. Error handling discipline for syscalls

Every syscall can fail. The checklist:

1. **Check every return value.** `close`, `fclose`, `munmap`, `write` — all of them. Disk full surfaces at `close`/`fsync`; you will not find that bug by staring at `write`.
2. **Read `errno` immediately**, before any other call — `printf` may clobber it. `perror("open weights")` prints `open weights: No such file or directory`; `strerror(errno)` gives the string; `strerror_r` is the thread-safe form. `errno` is thread-local (C11 §7.5 requires a modifiable lvalue; POSIX makes it per-thread) so threads don't stomp each other, but a *signal handler* on the same thread can — save/restore `errno` in handlers that call syscalls.
3. **Distinguish transient from fatal.** `EINTR` → retry. `EAGAIN` → not ready. `ENOENT`, `EACCES`, `EINVAL` → report to the caller. `ENOMEM`, `EMFILE` (too many fds — you leaked some) → probably fatal.
4. **Propagate, don't `exit`.** Library code returns `-1` with `errno` set (or your own error enum, chapter 14 §6) and lets `main` decide. `example.c` uses `die()` because it's a demo.
5. **Clean up on every path** — `goto cleanup` with fds initialized to `-1` and pointers to `NULL` so the cleanup block can close/free unconditionally (chapter 14). An fd leak shows up as `EMFILE` after 256/1024/… iterations — `lsof -p <pid>` shows what you leaked.
6. **Use the return, not the side effect**: `if (read(...) < 0)`, never `read(...); if (errno)`.

## 16. Security basics

You will run code that reads filenames from a config, arguments from a shell, bytes from a socket. Minimum hygiene:

- **Never `system()` or `popen()` with user-influenced strings.** `system("convert " + filename)` with `filename = "x; rm -rf ~"` runs both. Use `fork` + `execvp` with an `argv` array — each element is one argument, nothing is parsed by a shell. If you must call a shell, you must not.
- **Path traversal.** A request for `data/../../../etc/passwd` escapes your data directory. Canonicalize with `realpath(path, resolved)` and check the result starts with your allowed root (`strncmp`), or reject any component equal to `..`. Do this *after* resolving symlinks — that's what `realpath` does.
- **TOCTOU** (time-of-check to time-of-use): `if (access(f, W_OK) == 0) open(f, ...)` — the file can be swapped between the two calls. Just `open` and check the result. Create temp files with `mkstemp` (atomic `O_CREAT|O_EXCL`), never `tmpnam` + `open`. Set `umask(077)` in programs that create sensitive files.
- **`snprintf` bounds.** `snprintf(buf, sizeof buf, ...)` never overflows, but it *truncates* and returns the length it *would have written*. `int n = snprintf(...); if (n < 0 || (size_t)n >= sizeof buf) → truncated; don't use it as a path`. `sprintf` and `strcpy` have no bounds: don't.
- **Untrusted lengths.** A file header saying `n = 4 000 000 000` images means `n * 784` overflows 32 bits. Validate against the actual file size (§3's `mnist_map`), use `size_t`/`uint64_t` and check for overflow before multiplying (chapter 15).
- **Least privilege.** Don't run as root to bind port 80; bind 8080. Drop fds you don't need before `exec` (`O_CLOEXEC`).
- Network input is hostile by default: length-prefix messages, cap the length, and never `recv` into a buffer sized by the peer's claim.

## 17. Daemons (mention)

A daemon is a process detached from any terminal: `fork`, parent exits (shell thinks you're done); child `setsid()` (new session, no controlling tty); `fork` again so the process can never reacquire a tty; `chdir("/")`, `umask(0)`, redirect 0/1/2 to `/dev/null`, log to `syslog`/a file. In 2026 you don't write this yourself: write a normal foreground program that logs to stderr and let **launchd** (`~/Library/LaunchAgents/*.plist`) or **systemd** (`*.service`) daemonize, restart, and capture logs for you.

---

## Gotchas and undefined behavior

- **`MAP_FAILED`, not `NULL`.** `mmap` returns `(void *)-1` on failure; `if (!p)` never fires.
- **`mmap` offset must be page-aligned**; length may be anything (rounded up). `sysconf(_SC_PAGESIZE)` = 16384 on Apple Silicon — code that assumes 4096 breaks here.
- **Accessing a mapped file beyond its end** (or after another process truncates it) is `SIGBUS`. Writing through a `PROT_READ` mapping is `SIGSEGV`. Not recoverable in-band; validate sizes first.
- **`fork` without `fflush`** duplicates stdio buffers: output appears twice. **`exit` in the child** flushes the parent's buffers again *and* runs its `atexit` handlers: use `_exit`.
- **Unclosed pipe ends** → readers never see EOF → hang. Every process must close every end it doesn't use, *including the parent*.
- **Zombies**: every `fork` needs a `wait`.
- **Calling `printf`/`malloc`/`exit` in a signal handler** is UB in practice (heap corruption, deadlock). Set a `volatile sig_atomic_t` flag. A handler must also save/restore `errno` if it calls syscalls.
- **Forgetting `volatile`** on the flag: `-O2` turns `while (!stop)` into `while (1)`. Forgetting `sig_atomic_t`: a 64-bit store could tear on some targets.
- **`errno` after success** is unspecified; **`errno` after `printf`** may be clobbered. Read it first.
- **Ignoring `EINTR`** turns Ctrl-C-then-continue into "the checkpoint write failed". Ignoring partial `write` silently truncates your weights file.
- **`select` modifies its fd_sets and timeout** — rebuild them every loop. `poll` doesn't.
- **`htons` forgotten** → wrong port; `sockaddr_in` size passed as `sizeof(struct sockaddr *)` → `EINVAL`.
- **TCP is a byte stream**: one `send` ≠ one `recv`. Frame your messages.
- **`SIGPIPE` kills your server** when a client disconnects mid-write. `SIG_IGN` it and handle `EPIPE`.
- **`gai_strerror`, `dlerror`** — `getaddrinfo` and `dlopen` do *not* use `errno`.
- **`readdir` returns `NULL` for both end and error** — set `errno = 0` before the loop and check after.
- **`d_type` is not POSIX**; `st_mtim` vs `st_mtimespec` differ by OS; use `st_mtime` (seconds) portably.
- **Casting `void *` to a function pointer** is not ISO C; use the `*(void **)&fp = dlsym()` idiom or `memcpy`.
- **`system()` with interpolated strings** is a shell-injection hole. **`sprintf` into a fixed buffer** is a stack overflow. **`snprintf` truncation** silently produces a different path than you intended — check the return.
- **`fork` after creating threads**: only the calling thread survives; locks held by others stay locked forever in the child.
- **Feature-test macro after an `#include`** does nothing; it must be the first line.

## Common mistakes checklist

- [ ] `#define _POSIX_C_SOURCE 200809L` is the *first line* of every file that uses POSIX.
- [ ] Every syscall's return value is checked; `errno` is read before anything else.
- [ ] `read`/`write` go through `read_all`/`write_all` loops handling `EINTR` and partial transfers.
- [ ] `mmap` compared to `MAP_FAILED`; length validated against `fstat` size before any pointer arithmetic; header fields validated before trusting them.
- [ ] `fflush(stdout)` before `fork`; `_exit` in children; every child `waitpid`ed.
- [ ] All unused pipe ends closed in *every* process; `SIGPIPE` ignored in anything writing to a peer it doesn't control.
- [ ] Signal handlers only set `volatile sig_atomic_t` flags; installed with `sigaction`; the real work happens in the main loop after the flag.
- [ ] Durations measured with `CLOCK_MONOTONIC`; `nanosleep` loops on `EINTR`.
- [ ] `getaddrinfo` + `freeaddrinfo`; `SO_REUSEADDR` on listeners; ports through `htons`; messages framed.
- [ ] `snprintf` return value checked for truncation; no `sprintf`, `strcpy`, `system()`, `tmpnam`.
- [ ] Temp files via `mkstemp`; output written to `name.tmp` then `rename`d over `name`.
- [ ] `dlsym` result cast with the `*(void **)&fp` idiom; `dlerror()` for its errors.

## You can move on when...

- You can predict, then confirm with `dtruss`/`strace`, the exact syscall sequence of `fopen` + `fprintf` × 100 + `fclose` (how many `write`s?).
- You can write `write_all`/`read_all` from memory and explain each branch.
- You can `mmap` the MNIST training file, validate the header, and compute the mean pixel value without a single `read()` — and say why the first pass is slower than the second.
- You can implement `ls | wc -l` with `fork`/`pipe`/`dup2`/`execvp` and it terminates (no hang: you closed the right ends).
- You can make a 10-second loop stop cleanly on Ctrl-C, write a checkpoint, and explain why the handler can't write it itself.
- You can write a TCP echo server and connect to it with `nc localhost 8080`, and explain what `SO_REUSEADDR` prevents.
- You can list, from memory, five things in this chapter that are `errno`-reporting and two that aren't.
- Given `system("gnuplot " + user_string)`, you can say what's wrong and write the safe version.
