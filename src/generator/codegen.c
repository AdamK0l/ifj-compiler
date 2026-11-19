/*
 * Projekt IFJ26 -  IFJ26/IAL compiler to IFJcode26
 * codegen.c - generator final code IFJcode26 from AST
 *
 * Autoři: <Jiří Janásek, xjanasj00>
 *
 * - CORE IDEAS
 *
 *   - Expressions are evaluated on the data stack: gen_expr() always leaves
 *     exactly one value on top (ADDS, LTS, ... operate directly on the stack).
 *   - The calling convention is unified across user functions, lambdas,
 *     extension methods, and built-in functions: the caller pushes arguments
 *     left-to-right onto the stack, executes CREATEFRAME, and calls CALL.
 *     The callee executes PUSHFRAME, DEFVAR for all variables, POPS
 *     parameters in reverse order, runs the body, leaves the result on the
 *     stack, executes POPFRAME, and returns with RETURN. Every function returns
 *     exactly one value; Unit is represented as nil@nil.
 *   - A function value (lambda, function reference) is a string containing
 *     the target label name and is invoked via CALL with a variable
 *     (supported by IFJcode26).
 *   - Code for each function is built in its own buffer because DEFVAR for
 *     all local and temporary variables must be emitted at the start of the
 *     function (emitting DEFVAR inside a loop would fail on the second pass).
 *   - Variables are named LF@<name>$<n>, so name shadowing does not break
 *     anything.
 *   - Temporary variables GF@$t1..$t4 and GF@$discard are only ever used
 *     immediately (never across a function call), so recursion is unaffected.
 */

#include "codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef CODEGEN_COMMENTS
#define CODEGEN_COMMENTS 1 /* 1 = comments into the output (#) */
#endif

#define ERR_INTERNAL 99
#define K_NONE (-1) /* unknown type */

#define T1 "GF@$t1"
#define T2 "GF@$t2"
#define T3 "GF@$t3"
#define T4 "GF@$t4"
#define DISCARD "GF@$discard"

/* ------------------------------------------------------------ memory and buffer */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

/**
 * Report out of memory (internal error, exit code 99) and terminate.
 */
static void oom(void) {
    fputs("codegen: nedostatek pameti\n", stderr);
    exit(ERR_INTERNAL);
}

/**
 * realloc() that never returns NULL (terminates via oom() on failure).
 * @param p  block to resize (NULL allocates)
 * @param n  new size in bytes
 * @return   resized block
 */
static void *xrealloc(void *p, size_t n) {
    void *r = realloc(p, n);
    if (r == NULL) {
        oom();
    }
    return r;
}

/**
 * Append n bytes to a buffer (kept NUL-terminated).
 * @param b  buffer
 * @param s  bytes to append
 * @param n  number of bytes
 */
static void buf_putn(Buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 256;
        while (cap < b->len + n + 1) {
            cap *= 2;
        }
        b->data = xrealloc(b->data, cap);
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

/**
 * Append a C string to a buffer.
 * @param b  buffer
 * @param s  string
 */
static void buf_puts(Buf *b, const char *s) {
    buf_putn(b, s, strlen(s));
}

/**
 * Append printf-style text (va_list variant) to a buffer.
 * @param b    buffer
 * @param fmt  format
 * @param ap   arguments
 */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 0)))
#endif
static void buf_vprintf(Buf *b, const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    if (n < 0) {
        va_end(ap2);
        return;
    }
    char *tmp = xrealloc(NULL, (size_t)n + 1);
    vsnprintf(tmp, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    buf_putn(b, tmp, (size_t)n);
    free(tmp);
}

/**
 * Append printf-style text to a buffer.
 * @param b    buffer
 * @param fmt  format
 */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
static void buf_printf(Buf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(b, fmt, ap);
    va_end(ap);
}

/**
 * Format text into a new heap string.
 * @param fmt  printf format
 * @return     string, the caller frees it
 */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
static char *xfmt(const char *fmt, ...) {
    Buf b = {0};
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(&b, fmt, ap);
    va_end(ap);
    if (b.data == NULL) {
        buf_puts(&b, "");
    }
    return b.data;
}

/**
 * Write an IFJcode26 constant string@... (chars 0-32, 35, 92 and non-ASCII as \ddd).
 * @param b    output buffer
 * @param s    decoded string bytes
 * @param len  number of bytes
 */
static void put_string_const(Buf *b, const char *s, size_t len) {
    buf_puts(b, "string@");
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c <= 32 || c == 35 || c == 92 || c > 126) {
            buf_printf(b, "\\%03d", c);
        } else {
            buf_putn(b, (const char *)&c, 1);
        }
    }
}