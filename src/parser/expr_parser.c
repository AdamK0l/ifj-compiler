/**
 * Project: IFJ26 / IAL compiler
 * File: expr_parser.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 */

#include "parser_internal.h"
#include <stdlib.h>

/* ==================== Precedence table ==================== */

/* Terminals of the precedence table (rows = stack top, columns = input). */
typedef enum {
    PT_U,        /* unary -  ! */
    PT_MULDIV,   /* *  /       */
    PT_ADDSUB,   /* +  -       */
    PT_REL,      /* < <= > >=  */
    PT_EQ,       /* ==  !=     */
    PT_AND,      /* &&         */
    PT_OR,       /* ||         */
    PT_LPAR,     /* (          */
    PT_RPAR,     /* )          */
    PT_I,        /* operand    */
    PT_END,      /* $          */
    PT_COUNT
} PrecTerm;

/* '<' shift, '>' reduce, '=' shift ')', 'E' accept, ' ' syntax error */
static const char prec_table[PT_COUNT][PT_COUNT] = {
/*             u    mul  add  rel  eq   and  or   (    )    i    $   */
/* u    */  { '<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>' },
/* mul  */  { '<', '>', '>', '>', '>', '>', '>', '<', '>', '<', '>' },
/* add  */  { '<', '<', '>', '>', '>', '>', '>', '<', '>', '<', '>' },
/* rel  */  { '<', '<', '<', ' ', '>', '>', '>', '<', '>', '<', '>' },
/* eq   */  { '<', '<', '<', '<', ' ', '>', '>', '<', '>', '<', '>' },
/* and  */  { '<', '<', '<', '<', '<', '>', '>', '<', '>', '<', '>' },
/* or   */  { '<', '<', '<', '<', '<', '<', '>', '<', '>', '<', '>' },
/* (    */  { '<', '<', '<', '<', '<', '<', '<', '<', '=', '<', ' ' },
/* )    */  { ' ', '>', '>', '>', '>', '>', '>', ' ', '>', ' ', '>' },
/* i    */  { ' ', '>', '>', '>', '>', '>', '>', ' ', '>', ' ', '>' },
/* $    */  { '<', '<', '<', '<', '<', '<', '<', '<', ' ', '<', 'E' },
};

/* ==================== Stack ==================== */

typedef enum { SI_TERM, SI_MARK, SI_EXPR } StackKind;

typedef struct {
    StackKind kind;
    PrecTerm term;     /* valid when kind == SI_TERM */
    /* TODO: AstExpression *expr for SI_TERM (operand) and SI_EXPR */
} StackItem;

typedef struct {
    StackItem *items;
    size_t count;
    size_t capacity;
} ExprStack;

static void stack_push(ExprStack *s, StackItem item){
    if(s->count == s->capacity) {
        size_t new_cap = (s->capacity == 0) ? 16 : s->capacity * 2;
        StackItem *n = realloc(s->items, new_cap * sizeof(StackItem));
        if(n == NULL) exit(99);
        s->items = n;
        s->capacity = new_cap;
    }
    s->items[s->count++] = item;
}

static void stack_push_term(ExprStack *s, PrecTerm t){
    StackItem item = {.kind = SI_TERM, .term = t};
    stack_push(s, item);
}

static void stack_free(ExprStack *s){
    free(s->items);
    s->items = NULL;
    s->count = s->capacity = 0;
}

/* Index of the topmost terminal (skips E and marks). Bottom $ always exists. */
static size_t stack_top_term_index(ExprStack *s){
    size_t i = s->count;
    while(i > 0) {
        i--;
        if(s->items[i].kind == SI_TERM) return i;
    }
    return 0;
}

/* Inserts the '<' mark right after the topmost terminal. */
static void stack_insert_mark(ExprStack *s){
    size_t t = stack_top_term_index(s);
    StackItem mark = {.kind = SI_MARK, .term = PT_END};
    stack_push(s, mark);                       /* make room */
    for(size_t i = s->count - 1; i > t + 1; i--) {
        s->items[i] = s->items[i - 1];
    }
    s->items[t + 1] = mark;
}

/* ==================== Token classification ==================== */

/* Maps the current token to a table column.
   '-' is unary at the start, after '(' or after an operator (prev_operand == false).
   ')' without an open '(' in this expression and any other token end the expression. */
static PrecTerm classify(Parser *p, bool prev_operand, int paren_depth){
    if(check_operator(p, OP_MINUS)) return prev_operand ? PT_ADDSUB : PT_U;
    if(check_operator(p, OP_NOT)) return PT_U;
    if(check_operator(p, OP_PLUS)) return PT_ADDSUB;
    if(check_operator(p, OP_MUL) || check_operator(p, OP_DIV)) return PT_MULDIV;
    if(check_operator(p, OP_LESS) || check_operator(p, OP_LESS_EQUAL) ||
       check_operator(p, OP_GREATER) || check_operator(p, OP_GREATER_EQUAL)) return PT_REL;
    if(check_operator(p, OP_EQUAL) || check_operator(p, OP_NOT_EQUAL)) return PT_EQ;
    if(check_operator(p, OP_AND)) return PT_AND;
    if(check_operator(p, OP_OR)) return PT_OR;
    if(check(p, TOKEN_LEFT_PAREN)) return PT_LPAR;
    if(check(p, TOKEN_RIGHT_PAREN)) return (paren_depth > 0) ? PT_RPAR : PT_END;
    if(check(p, TOKEN_IDENTIFIER) || check(p, TOKEN_INT_LITERAL) ||
       check(p, TOKEN_DOUBLE_LITERAL) || check(p, TOKEN_STRING_LITERAL) ||
       check_keyword(p, KW_TRUE) || check_keyword(p, KW_FALSE)) return PT_I;
    return PT_END;
}

/* ==================== Operand (calls, members) ==================== */

/* ( ) or ( expr , expr , ... [,] ) — current token is '(' */
static bool parse_call_args(Parser *p){
    if(!parser_expect(p, TOKEN_LEFT_PAREN, "'('")) return false;
    if(check(p, TOKEN_RIGHT_PAREN)) {
        advance(p);
        return true;
    }
    while(true) {
        if(!parse_expression(p)) return false;
        if(!check(p, TOKEN_COMMA)) break;
        advance(p);
        if(check(p, TOKEN_RIGHT_PAREN)) break;   /* trailing comma */
    }
    return parser_expect(p, TOKEN_RIGHT_PAREN, "')'");
}

/* After an operand: calls f(a)(b) (FUNEXP, PARAMLISTS) and members x.length (VALMEM, EXTKW) */
static bool parse_postfix(Parser *p){
    while(true) {
        if(check(p, TOKEN_LEFT_PAREN)) {
            if(!parse_call_args(p)) return false;
        } else if(check(p, TOKEN_DOT)) {
            advance(p);
            if(!check(p, TOKEN_IDENTIFIER)) return syntax_error(p, "member name");
            advance(p);   // TODO: member AST node
        } else {
            return true;
        }
    }
}

/* Reads one operand (literal, true/false, identifier) with its postfix part. */
static bool read_operand(Parser *p){
    advance(p);   // TODO: literal / name AST node
    return parse_postfix(p);
}

/* ==================== Reduction ==================== */

/* Rules:  E -> i  |  u E  |  E op E  |  ( E )  */
static bool reduce(Parser *p, ExprStack *s){
    size_t m = s->count;
    while(m > 0 && s->items[m - 1].kind != SI_MARK) m--;
    if(m == 0) return syntax_error(p, "valid expression");
    size_t mark = m - 1;
    size_t n = s->count - m;
    StackItem *h = &s->items[m];
    bool ok = false;

    if(n == 1 && h[0].kind == SI_TERM && h[0].term == PT_I) {
        ok = true;                                                  /* E -> i      */
    } else if(n == 2 && h[0].kind == SI_TERM && h[0].term == PT_U &&
              h[1].kind == SI_EXPR) {
        ok = true;                                                  /* E -> u E    */
    } else if(n == 3 && h[0].kind == SI_EXPR && h[1].kind == SI_TERM &&
              h[1].term >= PT_MULDIV && h[1].term <= PT_OR && h[2].kind == SI_EXPR) {
        ok = true;                                                  /* E -> E op E */
    } else if(n == 3 && h[0].kind == SI_TERM && h[0].term == PT_LPAR &&
              h[1].kind == SI_EXPR && h[2].kind == SI_TERM && h[2].term == PT_RPAR) {
        ok = true;                                                  /* E -> ( E )  */
    }
    if(!ok) return syntax_error(p, "valid expression");

    s->count = mark;   // TODO: build AST node from the handle
    StackItem e = {.kind = SI_EXPR, .term = PT_END};
    stack_push(s, e);
    return true;
}

/* ==================== Main algorithm ==================== */

/* Precedence parsing. If first_operand_read, an ID was already consumed by the caller. */
static bool prec_parse(Parser *p, bool first_operand_read){
    ExprStack s = {NULL, 0, 0};
    bool ok = false;
    bool prev_operand = false;
    int paren_depth = 0;

    stack_push_term(&s, PT_END);
    if(first_operand_read) {
        stack_insert_mark(&s);
        stack_push_term(&s, PT_I);
        if(!parse_postfix(p)) goto end;
        prev_operand = true;
    }

    while(true) {
        PrecTerm a = s.items[stack_top_term_index(&s)].term;
        PrecTerm b = classify(p, prev_operand, paren_depth);
        char rel = prec_table[a][b];

        if(rel == 'E') {
            /* accept: stack must be exactly $ E */
            if(s.count == 2 && s.items[1].kind == SI_EXPR) ok = true;
            else syntax_error(p, "expression");
            goto end;
        }
        if(rel == '<') {
            stack_insert_mark(&s);
            if(b == PT_I) {
                stack_push_term(&s, PT_I);
                if(!read_operand(p)) goto end;
                prev_operand = true;
            } else if(b == PT_LPAR) {
                advance(p);
                if(check(p, TOKEN_RIGHT_PAREN)) {      /* () unit literal = operand */
                    advance(p);
                    stack_push_term(&s, PT_I);
                    prev_operand = true;
                } else {
                    stack_push_term(&s, PT_LPAR);
                    paren_depth++;
                    prev_operand = false;
                }
            } else {
                stack_push_term(&s, b);               /* operator */
                advance(p);
                prev_operand = false;
            }
        } else if(rel == '=') {
            stack_push_term(&s, PT_RPAR);             /* ')' */
            advance(p);
            paren_depth--;
            prev_operand = true;
        } else if(rel == '>') {
            if(!reduce(p, &s)) goto end;
        } else {
            if(a == PT_LPAR && b == PT_END) syntax_error(p, "')'");
            else if(b == PT_END) syntax_error(p, "operand");
            else syntax_error(p, "operator or end of expression");
            goto end;
        }
    }

end:
    stack_free(&s);
    return ok;
}

bool parse_expression(Parser *p){
    return prec_parse(p, false);
}

/* Continues an expression whose first ID was already read by the statement parser.
   Takes ownership of first_id. */
bool parse_expression_rest(Parser *p, char *first_id, position pos){
    free(first_id);   // TODO: becomes the first operand (name AST node)
    (void)pos;
    return prec_parse(p, true);
}
