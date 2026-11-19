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

/* ------------------------------------------------------------ types */

static const AstType TY_INT = {AST_TYPE_INT, {NULL, 0, NULL}};
static const AstType TY_DOUBLE = {AST_TYPE_DOUBLE, {NULL, 0, NULL}};
static const AstType TY_STRING = {AST_TYPE_STRING, {NULL, 0, NULL}};
static const AstType TY_BOOL = {AST_TYPE_BOOLEAN, {NULL, 0, NULL}};
static const AstType TY_UNIT = {AST_TYPE_UNIT, {NULL, 0, NULL}};

/**
 * Kind of a type as int.
 * @param t  type, may be NULL
 * @return   type kind, or K_NONE for NULL
 */
static int kind_of(const AstType *t) {
    return t ? (int)t->kind : K_NONE;
}

/**
 * Shared static object of a primitive type.
 * @param kind  AstTypeKind
 * @return      the type, or NULL for function/unknown
 */
static const AstType *prim_type(int kind) {
    switch (kind) {
    case AST_TYPE_INT:
        return &TY_INT;
    case AST_TYPE_DOUBLE:
        return &TY_DOUBLE;
    case AST_TYPE_STRING:
        return &TY_STRING;
    case AST_TYPE_BOOLEAN:
        return &TY_BOOL;
    case AST_TYPE_UNIT:
        return &TY_UNIT;
    default:
        return NULL;
    }
}

/**
 * Short printable name of a type kind.
 * @param kind  AstTypeKind
 * @return      "Int", "Double", "String", "Boolean", "Unit" or "Fn"
 */
static const char *kind_name(int kind) {
    switch (kind) {
    case AST_TYPE_INT:
        return "Int";
    case AST_TYPE_DOUBLE:
        return "Double";
    case AST_TYPE_STRING:
        return "String";
    case AST_TYPE_BOOLEAN:
        return "Boolean";
    case AST_TYPE_UNIT:
        return "Unit";
    default:
        return "Fn";
    }
}

/**
 * Structural type equality.
 * @param a  first type, may be NULL
 * @param b  second type, may be NULL
 * @return   true if both describe the same type
 */
static bool type_equal(const AstType *a, const AstType *b) {
    if (a == NULL || b == NULL) {
        return a == b;
    }
    if (a->kind != b->kind) {
        return false;
    }
    if (a->kind != AST_TYPE_FUNCTION) {
        return true;
    }
    if (a->function.parameter_count != b->function.parameter_count) {
        return false;
    }
    for (size_t i = 0; i < a->function.parameter_count; i++) {
        if (!type_equal(a->function.parameters[i], b->function.parameters[i])) {
            return false;
        }
    }
    return type_equal(a->function.result, b->function.result);
}

/* ------------------------------------------------------------ built-in functions */

enum { PK_ANY, PK_INT, PK_DOUBLE, PK_STRING };

typedef struct {
    const char *name;
    int nparams;
    int params[3];
    int ret; /* AstTypeKind result */
    const char *code;
} Builtin;

enum {
    BI_PRINT,
    BI_READ_LINE,
    BI_READ_INT,
    BI_READ_DOUBLE,
    BI_READ_BOOLEAN,
    BI_TO_STR,
    BI_TO_INT,
    BI_STR_LENGTH,
    BI_SUB_STR,
    BI_ORD,
    BI_CHR,
    BI_FLOOR, /* only as member .floor (VALMEM) */
    BI_COUNT
};

/* Built-in functions written directly in IFJcode26. They use the same calling
 * convention as user-defined functions. Only those actually used by the program
 * are included in the output. */
static const Builtin BUILTINS[BI_COUNT] = {
    [BI_PRINT] = {"print", 1, {PK_ANY}, AST_TYPE_UNIT,
                  "LABEL $ifj$print\n"
                  "PUSHFRAME\n"
                  "DEFVAR LF@v\n"
                  "DEFVAR LF@t\n"
                  "POPS LF@v\n"
                  "TYPE LF@t LF@v\n"
                  "JUMPIFNEQ $ifj$print$w LF@t string@nil\n"
                  "WRITE string@()\n"
                  "JUMP $ifj$print$e\n"
                  "LABEL $ifj$print$w\n"
                  "WRITE LF@v\n"
                  "LABEL $ifj$print$e\n"
                  "PUSHS nil@nil\n"
                  "POPFRAME\n"
                  "RETURN\n"},

    [BI_READ_LINE] = {"readLine", 0, {0}, AST_TYPE_STRING,
                      "LABEL $ifj$readLine\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@v\n"
                      "DEFVAR LF@t\n"
                      "READ LF@v string\n"
                      "TYPE LF@t LF@v\n"
                      "JUMPIFNEQ $ifj$readLine$e LF@t string@nil\n"
                      "MOVE LF@v string@\n"
                      "LABEL $ifj$readLine$e\n"
                      "PUSHS LF@v\n"
                      "POPFRAME\n"
                      "RETURN\n"},

    [BI_READ_INT] = {"readInt", 0, {0}, AST_TYPE_INT,
                     "LABEL $ifj$readInt\n"
                     "PUSHFRAME\n"
                     "DEFVAR LF@v\n"
                     "DEFVAR LF@t\n"
                     "READ LF@v int\n"
                     "TYPE LF@t LF@v\n"
                     "JUMPIFNEQ $ifj$readInt$e LF@t string@nil\n"
                     "MOVE LF@v int@0\n"
                     "LABEL $ifj$readInt$e\n"
                     "PUSHS LF@v\n"
                     "POPFRAME\n"
                     "RETURN\n"},

    [BI_READ_DOUBLE] = {"readDouble", 0, {0}, AST_TYPE_DOUBLE,
                        "LABEL $ifj$readDouble\n"
                        "PUSHFRAME\n"
                        "DEFVAR LF@v\n"
                        "DEFVAR LF@t\n"
                        "READ LF@v float\n"
                        "TYPE LF@t LF@v\n"
                        "JUMPIFNEQ $ifj$readDouble$e LF@t string@nil\n"
                        "MOVE LF@v float@0x0p+0\n"
                        "LABEL $ifj$readDouble$e\n"
                        "PUSHS LF@v\n"
                        "POPFRAME\n"
                        "RETURN\n"},

    /* BOOLTHEN: Returns false on error or EOF (behavior is unspecified). */
    [BI_READ_BOOLEAN] = {"readBoolean", 0, {0}, AST_TYPE_BOOLEAN,
                         "LABEL $ifj$readBoolean\n"
                         "PUSHFRAME\n"
                         "DEFVAR LF@v\n"
                         "DEFVAR LF@t\n"
                         "READ LF@v bool\n"
                         "TYPE LF@t LF@v\n"
                         "JUMPIFNEQ $ifj$readBoolean$e LF@t string@nil\n"
                         "MOVE LF@v bool@false\n"
                         "LABEL $ifj$readBoolean$e\n"
                         "PUSHS LF@v\n"
                         "POPFRAME\n"
                         "RETURN\n"},

    /* TYPE Determines the value at runtime: Doubles are formatted using %.2f (FLOAT2STR)*/
    [BI_TO_STR] = {"toStr", 1, {PK_ANY}, AST_TYPE_STRING,
                   "LABEL $ifj$toStr\n"
                   "PUSHFRAME\n"
                   "DEFVAR LF@v\n"
                   "DEFVAR LF@t\n"
                   "DEFVAR LF@r\n"
                   "POPS LF@v\n"
                   "TYPE LF@t LF@v\n"
                   "JUMPIFEQ $ifj$toStr$i LF@t string@int\n"
                   "JUMPIFEQ $ifj$toStr$f LF@t string@float\n"
                   "JUMPIFEQ $ifj$toStr$b LF@t string@bool\n"
                   "JUMPIFEQ $ifj$toStr$n LF@t string@nil\n"
                   "MOVE LF@r LF@v\n"
                   "JUMP $ifj$toStr$e\n"
                   "LABEL $ifj$toStr$i\n"
                   "INT2STR LF@r LF@v\n"
                   "JUMP $ifj$toStr$e\n"
                   "LABEL $ifj$toStr$f\n"
                   "FLOAT2STR LF@r LF@v\n"
                   "JUMP $ifj$toStr$e\n"
                   "LABEL $ifj$toStr$b\n"
                   "MOVE LF@r string@false\n"
                   "JUMPIFNEQ $ifj$toStr$e LF@v bool@true\n"
                   "MOVE LF@r string@true\n"
                   "JUMP $ifj$toStr$e\n"
                   "LABEL $ifj$toStr$n\n"
                   "MOVE LF@r string@()\n"
                   "LABEL $ifj$toStr$e\n"
                   "PUSHS LF@r\n"
                   "POPFRAME\n"
                   "RETURN\n"},

    [BI_TO_INT] = {"toInt", 1, {PK_DOUBLE}, AST_TYPE_INT,
                   "LABEL $ifj$toInt\n"
                   "PUSHFRAME\n"
                   "DEFVAR LF@v\n"
                   "DEFVAR LF@r\n"
                   "POPS LF@v\n"
                   "FLOAT2INT LF@r LF@v\n"
                   "PUSHS LF@r\n"
                   "POPFRAME\n"
                   "RETURN\n"},

    [BI_STR_LENGTH] = {"strLength", 1, {PK_STRING}, AST_TYPE_INT,
                       "LABEL $ifj$strLength\n"
                       "PUSHFRAME\n"
                       "DEFVAR LF@v\n"
                       "DEFVAR LF@r\n"
                       "POPS LF@v\n"
                       "STRLEN LF@r LF@v\n"
                       "PUSHS LF@r\n"
                       "POPFRAME\n"
                       "RETURN\n"},

    /* runtime error 40: start<0, end<0, start>end, start>=len, end>len */
    [BI_SUB_STR] = {"subStr", 3, {PK_STRING, PK_INT, PK_INT}, AST_TYPE_STRING,
                    "LABEL $ifj$subStr\n"
                    "PUSHFRAME\n"
                    "DEFVAR LF@s\n"
                    "DEFVAR LF@start\n"
                    "DEFVAR LF@end\n"
                    "DEFVAR LF@len\n"
                    "DEFVAR LF@c\n"
                    "DEFVAR LF@r\n"
                    "DEFVAR LF@ch\n"
                    "DEFVAR LF@i\n"
                    "POPS LF@end\n"
                    "POPS LF@start\n"
                    "POPS LF@s\n"
                    "STRLEN LF@len LF@s\n"
                    "LT LF@c LF@start int@0\n"
                    "JUMPIFEQ $ifj$subStr$err LF@c bool@true\n"
                    "LT LF@c LF@end int@0\n"
                    "JUMPIFEQ $ifj$subStr$err LF@c bool@true\n"
                    "GT LF@c LF@start LF@end\n"
                    "JUMPIFEQ $ifj$subStr$err LF@c bool@true\n"
                    "LT LF@c LF@start LF@len\n"
                    "JUMPIFEQ $ifj$subStr$err LF@c bool@false\n"
                    "GT LF@c LF@end LF@len\n"
                    "JUMPIFEQ $ifj$subStr$err LF@c bool@true\n"
                    "MOVE LF@r string@\n"
                    "MOVE LF@i LF@start\n"
                    "LABEL $ifj$subStr$loop\n"
                    "JUMPIFEQ $ifj$subStr$done LF@i LF@end\n"
                    "GETCHAR LF@ch LF@s LF@i\n"
                    "CONCAT LF@r LF@r LF@ch\n"
                    "ADD LF@i LF@i int@1\n"
                    "JUMP $ifj$subStr$loop\n"
                    "LABEL $ifj$subStr$done\n"
                    "PUSHS LF@r\n"
                    "POPFRAME\n"
                    "RETURN\n"
                    "LABEL $ifj$subStr$err\n"
                    "EXIT int@40\n"},

    /* -1 for empty string or index out of bounds */
    [BI_ORD] = {"ord", 2, {PK_STRING, PK_INT}, AST_TYPE_INT,
                "LABEL $ifj$ord\n"
                "PUSHFRAME\n"
                "DEFVAR LF@s\n"
                "DEFVAR LF@i\n"
                "DEFVAR LF@len\n"
                "DEFVAR LF@c\n"
                "DEFVAR LF@r\n"
                "POPS LF@i\n"
                "POPS LF@s\n"
                "STRLEN LF@len LF@s\n"
                "LT LF@c LF@i int@0\n"
                "JUMPIFEQ $ifj$ord$bad LF@c bool@true\n"
                "LT LF@c LF@i LF@len\n"
                "JUMPIFEQ $ifj$ord$bad LF@c bool@false\n"
                "STRI2INT LF@r LF@s LF@i\n"
                "JUMP $ifj$ord$e\n"
                "LABEL $ifj$ord$bad\n"
                "MOVE LF@r int@-1\n"
                "LABEL $ifj$ord$e\n"
                "PUSHS LF@r\n"
                "POPFRAME\n"
                "RETURN\n"},

    /* A value outside 0..127 fails with error 58 directly inside INT2CHAR */
    [BI_CHR] = {"chr", 1, {PK_INT}, AST_TYPE_STRING,
                "LABEL $ifj$chr\n"
                "PUSHFRAME\n"
                "DEFVAR LF@v\n"
                "DEFVAR LF@r\n"
                "POPS LF@v\n"
                "INT2CHAR LF@r LF@v\n"
                "PUSHS LF@r\n"
                "POPFRAME\n"
                "RETURN\n"},

    /* floor via FLOAT2INT (which truncates toward zero): for a negative non-integer x, subtracts 1 */
    [BI_FLOOR] = {"floor", 1, {PK_DOUBLE}, AST_TYPE_DOUBLE,
                  "LABEL $ifj$floor\n"
                  "PUSHFRAME\n"
                  "DEFVAR LF@x\n"
                  "DEFVAR LF@i\n"
                  "DEFVAR LF@f\n"
                  "DEFVAR LF@c\n"
                  "POPS LF@x\n"
                  "FLOAT2INT LF@i LF@x\n"
                  "INT2FLOAT LF@f LF@i\n"
                  "GT LF@c LF@f LF@x\n"
                  "JUMPIFEQ $ifj$floor$e LF@c bool@false\n"
                  "SUB LF@f LF@f float@0x1p+0\n"
                  "LABEL $ifj$floor$e\n"
                  "PUSHS LF@f\n"
                  "POPFRAME\n"
                  "RETURN\n"},
};

/**
 * Find a built-in function by name (floor is only a member).
 * @param name  function name
 * @return      index into BUILTINS, or -1
 */
static int builtin_find(const char *name) {
    for (int i = 0; i < BI_COUNT; i++) {
        if (i != BI_FLOOR && strcmp(BUILTINS[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------ generator state */

typedef struct {
    const char *name;    /* name in the source (belongs to AST) */
    char *mangled;       /* name in IFJcode26 (belongs to Ctx.locals) */
    const AstType *type; /* type of the variable */
} Var;

typedef struct {
    Var *v;
    size_t n;
    size_t cap;
} Scope;

/* Context of the currently generated function (lambda has its own). */
typedef struct {
    Buf body;
    char **locals; /* mangled names for DEFVAR */
    size_t nlocals, cap_locals;
    Scope *scopes;
    size_t nscopes, cap_scopes;
    const char *end_label; /* sem skáče return */
    int pending;           /* values of partially evaluated expressions on the stack */
    int ret_kind;          /* declared return type or K_NONE */
} Ctx;

typedef struct {
    const AstFunction *fn;
    char *label;
} FuncInfo;

typedef struct {
    const AstExtension *ext;
    const AstFunction *fn;
    char *label;
} ExtInfo;

typedef struct {
    Buf funcs; /* complete functions */
    const AstProgram *prog;
    FuncInfo *fi;
    size_t nfi;
    ExtInfo *xi;
    size_t nxi;
    bool builtin_used[BI_COUNT];
    unsigned labels, uid, lambdas;
    Ctx *ctx;
    int error;
} Gen;

/**
 * Record the first internal error: print it with the position to stderr, set g->error to 99.
 * @param g    generator
 * @param pos  source position
 * @param fmt  printf format of the message
 */
static void fail_at(Gen *g, position pos, const char *fmt, ...) {
    if (g->error == 0) {
        va_list ap;
        fprintf(stderr, "codegen: %zu:%zu: ", pos.line, pos.column);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
        g->error = ERR_INTERNAL;
    }
}

/**
 * Append one instruction line to the curent function body.
 * @param g    generator
 * @param fmt  printf format of the line
 */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
static void emit(Gen *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    buf_vprintf(&g->ctx->body, fmt, ap);
    va_end(ap);
    buf_puts(&g->ctx->body, "\n");
}

/**
 * Emit a "# text name" comment line (only if CODEGEN_COMMENTS is on).
 * @param g     generator
 * @param text  comment text
 * @param name  name added to the comment
 */
static void note(Gen *g, const char *text, const char *name) {
#if CODEGEN_COMMENTS
    emit(g, "# %s %s", text, name);
#else
    (void)g;
    (void)text;
    (void)name;
#endif
}

/**
 * Get a fresh label number (labels are $L<number>).
 * @param g  generator
 * @return   unique number
 */
static unsigned new_label(Gen *g) {
    return g->labels++;
}

/* ---- variable scopes ---- */

/**
 * Open a new innermost variable scope.
 * @param g  generator
 */
static void scope_push(Gen *g) {
    Ctx *c = g->ctx;
    if (c->nscopes == c->cap_scopes) {
        c->cap_scopes = c->cap_scopes ? c->cap_scopes * 2 : 4;
        c->scopes = xrealloc(c->scopes, c->cap_scopes * sizeof *c->scopes);
    }
    c->scopes[c->nscopes++] = (Scope){0};
}

/**
 * Close the innermost variable scope.
 * @param g  generator
 */
static void scope_pop(Gen *g) {
    Ctx *c = g->ctx;
    free(c->scopes[--c->nscopes].v);
}

/**
 * Register a local name so its DEFVAR is emitted at the function start.
 * @param g        generator
 * @param mangled  heap name, ownership passes to the context
 * @return         the same pointer
 */
static char *ctx_add_local(Gen *g, char *mangled) {
    Ctx *c = g->ctx;
    if (c->nlocals == c->cap_locals) {
        c->cap_locals = c->cap_locals ? c->cap_locals * 2 : 16;
        c->locals = xrealloc(c->locals, c->cap_locals * sizeof *c->locals);
    }
    c->locals[c->nlocals++] = mangled;
    return mangled;
}

/**
 * Create a helper local variable named $<prefix><uid>.
 * @param g       generator
 * @param prefix  name prefix
 * @return        variable name (owned by the context)
 */
static char *ctx_temp(Gen *g, const char *prefix) {
    return ctx_add_local(g, xfmt("$%s%u", prefix, ++g->uid));
}

/**
 * Declare a variable in the innermost scope under a unique name <name>$<uid>.
 * @param g     generator
 * @param name  source name
 * @param type  variable type
 * @return      name used in the generated code
 */
static const char *scope_declare(Gen *g, const char *name, const AstType *type) {
    Ctx *c = g->ctx;
    Scope *s = &c->scopes[c->nscopes - 1];
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 4;
        s->v = xrealloc(s->v, s->cap * sizeof *s->v);
    }
    char *m = ctx_add_local(g, xfmt("%s$%u", name, ++g->uid));
    s->v[s->n++] = (Var){name, m, type};
    return m;
}

/**
 * Find a variable, innermost scope first.
 * @param g     generator
 * @param name  source name
 * @return      the variable, or NULL
 */
static const Var *scope_lookup(Gen *g, const char *name) {
    Ctx *c = g->ctx;
    for (size_t i = c->nscopes; i > 0; i--) {
        const Scope *s = &c->scopes[i - 1];
        for (size_t j = s->n; j > 0; j--) {
            if (strcmp(s->v[j - 1].name, name) == 0) {
                return &s->v[j - 1];
            }
        }
    }
    return NULL;
}

/* ---- function tables ---- */

/**
 * Find a top-level function by name.
 * @param g     generator
 * @param name  function name
 * @return      table entry, or NULL
 */
static const FuncInfo *find_func(Gen *g, const char *name) {
    for (size_t i = 0; i < g->nfi; i++) {
        if (strcmp(g->fi[i].fn->name, name) == 0) {
            return &g->fi[i];
        }
    }
    return NULL;
}

/**
 * Find an extension method by receiver type and name.
 * @param g     generator
 * @param recv  receiver type
 * @param name  method name
 * @return      table entry, or NULL
 */
static const ExtInfo *find_ext(Gen *g, const AstType *recv, const char *name) {
    for (size_t i = 0; i < g->nxi; i++) {
        const ExtInfo *x = &g->xi[i];
        if (strcmp(x->fn->name, name) == 0 && type_equal(x->ext->receiver.type, recv)) {
            return x;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------ expression types */

/**
 * Type of an expression: evaluated_type, else inferred for simple cases.
 * @param g  generator
 * @param e  expression
 * @return   type, or NULL if unknown
 */
static const AstType *expr_type(Gen *g, const AstExpression *e) {
    if (e->evaluated_type) {
        return e->evaluated_type;
    }
    switch (e->kind) {
    case AST_EXPR_INT:
        return &TY_INT;
    case AST_EXPR_DOUBLE:
        return &TY_DOUBLE;
    case AST_EXPR_STRING:
    case AST_EXPR_INTERPOLATED_STRING:
        return &TY_STRING;
    case AST_EXPR_BOOLEAN:
        return &TY_BOOL;
    case AST_EXPR_UNIT:
        return &TY_UNIT;
    case AST_EXPR_NAME: {
        const Var *v = scope_lookup(g, e->data.name);
        if (v) {
            return v->type;
        }
        const FuncInfo *f = find_func(g, e->data.name);
        return (f && f->fn->parameters.count == 0) ? f->fn->return_type : NULL;
    }
    case AST_EXPR_UNARY:
        return e->data.unary.op == AST_UNARY_NOT ? &TY_BOOL : expr_type(g, e->data.unary.operand);
    case AST_EXPR_BINARY: {
        AstBinaryOp op = e->data.binary.op;
        if (op >= AST_EQUAL) {
            return &TY_BOOL;
        }
        const AstType *l = expr_type(g, e->data.binary.left);
        const AstType *r = expr_type(g, e->data.binary.right);
        if (!l || !r) {
            return NULL;
        }
        if (l->kind == AST_TYPE_STRING) {
            return l;
        }
        return (l->kind == AST_TYPE_DOUBLE || r->kind == AST_TYPE_DOUBLE) ? &TY_DOUBLE : l;
    }
    case AST_EXPR_CALL: {
        const AstExpression *c = e->data.call.callee;
        if (c->kind != AST_EXPR_NAME || scope_lookup(g, c->data.name)) {
            return NULL;
        }
        const FuncInfo *f = find_func(g, c->data.name);
        if (f) {
            return e->data.call.arguments.count == f->fn->parameters.count ? f->fn->return_type : NULL;
        }
        int b = builtin_find(c->data.name);
        return b >= 0 ? prim_type(BUILTINS[b].ret) : NULL;
    }
    default:
        return NULL;
    }
}