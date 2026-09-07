# 16 — asm.md: atomics are instructions, and a data race is the absence of them

Companion to `lesson.md` §8–10. Prerequisite: `00_arm64_assembly_primer/lesson.md`,
`05_pointers/asm.md`.
Output is `cc -O1 -S -o -` on this machine; noise lines removed. Where marked,
`-mcpu=apple-a7` is used to target ARMv8.0 (no LSE) — see "Two generations" below.

## What to look for

* `x++` on a plain `long` is `ldr` / `add` / `str`. **Three** instructions, and
  any of the gaps between them is where the other thread's update gets lost.
  A data race is not a special instruction — it is ordinary code, which is
  exactly why it is invisible.
* An atomic RMW is either one LSE instruction (`ldadd`, `swp`, `cas`) or a
  **retry loop** of `ldxr` … `stxr` … `cbnz`. Learn the loop shape: you will
  recognise it in every lock-free structure you ever read.
* Memory order is spelled in the *mnemonic suffix*: `ldr`/`ldar`/`ldapr`,
  `str`/`stlr`, `ldadd`/`ldadda`/`ldaddl`/`ldaddal`. `a` = acquire, `l` = release.
* `memory_order_relaxed` is genuinely free on the load/store side — it costs the
  same instruction as non-atomic. What you pay for is the *ordering*, not the
  atomicity.
* `atomic_thread_fence` is `dmb ish`. A standalone barrier, no data attached.
* A CAS loop is `load; compute; cas; compare; branch back`. Note the loop body is
  re-executed on failure — this is why the "compute" part must be side-effect free.

## The C

```c
#include <stdatomic.h>
long plain_inc(long *p)           { return ++*p; }                    /* RACY */
long relaxed_inc(_Atomic long *p) { return atomic_fetch_add_explicit(p,1,memory_order_relaxed); }
long seq_inc(_Atomic long *p)     { return atomic_fetch_add(p,1); }
long ld_relaxed(_Atomic long *p)  { return atomic_load_explicit(p, memory_order_relaxed); }
long ld_acquire(_Atomic long *p)  { return atomic_load_explicit(p, memory_order_acquire); }
long ld_seq    (_Atomic long *p)  { return atomic_load(p); }
void st_relaxed(_Atomic long *p, long v){ atomic_store_explicit(p,v,memory_order_relaxed); }
void st_release(_Atomic long *p, long v){ atomic_store_explicit(p,v,memory_order_release); }
long xchg(_Atomic long *p, long v){ return atomic_exchange(p, v); }
void fence(void)                  { atomic_thread_fence(memory_order_seq_cst); }

long cas_loop(_Atomic long *p, long add) {
    long old = atomic_load_explicit(p, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(p, &old, old + add,
              memory_order_acq_rel, memory_order_relaxed)) { }
    return old;
}
struct Node { struct Node *next; };
void push(_Atomic(struct Node*) *head, struct Node *n) {   /* Treiber stack */
    n->next = atomic_load_explicit(head, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(head, &n->next, n,
              memory_order_release, memory_order_relaxed)) { }
}
```

## The assembly

### A data race is three ordinary instructions

```
_plain_inc:                              ; ++*p on a plain long
	ldr	x8, [x0]                 ; read
	add	x8, x8, #1               ;   ← another thread can run its whole ++ HERE
	str	x8, [x0]                 ; write back a value computed from a stale read
	mov	x0, x8
	ret
```

There is nothing wrong-looking about this. No warning, no special instruction, no
trap. If two threads execute it simultaneously on the same address, both read the
same value and both store `value+1`, and one increment vanishes. This is the
whole of the lost-update bug, and the asm is the clearest possible explanation of
it: **`++` is not one thing**.

This is also why ThreadSanitizer exists. Nothing in the instruction stream marks
this as a race, so the only way to find it is to instrument every access and
track happens-before at runtime. `-fsanitize=thread` does exactly that.

### Two generations of atomic RMW

ARMv8.1 added **LSE** (Large System Extensions): single-instruction atomics.
Apple silicon has them, so this is the default output:

```
_relaxed_inc:                            ; fetch_add, relaxed
	mov	w8, #1
	ldadd	x8, x0, [x0]             ; atomically: x0 = *p; *p = x0 + x8
	ret
_seq_inc:                                ; fetch_add, seq_cst
	mov	w8, #1
	ldaddal	x8, x0, [x0]             ; same, plus Acquire and reLease semantics
	ret
_xchg:	swpal	x1, x0, [x0]             ; atomic_exchange: swap, acquire+release
```

One instruction. Read the suffix and you have read the memory order:
`ldadd` (relaxed) / `ldadda` (acquire) / `ldaddl` (release) / `ldaddal` (seq_cst).
The same `a`/`l` pattern applies to `swp`, `cas`, `ldset`, `ldclr`, `ldeor`.

Now compile the identical source for ARMv8.0 (`-mcpu=apple-a7`) and the classic
**load-linked / store-conditional** loop appears:

```
_relaxed_inc:                            ; -mcpu=apple-a7
	mov	x8, x0
LBB1_1:
	ldxr	x0, [x8]                 ; Load eXclusive:  read AND start monitoring this address
	add	x9, x0, #1               ; compute off to the side
	stxr	w10, x9, [x8]            ; STore eXclusive: succeeds only if nobody touched it;
                                         ;   w10 = 0 on success, 1 on failure
	cbnz	w10, LBB1_1              ; failed? do the whole thing again
	ret

_seq_inc:                                ; the same, with ordering
LBB2_1:
	ldaxr	x0, [x8]                 ; ldxr + Acquire
	add	x9, x0, #1
	stlxr	w10, x9, [x8]            ; stxr + reLease
	cbnz	w10, LBB2_1
```

**This shape is worth memorising.** `ldxr` / modify / `stxr` / `cbnz`-retry is the
universal lock-free idiom. The CPU sets an "exclusive monitor" on the cache line
at `ldxr`; any other core's write to that line clears it, and `stxr` then fails
and returns 1 instead of storing. Atomicity is not achieved by locking the bus —
it is achieved by *detecting interference and retrying*. That is why atomics get
slower under contention: the retry loop spins. It is also why you must keep the
body between `ldxr` and `stxr` tiny (no calls, no other memory access) — the
monitor is easily cleared, and a long body can livelock.

`clrex` in the listings below is the cleanup: "abandon the monitor", used when
the CAS bails out without storing.

### Memory order is a suffix, and relaxed is free

```
_ld_relaxed:	ldr	x0, [x0]         ; …a PLAIN load. Identical to non-atomic.
_ld_acquire:	ldapr	x0, [x0]         ; Load-Acquire (RCpc): nothing after may move before
_ld_seq:	ldar	x0, [x0]         ; Load-Acquire (RCsc): the stronger form for seq_cst

_st_relaxed:	str	x1, [x0]         ; …a PLAIN store.
_st_release:	stlr	x1, [x0]         ; STore-reLease: nothing before may move after

_fence:		dmb	ish              ; Data Memory Barrier, Inner SHareable domain
```

Three things follow:

**1. Relaxed atomics cost nothing but the guarantee of no tearing.** `ldr`/`str`
of an aligned word is already indivisible on this hardware. What `_Atomic` bought
you at relaxed order is the promise the *compiler* will not split it, invent
extra reads, hoist it out of a loop, or fuse two of them. That is not nothing —
it is the whole reason a plain `while (!done) {}` on a non-atomic flag can spin
forever while the atomic version terminates.

**2. Acquire/release are not fences, they are annotated accesses.** `ldar` and
`stlr` are single instructions that carry ordering with them. This is cheaper and
more precise than a standalone `dmb`, and it is why C11's per-operation memory
orders map so well onto ARM.

**3. The classic publish pattern reads directly.** Thread A writes the data with
plain stores then `stlr` a ready flag; thread B `ldar`s the flag then reads the
data with plain loads. The `stlr` guarantees the data stores are visible before
the flag; the `ldar` guarantees the data loads happen after seeing the flag. Two
instructions, and that is the entire happens-before edge.

x86 gets acquire/release for free (its hardware model is already that strong) and
so hides these bugs; ARM does not. Code that "works on my Intel laptop" and fails
on an M-series chip is usually a missing acquire or release, and this is where it
becomes visible.

### The CAS loop

With LSE, `compare_exchange` is one instruction plus bookkeeping:

```
_cas_loop:                               ; fetch_add written by hand as a CAS loop
	ldr	x8, [x0]                 ; old = relaxed load
	mov	x9, x8
LBB12_1:
	add	x10, x8, x1              ; desired = old + add        ← recomputed each retry
	casal	x9, x10, [x0]            ; if (*p == x9) *p = x10;  x9 = the OLD *p either way
	cmp	x9, x8                   ; did it match what we expected?
	mov	x8, x9                   ; on failure, `old` becomes the value we actually saw
	b.ne	LBB12_1                  ; ...and retry
	mov	x0, x8
	ret
```

Note how faithfully this mirrors the C: `compare_exchange` writing the observed
value back into `expected` is the `mov x8, x9`, and that is what makes the retry
loop converge instead of spinning on a stale expectation. Note also that `add
x10, x8, x1` is **inside** the loop: the desired value is recomputed from the
freshly observed one. Any expression you put there runs an unbounded number of
times, so it must be pure.

On ARMv8.0 the same source becomes an `ldaxr`/`stlxr` loop with a `clrex` on the
mismatch path:

```
LBB12_2:                                 ; -mcpu=apple-a7
	ldaxr	x8, [x0]                 ; load-exclusive + acquire
	cmp	x8, x9                   ; compare with expected
	b.ne	LBB12_1                  ; mismatch -> go clrex and retry with the new value
	add	x9, x9, x1
	stlxr	w10, x9, [x0]            ; store-exclusive + release
	cmp	w10, #0
	csetm	w10, eq
	mov	x9, x8
	tbz	w10, #0, LBB12_2         ; store failed -> retry
LBB12_1:
	mov	w10, #0
	clrex                            ; give up the exclusive monitor
	mov	x9, x8
	tbnz	w10, #0, LBB12_4
```

Two failure paths — "the value was not what I expected" and "someone touched the
line while I held the monitor" — and only the first is visible in C. That second
one is precisely what `compare_exchange_**weak**` is for: `weak` is allowed to
report spurious failure, so the compiler can leave the `stxr` failure as a plain
retry instead of adding an inner loop. In a loop you were going to write anyway,
`weak` is free and `strong` costs you a nested retry.

### The Treiber stack push

```
_push:                                   ; n->next = head; while (!CAS(head, &n->next, n));
	ldr	x9, [x0]                 ; relaxed load of head
	str	x9, [x1]                 ; n->next = head        (plain store, n is thread-private)
	mov	x8, x9
	casl	x8, x1, [x0]             ; CAS with reLease only (no acquire needed on push)
	cmp	x8, x9
	b.eq	LBB13_3                  ; won on the first try — the common case, no loop
	mov	x9, x8
LBB13_2:
	str	x8, [x1]                 ; retry: n->next = the head we actually observed
	casl	x9, x1, [x0]
	cmp	x9, x8
	mov	x8, x9
	b.ne	LBB13_2
LBB13_3:
	ret
```

`casl` — release, no acquire — because `push` only *publishes*: the `str x9, [x1]`
writing `n->next` must be visible to whoever later pops this node. The compiler
peeled the first attempt out of the loop, betting on no contention. And the whole
ABA problem lives in one instruction here: `casl` compares the *pointer value*,
so a head that was popped and pushed back looks unchanged, and this code cannot
tell. Nothing in the asm can help you with that; it is an algorithm problem.

## Read it yourself

1. Write `void spin(_Atomic int *flag) { while (atomic_load_explicit(flag,
   memory_order_acquire)) {} }`, then the same with a plain `int *`. Compare the
   loops. Which one can hoist the load out and spin forever, and what instruction
   is (or is not) inside the loop body?
2. Compile `relaxed_inc` for `int`, `long`, and `char`. Predict the register
   width and the instruction suffix (`ldaddb`? `ldaddh`?) before checking.
3. Take `cas_loop` and change `compare_exchange_weak` to `_strong`. Diff at
   `-mcpu=apple-a7`. Where does the extra nested loop appear, and which failure
   mode does it now hide?
4. Write the release/acquire publish pattern as two functions — `publish(data,
   flag)` and `consume(data, flag)` — and identify the `stlr` and the `ldar`.
   Then downgrade both to `relaxed` and note that the *only* change is two
   instruction suffixes. Explain the bug you just introduced.
5. Put `atomic_fetch_add(&counter, 1)` inside a loop and compile with `-O2`.
   Does the compiler combine N increments into one `fetch_add(N)`? Try the same
   with a plain `long`. Explain the difference in terms of what `_Atomic`
   forbids the compiler from doing.

## Takeaways

* `x++` on a shared plain variable is `ldr`/`add`/`str` — three instructions with
  two windows for another thread. A data race looks exactly like correct code,
  which is why you need TSan rather than review.
* An atomic RMW is one LSE instruction (`ldadd`, `swp`, `cas`) or an
  `ldxr`…`stxr`…`cbnz` retry loop. The retry loop is the shape of all lock-free
  code; keep its body tiny.
* Memory order lives in the mnemonic suffix: `a` = acquire, `l` = release.
  `ldar`/`ldapr`, `stlr`, `ldaddal`, `casl`. Read the suffix, read the ordering.
* Relaxed atomic load/store compile to plain `ldr`/`str`. You pay for ordering,
  not atomicity — but you still gain the promise that the *compiler* will not
  duplicate, hoist, or split the access.
* `atomic_thread_fence` is `dmb ish`; prefer annotated accesses (`ldar`/`stlr`)
  over standalone barriers when you can.
* ARM's weak model makes missing acquire/release *visible*; x86 hides them. Test
  concurrent code on ARM.
