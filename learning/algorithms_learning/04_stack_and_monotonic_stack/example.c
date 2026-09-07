/*
 * example.c — Chapter 04: Stack & monotonic stack — pattern skeletons on tiny inputs.
 *
 * Compile and run:
 *     cc -Wall -Wextra -std=c11 -O2 -o ex_demo example.c -lm && ./ex_demo
 *
 * What is demonstrated (one function per pattern, NOT solutions to the
 * chapter's LeetCode problems — the inputs are neutral demo data):
 *
 *   1. bracket_valid      — three-check bracket matching with a char stack
 *   2. depth_counter      — the same idea collapsed to one int when only depth matters
 *   3. cancel_adjacent    — undo/state-machine stack with a chained cancel loop
 *   4. eval_rpn           — postfix evaluation with an operand stack
 *   5. eval_infix_pm      — infix +/- with brackets: push (result, sign), merge on ')'
 *   6. next_greater       — monotonic decreasing stack of indices, amortised O(n)
 *   7. largest_rect       — increasing stack + sentinel, area = h * (i - L - 1)
 *   8. sum_subarray_mins  — contribution formula value * left * right
 *   9. MinStack           — each node carries min_so_far, growable with realloc
 *  10. TwoStackQueue      — FIFO from two LIFOs, pour only when out is empty
 *
 * Every stack here is "array + size_t top": st[top++] = x pushes, st[--top] pops,
 * st[top-1] peeks. Every peek/pop is guarded by top > 0.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------ */

static void print_ints(const char *label, const int *a, size_t n) {
    printf("%-28s[", label);
    for (size_t i = 0; i < n; i++) printf("%s%d", i ? ", " : "", a[i]);
    printf("]\n");
}

/* xmalloc: abort on allocation failure so the demos stay short. Real code
 * returns an error code instead (see c_learning chapter 06). */
static void *xmalloc(size_t bytes) {
    void *p = malloc(bytes ? bytes : 1);
    if (!p) { fputs("out of memory\n", stderr); exit(1); }
    return p;
}

/* ------------------------------------------------------------------------ */
/* 1. Bracket matching — the three checks                                    */
/* ------------------------------------------------------------------------ */

static bool bracket_valid(const char *s) {
    size_t n = strlen(s);
    char  *st = xmalloc(n);          /* each char is pushed at most once -> n is enough */
    size_t top = 0;
    bool   ok = true;

    for (size_t i = 0; i < n && ok; i++) {
        char c = s[i];
        if (c == '(' || c == '[' || c == '{') {
            st[top++] = c;                                    /* push opener */
        } else {
            char want = (c == ')') ? '(' : (c == ']') ? '[' : '{';
            if (top == 0 || st[top - 1] != want) ok = false;  /* check 1 + check 2 */
            else top--;                                       /* matched: pop */
        }
    }
    ok = ok && top == 0;                                      /* check 3: nothing left open */
    free(st);
    return ok;
}

/* ------------------------------------------------------------------------ */
/* 2. Depth counter — a stack whose content is irrelevant                    */
/* ------------------------------------------------------------------------ */

/* Single bracket type. Returns the minimum number of insertions that would
 * make the string balanced: unmatched closers are counted as they happen,
 * unmatched openers are whatever depth is left at the end. */
static int depth_counter_fixes(const char *s) {
    int depth = 0, fixes = 0;
    for (; *s; s++) {
        if (*s == '(')       depth++;
        else if (depth > 0)  depth--;      /* closer matched an opener */
        else                 fixes++;      /* closer with nothing open: needs an opener */
    }
    return fixes + depth;                  /* leftover openers need closers */
}

/* ------------------------------------------------------------------------ */
/* 3. Undo / state-machine stack — cancel while the top collides             */
/* ------------------------------------------------------------------------ */

/* Removes adjacent equal pairs repeatedly ("abbaccd" -> "d").
 * Writes the survivors into out (capacity >= strlen(s) + 1) and returns the length. */
static size_t cancel_adjacent(const char *s, char *out) {
    size_t top = 0;
    for (; *s; s++) {
        if (top > 0 && out[top - 1] == *s) top--;     /* (b) cancel the top      */
        else                               out[top++] = *s;   /* (a) push       */
        /* The chaining happens for free: after a cancel, the next character is
         * compared against whatever was underneath, which may cancel again.  */
    }
    out[top] = '\0';                                  /* char stacks are not strings until you do this */
    return top;
}

/* ------------------------------------------------------------------------ */
/* 4. RPN (postfix) evaluation — operand stack                               */
/* ------------------------------------------------------------------------ */

/* Returns 0 on success and stores the result in *out; nonzero on malformed input. */
static int eval_rpn(const char *const *tok, size_t n, long *out) {
    long  *st = xmalloc(n * sizeof *st);
    size_t top = 0;
    int    err = 0;

    for (size_t i = 0; i < n && !err; i++) {
        const char *t = tok[i];
        bool is_op = (strlen(t) == 1 && strchr("+-*/", t[0]) != NULL); /* "-3" is a number */
        if (!is_op) {
            st[top++] = strtol(t, NULL, 10);
        } else if (top < 2) {
            err = 1;
        } else {
            long b = st[--top];          /* right operand (pushed last) */
            long a = st[--top];          /* left operand                */
            switch (t[0]) {
            case '+': st[top++] = a + b; break;
            case '-': st[top++] = a - b; break;
            case '*': st[top++] = a * b; break;
            default:  if (b == 0) err = 1; else st[top++] = a / b; break; /* C truncates toward 0 */
            }
        }
    }
    if (!err && top == 1) *out = st[0]; else err = 1;
    free(st);
    return err;
}

/* ------------------------------------------------------------------------ */
/* 5. Infix +/- with brackets — push (result, sign) on '(' and merge on ')'  */
/* ------------------------------------------------------------------------ */

/* Evaluates expressions like "1 - (2 + 3) + 10". Digits, + - ( ) and spaces only.
 * The same skeleton, with a different payload, decodes "3[a2[c]]" or scores brackets. */
static long eval_infix_pm(const char *s) {
    size_t n = strlen(s);
    long *res_st  = xmalloc(n * sizeof *res_st);    /* outer results  */
    int  *sign_st = xmalloc(n * sizeof *sign_st);   /* outer signs    */
    size_t top = 0;

    long result = 0, num = 0;
    int  sign = 1;
    bool have_num = false;

    for (size_t i = 0; i <= n; i++) {              /* i == n acts as an end-of-string "operator" */
        char c = (i < n) ? s[i] : '\0';
        if (c >= '0' && c <= '9') {
            num = num * 10 + (c - '0');            /* accumulate multi-digit numbers */
            have_num = true;
            continue;
        }
        if (have_num) { result += sign * num; num = 0; have_num = false; }  /* commit */

        if (c == '+') sign = 1;
        else if (c == '-') sign = -1;
        else if (c == '(') {
            res_st[top] = result; sign_st[top] = sign; top++;   /* save outer context */
            result = 0; sign = 1;                               /* fresh inner scope  */
        } else if (c == ')') {
            if (top > 0) {
                top--;
                result = res_st[top] + sign_st[top] * result;   /* merge inner into outer */
            }
        }
        /* spaces and '\0' fall through */
    }
    free(res_st);
    free(sign_st);
    return result;
}

/* ------------------------------------------------------------------------ */
/* 6. Monotonic stack — next strictly greater element (index or -1)         */
/* ------------------------------------------------------------------------ */

static void next_greater(const int *a, size_t n, int *ans) {
    size_t *st = xmalloc(n * sizeof *st);     /* stack of INDICES, values decreasing bottom->top */
    size_t  top = 0;
    size_t  pushes = 0, pops = 0;             /* just to print the amortised argument */

    for (size_t i = 0; i < n; i++) {
        while (top > 0 && a[st[top - 1]] < a[i]) {   /* top has found its next greater */
            ans[st[top - 1]] = (int)i;
            top--; pops++;
        }
        st[top++] = i; pushes++;                      /* i now waits for its own answer */
    }
    while (top > 0) ans[st[--top]] = -1;              /* leftovers never found one */
    printf("%-28s%zu pushes, %zu pops (pops <= pushes -> O(n))\n", "  work:", pushes, pops);
    free(st);
}

/* ------------------------------------------------------------------------ */
/* 7. Monotonic stack — largest rectangle under a histogram                  */
/* ------------------------------------------------------------------------ */

static long long largest_rect(const int *h, size_t n) {
    size_t *st = xmalloc((n + 1) * sizeof *st);   /* increasing stack of indices */
    size_t  top = 0;
    long long best = 0;

    for (size_t i = 0; i <= n; i++) {
        int cur = (i == n) ? 0 : h[i];             /* virtual 0-height sentinel at i == n */
        while (top > 0 && h[st[top - 1]] > cur) {  /* strict >: equal bars stay, leftmost gets full width */
            size_t k = st[--top];
            size_t width = (top == 0) ? i : i - st[top - 1] - 1;   /* walls at L and i excluded */
            long long area = (long long)h[k] * (long long)width;   /* widen BEFORE multiplying */
            if (area > best) best = area;
        }
        if (i < n) st[top++] = i;
    }
    free(st);
    return best;
}

/* ------------------------------------------------------------------------ */
/* 8. Monotonic stack — contribution sum: value * left_span * right_span     */
/* ------------------------------------------------------------------------ */

/* Sum over all subarrays of their minimum. Strict on the left, non-strict on
 * the right so runs of equal values are attributed exactly once. */
static long long sum_subarray_mins(const int *a, size_t n) {
    size_t *st   = xmalloc((n + 1) * sizeof *st);
    long   *left = xmalloc((n + 1) * sizeof *left);   /* left[i]  = i - (prev strictly smaller) */
    size_t  top = 0;
    long long total = 0;

    for (size_t i = 0; i <= n; i++) {
        /* Sentinel: a value smaller than everything drains the stack at i == n.
         * We simulate it with the flag `is_end` instead of a real value.       */
        bool is_end = (i == n);
        while (top > 0 && (is_end || a[st[top - 1]] >= a[i])) {  /* >= : pop equal too (non-strict right) */
            size_t k = st[--top];
            long right = (long)(i - k);                            /* right[k] = i - k */
            total += (long long)a[k] * left[k] * right;
        }
        if (!is_end) {
            left[i] = (top == 0) ? (long)i + 1 : (long)(i - st[top - 1]);  /* strict left */
            st[top++] = i;
        }
    }
    free(left);
    free(st);
    return total;
}

/* ------------------------------------------------------------------------ */
/* 9. MinStack — each node carries the minimum so far                        */
/* ------------------------------------------------------------------------ */

typedef struct { int value, min_so_far; } MinNode;
typedef struct { MinNode *data; size_t len, cap; } MinStack;

static void ms_init(MinStack *s) { s->data = NULL; s->len = s->cap = 0; }
static void ms_free(MinStack *s) { free(s->data); ms_init(s); }

static int ms_push(MinStack *s, int v) {
    if (s->len == s->cap) {
        size_t ncap = s->cap ? 2 * s->cap : 4;
        MinNode *t = realloc(s->data, ncap * sizeof *t);
        if (!t) return -1;
        s->data = t; s->cap = ncap;
    }
    int m = s->len ? s->data[s->len - 1].min_so_far : v;
    s->data[s->len].value = v;
    s->data[s->len].min_so_far = (v < m) ? v : m;     /* snapshot, not a global */
    s->len++;
    return 0;
}
/* Preconditions: non-empty. Callers check s->len first. */
static void ms_pop(MinStack *s)       { if (s->len) s->len--; }
static int  ms_top(const MinStack *s) { return s->data[s->len - 1].value; }
static int  ms_min(const MinStack *s) { return s->data[s->len - 1].min_so_far; }

/* ------------------------------------------------------------------------ */
/* 10. Queue from two stacks — pour only when `out` is empty                 */
/* ------------------------------------------------------------------------ */

#define TSQ_CAP 16
typedef struct {
    int in[TSQ_CAP],  out[TSQ_CAP];
    size_t n_in, n_out;
} TwoStackQueue;

static void tsq_init(TwoStackQueue *q) { q->n_in = q->n_out = 0; }
static bool tsq_empty(const TwoStackQueue *q) { return q->n_in == 0 && q->n_out == 0; }
static bool tsq_push(TwoStackQueue *q, int x) {
    if (q->n_in + q->n_out >= TSQ_CAP) return false;
    q->in[q->n_in++] = x;
    return true;
}
/* Moves everything from in to out (reversing order) — but ONLY if out is empty,
 * otherwise two batches would interleave and FIFO order would break. */
static void tsq_pour(TwoStackQueue *q) {
    if (q->n_out == 0)
        while (q->n_in > 0) q->out[q->n_out++] = q->in[--q->n_in];
}
static bool tsq_pop(TwoStackQueue *q, int *x) {
    tsq_pour(q);
    if (q->n_out == 0) return false;
    *x = q->out[--q->n_out];
    return true;
}

/* ------------------------------------------------------------------------ */
/* main — run every skeleton on a tiny input                                 */
/* ------------------------------------------------------------------------ */

int main(void) {
    puts("== 1. bracket matching (three checks) ==");
    const char *br[] = { "{[()]}", "{[()]}(", ")(", "([)]", "" };
    for (size_t i = 0; i < sizeof br / sizeof *br; i++)
        printf("  %-10s -> %s\n", br[i][0] ? br[i] : "\"\"", bracket_valid(br[i]) ? "valid" : "INVALID");

    puts("\n== 2. depth counter (single bracket type, min insertions) ==");
    const char *dc[] = { "())", "(((", "()))((", "()" };
    for (size_t i = 0; i < sizeof dc / sizeof *dc; i++)
        printf("  %-8s -> %d insertion(s)\n", dc[i], depth_counter_fixes(dc[i]));

    puts("\n== 3. undo stack: cancel adjacent equal pairs (chained) ==");
    {
        const char *s = "abbaccd";
        char out[16];
        size_t len = cancel_adjacent(s, out);
        printf("  %s -> \"%s\" (len %zu)\n", s, out, len);
    }

    puts("\n== 4. RPN evaluation ==");
    {
        const char *const rpn[] = { "2", "1", "+", "3", "*" };          /* (2+1)*3 = 9  */
        const char *const rpn2[] = { "4", "13", "5", "/", "+" };        /* 4 + 13/5 = 6 */
        const char *const rpn3[] = { "3", "-4", "-" };                  /* 3 - (-4) = 7 */
        long r;
        if (eval_rpn(rpn,  5, &r) == 0) printf("  2 1 + 3 *    = %ld\n", r);
        if (eval_rpn(rpn2, 5, &r) == 0) printf("  4 13 5 / +   = %ld\n", r);
        if (eval_rpn(rpn3, 3, &r) == 0) printf("  3 -4 -       = %ld   (\"-4\" is a number, not an operator)\n", r);
    }

    puts("\n== 5. infix +/- with brackets: (result, sign) stack ==");
    {
        const char *ex[] = { "1 - (2 + 3)", "(1+(4+5+2)-3)+(6+8)", "10 - (2 - (3 - 4))", "42" };
        for (size_t i = 0; i < sizeof ex / sizeof *ex; i++)
            printf("  %-22s = %ld\n", ex[i], eval_infix_pm(ex[i]));
    }

    puts("\n== 6. monotonic stack: next strictly greater element (as index, -1 if none) ==");
    {
        int a[] = { 2, 1, 2, 4, 3 };
        size_t n = sizeof a / sizeof *a;
        int ans[5];
        print_ints("  a:", a, n);
        next_greater(a, n, ans);
        print_ints("  next greater index:", ans, n);
        /* Distances ("days to wait") are ans[i] - i when ans[i] != -1. */
        int dist[5];
        for (size_t i = 0; i < n; i++) dist[i] = ans[i] < 0 ? 0 : ans[i] - (int)i;
        print_ints("  distance:", dist, n);
    }

    puts("\n== 7. monotonic stack: largest rectangle in histogram ==");
    {
        int h[] = { 2, 1, 5, 6, 2, 3 };
        size_t n = sizeof h / sizeof *h;
        print_ints("  heights:", h, n);
        printf("%-28s%lld   (bars 5 and 6, width 2)\n", "  largest area:", largest_rect(h, n));
        int flat[] = { 3, 3, 3 };
        printf("%-28s%lld   (equal bars: leftmost one takes full width)\n",
               "  [3,3,3] area:", largest_rect(flat, 3));
    }

    puts("\n== 8. monotonic stack: sum of subarray minimums (contribution formula) ==");
    {
        int a[] = { 3, 1, 2, 4 };
        size_t n = sizeof a / sizeof *a;
        /* Brute force check: subarrays [3][1][2][4][3,1][1,2][2,4][3,1,2][1,2,4][3,1,2,4]
         * minimums 3+1+2+4+1+1+2+1+1+1 = 17 */
        long long brute = 0;
        for (size_t i = 0; i < n; i++) {
            int m = a[i];
            for (size_t j = i; j < n; j++) { if (a[j] < m) m = a[j]; brute += m; }
        }
        print_ints("  a:", a, n);
        printf("%-28s%lld   (brute force: %lld)\n", "  sum of minimums:", sum_subarray_mins(a, n), brute);
        int dup[] = { 2, 2, 2 };
        printf("%-28s%lld   (6 subarrays, all min 2 -> 12; no double counting)\n",
               "  [2,2,2]:", sum_subarray_mins(dup, 3));
    }

    puts("\n== 9. MinStack: (value, min_so_far) per node ==");
    {
        MinStack s; ms_init(&s);
        int ops[] = { 5, 3, 7, 2 };
        for (size_t i = 0; i < 4; i++) {
            if (ms_push(&s, ops[i]) != 0) { fputs("push failed\n", stderr); ms_free(&s); return 1; }
            printf("  push %d -> top %d, min %d\n", ops[i], ms_top(&s), ms_min(&s));
        }
        while (s.len > 0) {
            int t = ms_top(&s), m = ms_min(&s);
            ms_pop(&s);
            printf("  pop %d  -> min before pop was %d%s\n", t, m,
                   s.len ? "" : " (stack now empty)");
        }
        ms_free(&s);
    }

    puts("\n== 10. queue from two stacks (pour only when out is empty) ==");
    {
        TwoStackQueue q; tsq_init(&q);
        int x;
        tsq_push(&q, 1); tsq_push(&q, 2); tsq_push(&q, 3);
        tsq_pop(&q, &x); printf("  push 1,2,3; pop -> %d\n", x);
        tsq_push(&q, 4);                        /* lands in `in` while `out` still holds 3,2 */
        tsq_pop(&q, &x); printf("  push 4; pop -> %d   (not 4: out was non-empty, so no pour yet)\n", x);
        tsq_pop(&q, &x); printf("  pop -> %d\n", x);
        tsq_pop(&q, &x); printf("  pop -> %d   (out emptied, in was poured)\n", x);
        printf("  empty now: %s\n", tsq_empty(&q) ? "yes" : "no");
    }

    return 0;
}
