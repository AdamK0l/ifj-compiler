/**
 * Project: IFJ26 / IAL compiler
 * File: expr_parser.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 *   Adam Kolacek (xkolaca00)
 */

#include "parser_internal.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>

/* Precedence levels (higher number = higher priority) */
typedef enum {
    PREC_NONE = 0,
    PREC_LOGICAL_OR = 1,     // ||
    PREC_LOGICAL_AND = 2,    // &&
    PREC_EQUALITY = 3,       // ==, !=
    PREC_RELATIONAL = 4,     // <, <=, >, >=
    PREC_ADDITIVE = 5,       // +, -
    PREC_MULTIPLICATIVE = 6, // *, /
    PREC_UNARY = 7           // -, !, +
} Precedence;

static int get_binary_precedence(OperatorType op) {
    switch (op) {
    case OP_OR:
        return PREC_LOGICAL_OR;
    case OP_AND:
        return PREC_LOGICAL_AND;
    case OP_EQUAL:
    case OP_NOT_EQUAL:
        return PREC_EQUALITY;
    case OP_LESS:
    case OP_LESS_EQUAL:
    case OP_GREATER:
    case OP_GREATER_EQUAL:
        return PREC_RELATIONAL;
    case OP_PLUS:
    case OP_MINUS:
        return PREC_ADDITIVE;
    case OP_MUL:
    case OP_DIV:
        return PREC_MULTIPLICATIVE;
    default:
        return PREC_NONE;
    }
}

static AstBinaryOp get_binary_ast_op(OperatorType op) {
    switch (op) {
    case OP_OR:
        return AST_OR;
    case OP_AND:
        return AST_AND;
    case OP_EQUAL:
        return AST_EQUAL;
    case OP_NOT_EQUAL:
        return AST_NOT_EQUAL;
    case OP_LESS:
        return AST_LESS;
    case OP_LESS_EQUAL:
        return AST_LESS_EQUAL;
    case OP_GREATER:
        return AST_GREATER;
    case OP_GREATER_EQUAL:
        return AST_GREATER_EQUAL;
    case OP_PLUS:
        return AST_ADD;
    case OP_MINUS:
        return AST_SUBTRACT;
    case OP_MUL:
        return AST_MULTIPLY;
    case OP_DIV:
        return AST_DIVIDE;
    default:
        return AST_ADD;
    }
}

static AstExpression *parse_expr_precedence(Parser *p, int min_prec);

static AstExpression *parse_primary(Parser *p) {
    position pos = { (size_t)p->current.line, (size_t)p->current.column };

    /* 1. Literals */
    if (check(p, TOKEN_INT_LITERAL)) {
        int32_t val = (int32_t)p->current.int_value;
        advance(p);
        return ast_expr_int(val, pos);
    }

    if (check(p, TOKEN_DOUBLE_LITERAL)) {
        double val = p->current.double_value;
        advance(p);
        return ast_expr_double(val, pos);
    }

    if (check(p, TOKEN_STRING_LITERAL)) {
        char *str = take_string(p);
        size_t len = str ? strlen(str) : 0;
        advance(p);
        return ast_expr_string(str, len, pos);
    }

    if (check_keyword(p, KW_TRUE)) {
        advance(p);
        return ast_expr_bool(true, pos);
    }

    if (check_keyword(p, KW_FALSE)) {
        advance(p);
        return ast_expr_bool(false, pos);
    }

    /* 2. Unary operators (+, -, !) */
    if (check_operator(p, OP_MINUS)) {
        advance(p);
        AstExpression *operand = parse_expr_precedence(p, PREC_UNARY);
        if (!operand) return NULL;
        return ast_expr_unary(AST_UNARY_MINUS, operand, pos);
    }

    if (check_operator(p, OP_PLUS)) {
        advance(p);
        AstExpression *operand = parse_expr_precedence(p, PREC_UNARY);
        if (!operand) return NULL;
        return ast_expr_unary(AST_UNARY_PLUS, operand, pos);
    }

    if (check_operator(p, OP_NOT)) {
        advance(p);
        AstExpression *operand = parse_expr_precedence(p, PREC_UNARY);
        if (!operand) return NULL;
        return ast_expr_unary(AST_UNARY_NOT, operand, pos);
    }

    /* 3. Parentheses or Unit literal () */
    if (check(p, TOKEN_LEFT_PAREN)) {
        advance(p);
        if (check(p, TOKEN_RIGHT_PAREN)) {
            advance(p);
            return ast_expr_unit(pos);
        }
        AstExpression *inner = NULL;
        if (!parse_expression(p, &inner)) return NULL;
        if (!parser_expect(p, TOKEN_RIGHT_PAREN, "')'")) {
            ast_free_expression(inner);
            return NULL;
        }
        return inner;
    }

    /* 4. Identifier or Function call */
    if (check(p, TOKEN_IDENTIFIER)) {
        char *name = take_string(p);
        advance(p);

        if (check(p, TOKEN_LEFT_PAREN)) {
            /* Function call */
            advance(p);
            AstExpression *callee = ast_expr_name(name, pos);
            AstExpression *call = ast_expr_call(callee, pos);
            AstExpressionList *args = ast_argument_lists_push(&call->data.call.arguments);

            if (!check(p, TOKEN_RIGHT_PAREN)) {
                while (true) {
                    AstExpression *arg = NULL;
                    if (!parse_expression(p, &arg)) {
                        ast_free_expression(call);
                        return NULL;
                    }
                    ast_expr_list_push(args, arg);

                    if (check(p, TOKEN_COMMA)) {
                        advance(p);
                    } else {
                        break;
                    }
                }
            }

            if (!parser_expect(p, TOKEN_RIGHT_PAREN, "')'")) {
                ast_free_expression(call);
                return NULL;
            }
            return call;
        }

        return ast_expr_name(name, pos);
    }

    /* 5. Conditional (if-then-else expression) */
    if (check_keyword(p, KW_IF)) {
        advance(p);
        AstExpression *cond = NULL;
        if (!parse_expression(p, &cond)) return NULL;

        if (!parser_expect_keyword(p, KW_THEN, "'then'")) {
            ast_free_expression(cond);
            return NULL;
        }

        AstBLOCK *then_block = NULL;
        if (check(p, TOKEN_EOL)) {
            advance(p);
            if (!parse_block(p, &then_block)) {
                ast_free_expression(cond);
                return NULL;
            }
        } else {
            AstExpression *then_expr = NULL;
            if (!parse_expression(p, &then_expr)) {
                ast_free_expression(cond);
                return NULL;
            }
            then_block = ast_block_create(pos);
            ast_stmt_list_push(&then_block->statements, ast_stmt_expression(then_expr, pos));
        }

        while (check(p, TOKEN_EOL)) {
            advance(p);
        }

        AstBLOCK *else_block = NULL;
        if (check_keyword(p, KW_ELSE)) {
            advance(p);
            if (check(p, TOKEN_EOL)) {
                advance(p);
                if (!parse_block(p, &else_block)) {
                    ast_free_expression(cond);
                    ast_free_block(then_block);
                    return NULL;
                }
            } else {
                AstExpression *else_expr = NULL;
                if (!parse_expression(p, &else_expr)) {
                    ast_free_expression(cond);
                    ast_free_block(then_block);
                    return NULL;
                }
                else_block = ast_block_create(pos);
                ast_stmt_list_push(&else_block->statements, ast_stmt_expression(else_expr, pos));
            }
        }

        return ast_expr_conditional(cond, then_block, else_block, pos);
    }

    syntax_error(p, "expression (literal, identifier, unary operator, or '(')");
    return NULL;
}

static AstExpression *parse_expr_precedence(Parser *p, int min_prec) {
    AstExpression *left = parse_primary(p);
    if (!left) return NULL;

    while (true) {
        if (!check(p, TOKEN_OPERATOR)) {
            break;
        }

        int prec = get_binary_precedence(p->current.operator);
        if (prec < min_prec) {
            break;
        }

        OperatorType token_op = p->current.operator;
        AstBinaryOp ast_op = get_binary_ast_op(token_op);
        position op_pos = { (size_t)p->current.line, (size_t)p->current.column };
        advance(p);

        /* Left-associative: child expression must have strictly higher precedence */
        int next_min_prec = prec + 1;
        AstExpression *right = parse_expr_precedence(p, next_min_prec);
        if (!right) {
            ast_free_expression(left);
            return NULL;
        }

        left = ast_expr_binary(ast_op, left, right, op_pos);
    }

    return left;
}

bool parse_expression(Parser *p, AstExpression **out_expr) {
    AstExpression *expr = parse_expr_precedence(p, PREC_LOGICAL_OR);
    if (!expr) {
        *out_expr = NULL;
        return false;
    }
    *out_expr = expr;
    return true;
}
