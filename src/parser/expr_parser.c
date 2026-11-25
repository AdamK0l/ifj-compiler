/**
 * Project: IFJ26 / IAL compiler
 * File: expr_parser.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 */

#include "parser_internal.h"
#include <stdlib.h>
#include <string.h>

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
    PrecTerm term;          /* valid when kind == SI_TERM                        */
    int op;                 /* AstBinaryOp / AstUnaryOp for operator terminals   */
    position pos;           /* position of the token                            */
    AstExpression *expr;    /* operand (SI_TERM PT_I) or reduced E (SI_EXPR)     */
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

static void stack_push_term(ExprStack *s, PrecTerm t, int op, position pos, AstExpression *expr){
    StackItem item = {.kind = SI_TERM, .term = t, .op = op, .pos = pos, .expr = expr};
    stack_push(s, item);
}

static void stack_push_expr(ExprStack *s, AstExpression *expr){
    StackItem item = {.kind = SI_EXPR, .term = PT_END, .op = 0, .pos = expr->pos, .expr = expr};
    stack_push(s, item);
}

/* Frees the stack and every expression still on it. */
static void stack_free(ExprStack *s){
    for(size_t i = 0; i < s->count; i++) {
        ast_free_expression(s->items[i].expr);
    }
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
    StackItem mark = {.kind = SI_MARK, .term = PT_END, .op = 0, .pos = {0, 0}, .expr = NULL};
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

/* AST operator for the current operator token. */
static int token_to_op(Parser *p, PrecTerm t){
    if(t == PT_U) return check_operator(p, OP_NOT) ? AST_UNARY_NOT : AST_UNARY_MINUS;
    switch(p->current.operator) {
        case OP_PLUS:          return AST_ADD;
        case OP_MINUS:         return AST_SUBTRACT;
        case OP_MUL:           return AST_MULTIPLY;
        case OP_DIV:           return AST_DIVIDE;
        case OP_LESS:          return AST_LESS;
        case OP_LESS_EQUAL:    return AST_LESS_EQUAL;
        case OP_GREATER:       return AST_GREATER;
        case OP_GREATER_EQUAL: return AST_GREATER_EQUAL;
        case OP_EQUAL:         return AST_EQUAL;
        case OP_NOT_EQUAL:     return AST_NOT_EQUAL;
        case OP_AND:           return AST_AND;
        case OP_OR:            return AST_OR;
        default:               return AST_ADD;
    }
}

/* ==================== Operand (literals, calls, members) ==================== */

/* ( ) or ( expr , expr , ... [,] ) — current token is '('. Arguments go into list. */
static bool parse_call_args(Parser *p, AstExpressionList *list){
    if(!parser_expect(p, TOKEN_LEFT_PAREN, "'('")) return false;
    if(check(p, TOKEN_RIGHT_PAREN)) {
        advance(p);
        return true;
    }
    while(true) {
        AstExpression *arg = parse_expression(p);
        if(arg == NULL) return false;
        ast_expr_list_push(list, arg);

        if(!check(p, TOKEN_COMMA)) break;
        advance(p);
        if(check(p, TOKEN_RIGHT_PAREN)) break;   /* trailing comma */
    }
    return parser_expect(p, TOKEN_RIGHT_PAREN, "')'");
}

/* After an operand: calls f(a)(b) (FUNEXP, PARAMLISTS) and members x.length (VALMEM, EXTKW).
   More argument lists right after a call belong to the same call: add(1)(2) = 1 call, 2 lists.
   Takes ownership of expr; returns NULL on error (expr is freed). */
static AstExpression *parse_postfix(Parser *p, AstExpression *expr){
    while(true) {
        if(check(p, TOKEN_LEFT_PAREN)) {
            if(expr->kind != AST_EXPR_CALL) {
                expr = ast_expr_call(expr, expr->pos);
            }
            AstExpressionList *args = ast_argument_lists_push(&expr->data.call.arguments);
            if(!parse_call_args(p, args)) {
                ast_free_expression(expr);
                return NULL;
            }
        } else if(check(p, TOKEN_DOT)) {
            advance(p);
            if(!check(p, TOKEN_IDENTIFIER)) {
                syntax_error(p, "member name");
                ast_free_expression(expr);
                return NULL;
            }
            position pos = current_pos(p);
            char *name = take_string(p);
            advance(p);
            expr = ast_expr_member(expr, name, pos);
        } else {
            return expr;
        }
    }
}

/* Reads one operand (literal, true/false, identifier) with its postfix part. */
static AstExpression *read_operand(Parser *p){
    position pos = current_pos(p);
    AstExpression *expr = NULL;

    if(check(p, TOKEN_INT_LITERAL)) {
        expr = ast_expr_int((int32_t)p->current.int_value, pos);
    } else if(check(p, TOKEN_DOUBLE_LITERAL)) {
        expr = ast_expr_double(p->current.double_value, pos);
    } else if(check(p, TOKEN_STRING_LITERAL)) {
        char *s = take_string(p);
        if(s == NULL) s = ast_alloc(1);            /* empty string "" */
        expr = ast_expr_string(s, strlen(s), pos);
    } else if(check_keyword(p, KW_TRUE)) {
        expr = ast_expr_bool(true, pos);
    } else if(check_keyword(p, KW_FALSE)) {
        expr = ast_expr_bool(false, pos);
    } else {
        expr = ast_expr_name(take_string(p), pos);  /* TOKEN_IDENTIFIER */
    }
    advance(p);
    return parse_postfix(p, expr);
}

/* ==================== Reduction ==================== */

/* Rules:  E -> i  |  u E  |  E op E  |  ( E )  — builds the AST node of the handle. */
static bool reduce(Parser *p, ExprStack *s){
    size_t m = s->count;
    while(m > 0 && s->items[m - 1].kind != SI_MARK) m--;
    if(m == 0) return syntax_error(p, "valid expression");
    size_t mark = m - 1;
    size_t n = s->count - m;
    StackItem *h = &s->items[m];
    AstExpression *result = NULL;

    if(n == 1 && h[0].kind == SI_TERM && h[0].term == PT_I) {
        result = h[0].expr;                                                         /* E -> i      */
    } else if(n == 2 && h[0].kind == SI_TERM && h[0].term == PT_U &&
              h[1].kind == SI_EXPR) {
        result = ast_expr_unary((AstUnaryOp)h[0].op, h[1].expr, h[0].pos);          /* E -> u E    */
    } else if(n == 3 && h[0].kind == SI_EXPR && h[1].kind == SI_TERM &&
              h[1].term >= PT_MULDIV && h[1].term <= PT_OR && h[2].kind == SI_EXPR) {
        result = ast_expr_binary((AstBinaryOp)h[1].op, h[0].expr, h[2].expr, h[1].pos); /* E -> E op E */
    } else if(n == 3 && h[0].kind == SI_TERM && h[0].term == PT_LPAR &&
              h[1].kind == SI_EXPR && h[2].kind == SI_TERM && h[2].term == PT_RPAR) {
        result = h[1].expr;                                                         /* E -> ( E )  */
    }
    if(result == NULL) return syntax_error(p, "valid expression");

    s->count = mark;            /* the handle's expressions now live inside result */
    stack_push_expr(s, result);
    return true;
}

/* ==================== Main algorithm ==================== */

/* Precedence parsing. If first is not NULL, it is the already read first operand. */
static AstExpression *prec_parse(Parser *p, AstExpression *first){
    ExprStack s = {NULL, 0, 0};
    AstExpression *result = NULL;
    bool prev_operand = false;
    int paren_depth = 0;
    position none = {0, 0};

    stack_push_term(&s, PT_END, 0, none, NULL);
    if(first != NULL) {
        stack_insert_mark(&s);
        stack_push_term(&s, PT_I, 0, first->pos, first);
        prev_operand = true;
    }

    while(true) {
        PrecTerm a = s.items[stack_top_term_index(&s)].term;
        PrecTerm b = classify(p, prev_operand, paren_depth);
        char rel = prec_table[a][b];

        if(rel == 'E') {
            /* accept: stack must be exactly $ E */
            if(s.count == 2 && s.items[1].kind == SI_EXPR) {
                result = s.items[1].expr;
                s.items[1].expr = NULL;
            } else {
                syntax_error(p, "expression");
            }
            break;
        }
        if(rel == '<') {
            stack_insert_mark(&s);
            position pos = current_pos(p);
            if(b == PT_I) {
                AstExpression *operand = read_operand(p);
                if(operand == NULL) break;
                stack_push_term(&s, PT_I, 0, pos, operand);
                prev_operand = true;
            } else if(b == PT_LPAR) {
                advance(p);
                if(check(p, TOKEN_RIGHT_PAREN)) {      /* () unit literal = operand */
                    advance(p);
                    stack_push_term(&s, PT_I, 0, pos, ast_expr_unit(pos));
                    prev_operand = true;
                } else {
                    stack_push_term(&s, PT_LPAR, 0, pos, NULL);
                    paren_depth++;
                    prev_operand = false;
                }
            } else {
                stack_push_term(&s, b, token_to_op(p, b), pos, NULL);   /* operator */
                advance(p);
                prev_operand = false;
            }
        } else if(rel == '=') {
            stack_push_term(&s, PT_RPAR, 0, current_pos(p), NULL);      /* ')' */
            advance(p);
            paren_depth--;
            prev_operand = true;
            if(check(p, TOKEN_DOT)) {
                /* (expr).member — reduce ( E ) now and apply the postfix part (VALMEM) */
                if(!reduce(p, &s)) break;
                AstExpression *inner = s.items[s.count - 1].expr;
                s.count--;
                AstExpression *with_postfix = parse_postfix(p, inner);
                if(with_postfix == NULL) break;
                stack_push_expr(&s, with_postfix);
            }
        } else if(rel == '>') {
            if(!reduce(p, &s)) break;
        } else {
            if(a == PT_LPAR && b == PT_END) syntax_error(p, "')'");
            else if(b == PT_END) syntax_error(p, "operand");
            else syntax_error(p, "operator or end of expression");
            break;
        }
    }

    stack_free(&s);
    return result;
}

/* Parses an expression. Returns the AST or NULL on error. */
AstExpression *parse_expression(Parser *p){
    return prec_parse(p, NULL);
}

/* Continues an expression whose first ID was already read by the statement parser.
   Takes ownership of first_id. */
AstExpression *parse_expression_rest(Parser *p, char *first_id, position pos){
    AstExpression *first = parse_postfix(p, ast_expr_name(first_id, pos));
    if(first == NULL) return NULL;
    return prec_parse(p, first);
}
