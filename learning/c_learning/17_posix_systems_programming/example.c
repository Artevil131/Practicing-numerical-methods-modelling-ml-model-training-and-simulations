/*
 * Chapter 17 — POSIX Systems Programming
 *
 * Compile:  cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm
 * Run:      ./ex_demo
 * Debug:    cc -Wall -Wextra -std=c11 -g -fsanitize=address,undefined -o ex_demo example.c -lm
 * Trace:    sudo dtruss -f ./ex_demo 2>&1 | head -80      (macOS; Linux: strace -f ./ex_demo)
 *
 * Every demo in this file was run, not just compiled, on macOS arm64 (Apple clang 21).
 * fork/pipe/mmap/loopback sockets all work in the sandbox it was written in; if the TCP
 * demo cannot bind on your machine it prints a note and the rest continues.
 *
 * Feature-test macros: -std=c11 is "strict ISO C"; on Linux glibc that HIDES POSIX
 * declarations (getopt, sigaction, clock_gettime, mkstemp, ...) unless you ask for
 * them with _POSIX_C_SOURCE (or _GNU_SOURCE) BEFORE the first #include. Apple's
 * headers expose POSIX under -std=c11 anyway, so the define is harmless here and
 * required there. Always put it first.
 *
 * Contents:
 *   1. raw fds: mkstemp, write_all (partial writes + EINTR), fstat, lseek, read
 *   2. mmap the same file (zero-copy read), anonymous mmap
 *   3. a mmap'd MAP_SHARED checkpoint: write through memory, msync, reopen, verify
 *   4. fork + waitpid + exit-status macros
 *   5. pipe + dup2 + execvp: `/bin/echo hello world | /usr/bin/tr a-z A-Z`
 *   6. poll() on a pipe with a timeout
 *   7. sigaction + volatile sig_atomic_t flag: SIGALRM stops a "simulation" loop
 *   8. clock_gettime(CLOCK_MONOTONIC) + nanosleep
 *   9. getenv, sysconf(_SC_PAGESIZE / _SC_NPROCESSORS_ONLN)
 *  10. opendir/readdir over this chapter's directory
 *  11. TCP echo server (forked child, 127.0.0.1, ephemeral port) + client
 */
#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
#  define _DARWIN_C_SOURCE        /* keeps macOS-only bits (MAP_ANON, DT_* etc.) visible */
#endif

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>       /* setitimer / struct itimerval (XSI, not in <time.h>) */
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Print "where: strerror(errno)" and exit. Use for demo setup failures only;
 * real programs propagate errors (see lesson §15). */
static void die(const char *where) {
    fprintf(stderr, "%s: %s (errno %d)\n", where, strerror(errno), errno);
    exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------ */
/* 1. Raw file descriptors                                              */
/* ------------------------------------------------------------------ */

/* write(2) may write FEWER bytes than asked (pipes, sockets, signals) and may
 * fail with EINTR if a signal arrives first. Every serious program has this loop. */
static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;    /* interrupted before writing anything: retry */
            return -1;                       /* real error; errno is set */
        }
        p   += n;                            /* partial write: advance and loop */
        len -= (size_t)n;
    }
    return 0;
}

/* Same shape for read: returns bytes read (< len only at EOF), -1 on error. */
static ssize_t read_all(int fd, void *buf, size_t len) {
    char *p = buf;
    size_t got = 0;
    while (got < len) {
        ssize_t n = read(fd, p + got, len - got);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;                   /* EOF */
        got += (size_t)n;
    }
    return (ssize_t)got;
}

static void demo_fds(char *path_out, size_t path_cap) {
    puts("== 1. raw fds: mkstemp / write_all / fstat / lseek / read ==");

    /* mkstemp replaces XXXXXX, creates the file O_RDWR|O_CREAT|O_EXCL with mode 0600
     * and returns an open fd — atomically. Never open(tmpnam()) — that's a TOCTOU race. */
    char tmpl[] = "/tmp/ch17_demo_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) die("mkstemp");
    snprintf(path_out, path_cap, "%s", tmpl);

    /* 1000 doubles: a tiny "weights" file. */
    double w[1000];
    for (int i = 0; i < 1000; i++) w[i] = i * 0.5;
    if (write_all(fd, w, sizeof w) < 0) die("write_all");

    struct stat st;
    if (fstat(fd, &st) < 0) die("fstat");
    printf("fd=%d  size=%lld bytes  mode=%o  inode=%llu  blocks=%lld\n",
           fd, (long long)st.st_size, st.st_mode & 0777,
           (unsigned long long)st.st_ino, (long long)st.st_blocks);

    /* lseek: move the file offset. Read back element 500 without reading the rest. */
    off_t off = lseek(fd, (off_t)(500 * sizeof(double)), SEEK_SET);
    if (off < 0) die("lseek");
    double x;
    if (read_all(fd, &x, sizeof x) != (ssize_t)sizeof x) die("read");
    printf("lseek to byte %lld, read w[500] = %.1f\n", (long long)off, x);

    /* Unlike FILE*, there is NO user-space buffer: this write(2) IS a syscall.
     * 1000 one-byte writes through write(2) = 1000 syscalls (~200 ns each here);
     * through fwrite = 1 syscall when the 4-64 KiB stdio buffer flushes. */
    if (close(fd) < 0) die("close");
}

/* ------------------------------------------------------------------ */
/* 2. mmap: the file becomes memory                                     */
/* ------------------------------------------------------------------ */
static void demo_mmap_file(const char *path) {
    puts("\n== 2. mmap a file (zero-copy) + anonymous mmap ==");
    int fd = open(path, O_RDONLY);
    if (fd < 0) die("open");
    struct stat st;
    if (fstat(fd, &st) < 0) die("fstat");
    size_t len = (size_t)st.st_size;

    /* PROT_READ: reads only. MAP_PRIVATE: our writes (none) wouldn't hit the file.
     * No read(2) copies happen; pages are faulted in from the page cache on touch. */
    void *base = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) die("mmap");
    close(fd);                              /* the mapping keeps its own reference */

    /* The bytes ARE doubles we wrote with the same compiler/ABI, so this is a valid
     * view (alignment: mmap returns page-aligned memory). For a foreign format
     * (MNIST IDX, big-endian) you'd read bytes and assemble — lesson §4. */
    const double *w = base;
    double sum = 0;
    for (size_t i = 0; i < len / sizeof(double); i++) sum += w[i];
    printf("mapped %zu bytes at %p; sum of %zu doubles = %.1f (expect %.1f)\n",
           len, base, len / sizeof(double), sum, 0.5 * 999 * 1000 / 2);

    /* Tell the kernel our access pattern (advisory; safe to ignore failures). */
    madvise(base, len, MADV_SEQUENTIAL);
    if (munmap(base, len) < 0) die("munmap");

    /* Anonymous mapping: memory not backed by any file — what malloc uses for large
     * blocks. Zero-filled, page-aligned, page-granular (16 KiB pages on Apple Silicon). */
    size_t big = 1u << 20;                  /* 1 MiB */
    unsigned char *anon = mmap(NULL, big, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (anon == MAP_FAILED) die("mmap anon");
    anon[0] = 1; anon[big - 1] = 2;        /* touching two pages faults in exactly two */
    printf("anonymous 1 MiB at %p, first=%u last=%u (untouched pages read as 0: %u)\n",
           (void *)anon, anon[0], anon[big - 1], anon[big / 2]);
    munmap(anon, big);
}

/* ------------------------------------------------------------------ */
/* 3. Checkpointing through a MAP_SHARED mapping                         */
/* ------------------------------------------------------------------ */
#define CKPT_MAGIC 0x54504B43u   /* "CKPT" little-endian */
#define CKPT_N 64

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t step;            /* simulation step reached */
    double   t;               /* simulation time */
    double   state[CKPT_N];   /* the actual particle/field data */
} Checkpoint;

static void demo_checkpoint(void) {
    puts("\n== 3. mmap'd checkpoint (MAP_SHARED + msync) ==");
    char tmpl[] = "/tmp/ch17_ckpt_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) die("mkstemp ckpt");

    /* A mapping cannot extend a file: size it first. ftruncate to the struct size. */
    if (ftruncate(fd, (off_t)sizeof(Checkpoint)) < 0) die("ftruncate");

    /* MAP_SHARED: stores through the pointer become file contents. */
    Checkpoint *ck = mmap(NULL, sizeof *ck, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ck == MAP_FAILED) die("mmap ckpt");

    ck->magic = CKPT_MAGIC;
    ck->version = 1;
    for (int i = 0; i < CKPT_N; i++) ck->state[i] = 0.0;

    /* "Simulate": each step updates the state in place — the file IS the state.
     * Header fields are written last so a crash mid-step leaves step/t consistent. */
    for (uint64_t s = 1; s <= 1000; s++) {
        for (int i = 0; i < CKPT_N; i++) ck->state[i] += 0.001 * (i + 1);
        ck->step = s;
        ck->t = (double)s * 0.001;
        if (s % 250 == 0) {
            /* Force dirty pages to disk now (MS_SYNC blocks until written).
             * Without msync, the kernel writes back "eventually" — fine after a
             * process crash, not fine after a power loss. */
            if (msync(ck, sizeof *ck, MS_SYNC) < 0) die("msync");
        }
    }
    munmap(ck, sizeof *ck);
    close(fd);

    /* Resume: reopen, validate header, continue. This is what a SIGINT handler
     * lets you do (§7): the flag stops the loop, main msyncs and exits cleanly. */
    fd = open(tmpl, O_RDWR);
    if (fd < 0) die("reopen ckpt");
    Checkpoint *rk = mmap(NULL, sizeof *rk, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rk == MAP_FAILED) die("mmap reopen");
    if (rk->magic != CKPT_MAGIC || rk->version != 1) die("bad checkpoint header");
    printf("resumed: step=%llu t=%.3f state[0]=%.3f state[%d]=%.3f\n",
           (unsigned long long)rk->step, rk->t, rk->state[0], CKPT_N - 1,
           rk->state[CKPT_N - 1]);
    munmap(rk, sizeof *rk);
    close(fd);
    unlink(tmpl);
}

/* ------------------------------------------------------------------ */
/* 4. fork + waitpid                                                    */
/* ------------------------------------------------------------------ */
static void demo_fork(void) {
    puts("\n== 4. fork / waitpid / exit status ==");
    fflush(stdout);   /* IMPORTANT: flush before fork or the buffered text prints twice */

    int shared = 42;
    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (pid == 0) {
        /* Child: a copy-on-write duplicate of the parent. Changing `shared` here
         * changes the child's page only. */
        shared = 7;
        printf("  child: pid=%d ppid=%d shared=%d -> _exit(3)\n", getpid(), getppid(), shared);
        fflush(stdout);
        _exit(3);     /* _exit, not exit: don't run parent's atexit handlers / flush its buffers */
    }
    /* Parent */
    int status;
    if (waitpid(pid, &status, 0) < 0) die("waitpid");   /* reaps the zombie */
    if (WIFEXITED(status))
        printf("parent: child %d exited with code %d; my shared is still %d\n",
               pid, WEXITSTATUS(status), shared);
    else if (WIFSIGNALED(status))
        printf("parent: child killed by signal %d\n", WTERMSIG(status));
}

/* ------------------------------------------------------------------ */
/* 5. pipe + dup2 + execvp: implementing `a | b`                        */
/* ------------------------------------------------------------------ */
static void demo_pipeline(void) {
    puts("\n== 5. pipe + dup2 + execvp:  echo hello world | tr a-z A-Z ==");
    fflush(stdout);

    int p[2];                 /* p[0] = read end, p[1] = write end */
    if (pipe(p) < 0) die("pipe");

    pid_t a = fork();
    if (a < 0) die("fork a");
    if (a == 0) {
        /* left side: stdout -> pipe write end */
        if (dup2(p[1], STDOUT_FILENO) < 0) _exit(127);
        close(p[0]); close(p[1]);            /* close BOTH originals after dup2 */
        char *argv[] = { "echo", "hello world from a pipe", NULL };
        execvp("/bin/echo", argv);           /* only returns on failure */
        _exit(127);
    }
    pid_t b = fork();
    if (b < 0) die("fork b");
    if (b == 0) {
        /* right side: stdin <- pipe read end */
        if (dup2(p[0], STDIN_FILENO) < 0) _exit(127);
        close(p[0]); close(p[1]);
        char *argv[] = { "tr", "a-z", "A-Z", NULL };
        execvp("/usr/bin/tr", argv);
        _exit(127);
    }
    /* Parent must close its copies, or `tr` never sees EOF (the write end stays open). */
    close(p[0]); close(p[1]);
    int st;
    waitpid(a, &st, 0);
    waitpid(b, &st, 0);
    printf("pipeline done, tr exit=%d\n", WIFEXITED(st) ? WEXITSTATUS(st) : -1);
}

/* ------------------------------------------------------------------ */
/* 6. poll() with a timeout                                             */
/* ------------------------------------------------------------------ */
static void demo_poll(void) {
    puts("\n== 6. poll() on a pipe ==");
    int p[2];
    if (pipe(p) < 0) die("pipe");
    struct pollfd pfd = { .fd = p[0], .events = POLLIN, .revents = 0 };

    int r = poll(&pfd, 1, 50);              /* 50 ms timeout, nothing written yet */
    printf("poll with empty pipe: returned %d (0 = timeout)\n", r);

    if (write_all(p[1], "x", 1) < 0) die("write");
    r = poll(&pfd, 1, 50);
    printf("poll after write: returned %d, revents=%s\n", r,
           (pfd.revents & POLLIN) ? "POLLIN" : "?");
    close(p[1]);                            /* writer gone -> reader sees EOF/POLLHUP */
    char c;
    read(p[0], &c, 1);
    r = poll(&pfd, 1, 50);
    printf("poll after writer closed: returned %d, POLLHUP=%d POLLIN=%d\n", r,
           !!(pfd.revents & POLLHUP), !!(pfd.revents & POLLIN));
    close(p[0]);
}

/* ------------------------------------------------------------------ */
/* 7. Signals: sigaction + the flag pattern                             */
/* ------------------------------------------------------------------ */

/* The ONLY thing a handler should do: set a flag. sig_atomic_t is guaranteed to be
 * read/written atomically w.r.t. signals; volatile stops the compiler hoisting the
 * load out of the loop (it can't see the handler runs). */
static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;     /* no printf, no malloc, no exit: those are NOT async-signal-safe */
}

static void demo_signals(void) {
    puts("\n== 7. sigaction: SIGALRM/SIGINT stop a simulation loop ==");
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                        /* no SA_RESTART: blocked syscalls return EINTR */
    if (sigaction(SIGALRM, &sa, NULL) < 0) die("sigaction SIGALRM");
    if (sigaction(SIGINT,  &sa, NULL) < 0) die("sigaction SIGINT");

    /* setitimer: one-shot, fires SIGALRM in 200 ms. (alarm(1) is the coarse version.) */
    struct itimerval it;
    memset(&it, 0, sizeof it);
    it.it_value.tv_usec = 200 * 1000;
    if (setitimer(ITIMER_REAL, &it, NULL) < 0) die("setitimer");

    /* A "simulation": leapfrog a harmonic oscillator until told to stop. The loop
     * checks the flag every step; after the loop, main does the cleanup a handler
     * never could (checkpoint, close files, print). Ctrl-C does the same here. */
    double x = 1.0, v = 0.0, dt = 1e-6;
    unsigned long steps = 0;
    while (!g_stop) {
        v -= x * dt;
        x += v * dt;
        steps++;
    }
    printf("stopped by signal after %lu steps; x=%.6f energy=%.6f\n",
           steps, x, 0.5 * (x * x + v * v));

    /* Restore defaults so a real Ctrl-C later kills us normally. */
    sa.sa_handler = SIG_DFL;
    sigaction(SIGINT, &sa, NULL);
    g_stop = 0;
}

/* ------------------------------------------------------------------ */
/* 8. Time                                                              */
/* ------------------------------------------------------------------ */
static double mono_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);    /* never jumps; use for durations */
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void demo_time(void) {
    puts("\n== 8. clock_gettime + nanosleep ==");
    struct timespec rt, mono, res;
    clock_gettime(CLOCK_REALTIME, &rt);     /* wall clock; can jump (NTP, user) */
    clock_gettime(CLOCK_MONOTONIC, &mono);
    clock_getres(CLOCK_MONOTONIC, &res);
    printf("REALTIME  = %lld.%09ld  (seconds since 1970-01-01 UTC)\n",
           (long long)rt.tv_sec, rt.tv_nsec);
    printf("MONOTONIC = %lld.%09ld  (seconds since boot-ish; resolution %ld ns)\n",
           (long long)mono.tv_sec, mono.tv_nsec, res.tv_nsec);

    struct timespec req = { .tv_sec = 0, .tv_nsec = 20 * 1000 * 1000 }, rem;
    double t0 = mono_now();
    while (nanosleep(&req, &rem) < 0 && errno == EINTR) req = rem;  /* resume the remainder */
    printf("nanosleep(20 ms) actually slept %.2f ms\n", (mono_now() - t0) * 1e3);
}

/* ------------------------------------------------------------------ */
/* 9. Environment + system limits                                       */
/* ------------------------------------------------------------------ */
static void demo_env(void) {
    puts("\n== 9. getenv / sysconf ==");
    const char *home = getenv("HOME");
    const char *threads = getenv("CH17_THREADS");           /* unset -> NULL */
    printf("HOME=%s   CH17_THREADS=%s\n", home ? home : "(unset)",
           threads ? threads : "(unset; try CH17_THREADS=4 ./ex_demo)");
    printf("page size = %ld bytes, online CPUs = %ld, open-file limit = %ld\n",
           sysconf(_SC_PAGESIZE), sysconf(_SC_NPROCESSORS_ONLN), sysconf(_SC_OPEN_MAX));
}

/* ------------------------------------------------------------------ */
/* 10. Directories                                                      */
/* ------------------------------------------------------------------ */
static void demo_dir(void) {
    puts("\n== 10. opendir / readdir / stat ==");
    const char *dir = ".";
    DIR *d = opendir(dir);
    if (!d) die("opendir");
    int files = 0, dirs = 0;
    long long total = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char path[4096];
        int n = snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (n < 0 || (size_t)n >= sizeof path) continue;      /* truncated: skip, don't trust */
        struct stat st;
        if (stat(path, &st) < 0) continue;  /* d_type is NOT portable; stat is */
        if (S_ISDIR(st.st_mode)) dirs++;
        else if (S_ISREG(st.st_mode)) { files++; total += st.st_size; }
    }
    closedir(d);
    printf("cwd: %d regular files (%lld bytes), %d directories\n", files, total, dirs);
}

/* ------------------------------------------------------------------ */
/* 11. TCP echo server + client on the loopback                          */
/* ------------------------------------------------------------------ */
static void demo_tcp(void) {
    puts("\n== 11. TCP echo: server in a forked child, client in the parent ==");
    fflush(stdout);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { printf("socket() failed (%s) — skipping network demo\n", strerror(errno)); return; }

    /* Without SO_REUSEADDR a restarted server gets EADDRINUSE for ~60 s while the old
     * connection sits in TIME_WAIT. Irrelevant for port 0 but always set it. */
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* 127.0.0.1, network byte order */
    addr.sin_port = htons(0);                        /* 0 = kernel picks a free port */
    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) < 0) {
        printf("bind() failed (%s) — sandbox forbids sockets; compile-checked only\n",
               strerror(errno));
        close(srv);
        return;
    }
    if (listen(srv, 8) < 0) die("listen");

    socklen_t alen = sizeof addr;
    if (getsockname(srv, (struct sockaddr *)&addr, &alen) < 0) die("getsockname");
    unsigned short port = ntohs(addr.sin_port);
    printf("server listening on 127.0.0.1:%u\n", port);

    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (pid == 0) {
        /* ---- server child: accept one client, echo until EOF ---- */
        struct sockaddr_in peer;
        socklen_t plen = sizeof peer;
        int c = accept(srv, (struct sockaddr *)&peer, &plen);
        if (c < 0) _exit(1);
        char buf[512];
        for (;;) {
            ssize_t n = recv(c, buf, sizeof buf, 0);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;                         /* 0 = peer closed */
            if (write_all(c, buf, (size_t)n) < 0) break;
        }
        close(c);
        close(srv);
        _exit(0);
    }

    /* ---- parent = client ---- */
    close(srv);                                        /* the child owns the listener */
    int cl = socket(AF_INET, SOCK_STREAM, 0);
    if (cl < 0) die("client socket");
    if (connect(cl, (struct sockaddr *)&addr, sizeof addr) < 0) die("connect");

    const char *msg = "loss=0.2311 step=42\n";
    if (write_all(cl, msg, strlen(msg)) < 0) die("send");
    char back[128];
    ssize_t n = read_all(cl, back, strlen(msg));       /* echo has the same length */
    if (n < 0) die("recv");
    back[n] = '\0';
    printf("client sent %zu bytes, got back %zd: \"%.*s\"  match=%s\n",
           strlen(msg), n, (int)(n - 1), back, strcmp(back, msg) == 0 ? "yes" : "NO");
    close(cl);                                         /* server sees EOF and exits */
    int st;
    waitpid(pid, &st, 0);
}

/* ------------------------------------------------------------------ */
int main(void) {
    char tmp_path[64];

    demo_fds(tmp_path, sizeof tmp_path);
    demo_mmap_file(tmp_path);
    unlink(tmp_path);                       /* remove the temp file (§1/§2) */
    demo_checkpoint();
    demo_fork();
    demo_pipeline();
    demo_poll();
    demo_signals();
    demo_time();
    demo_env();
    demo_dir();
    demo_tcp();

    puts("\ndone.");
    return 0;
}
