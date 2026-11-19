/**
 * Project: IFJ26 / IAL compiler
 * File: parser.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 *   Adam Kolacek (xkolaca00)
 */

#include "parser_internal.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>

/* ==================== Token helpers ==================== */

/* Frees the current token and reads the next one from the scanner. */
void advance(Parser *p) {
    token_free(&p->current);
    if (p->has_lookahead) {
        p->current = p->lookahead;
        p->has_lookahead = false;
        memset(&p->lookahead, 0, sizeof(Token));
    } else {
        ErrorCode err = scanner_get_next_token(p->scanner, &p->current);
        if (err != ERR_OK) {
            if (p->error == ERR_OK) {
                p->error = err;
            }
            p->current.type = TOKEN_EOF;
        }
    }
}

/* Peeks one token ahead without consuming it. */
Token *peek_token(Parser *p) {
    if (!p->has_lookahead) {
        ErrorCode err = scanner_get_next_token(p->scanner, &p->lookahead);
        if (err != ERR_OK) {
            if (p->error == ERR_OK) {
                p->error = err;
            }
            p->lookahead.type = TOKEN_EOF;
        }
        p->has_lookahead = true;
    }
    return &p->lookahead;
}

/* Takes the current token's string, so advance will not free it. */
char *take_string(Parser *p) {
    char *s = p->current.string;
    p->current.string = NULL;
    return s;
}

/* Returns true if the current token has the given type. */
bool check(Parser *p, TokenType type) {
    return (p->current.type == type);
}

bool check_keyword(Parser *p, KeywordType kw) {
    return token_is_keyword(&p->current, kw);
}

bool check_operator(Parser *p, OperatorType op) {
    return token_is_operator(&p->current, op);
}

/* ==================== Error handling ==================== */

/* Reports the first syntax error and sets ERR_SYNTAX. */
bool syntax_error(Parser *p, const char *expected) {
    if (p->error == ERR_OK) {
        position pos = { (size_t)p->current.line, (size_t)p->current.column };
        error_print(ERR_SYNTAX, pos, "expected %s", expected);
        p->error = ERR_SYNTAX;
    }
    return false;
}

/* ==================== Expect helpers ==================== */

/* Consumes the current token if it has the given type, otherwise reports a syntax error. */
bool parser_expect(Parser *p, TokenType type, const char *expected) {
    if (check(p, type)) {
        advance(p);
        return true;
    } else {
        return syntax_error(p, expected);
    }
}

bool parser_expect_keyword(Parser *p, KeywordType type, const char *expected) {
    if (check_keyword(p, type)) {
        advance(p);
        return true;
    } else {
        return syntax_error(p, expected);
    }
}

bool parser_expect_operator(Parser *p, OperatorType type, const char *expected) {
    if (check_operator(p, type)) {
        advance(p);
        return true;
    } else {
        return syntax_error(p, expected);
    }
}

/* ==================== Type parsing ==================== */

AstType *parse_type(Parser *p) {
    if (check_keyword(p, KW_INT)) {
        advance(p);
        return ast_type_create(AST_TYPE_INT);
    }
    if (check_keyword(p, KW_DOUBLE)) {
        advance(p);
        return ast_type_create(AST_TYPE_DOUBLE);
    }
    if (check_keyword(p, KW_STRING)) {
        advance(p);
        return ast_type_create(AST_TYPE_STRING);
    }
    if (check_keyword(p, KW_BOOLEAN)) {
        advance(p);
        return ast_type_create(AST_TYPE_BOOLEAN);
    }
    if (check_keyword(p, KW_UNIT)) {
        advance(p);
        return ast_type_create(AST_TYPE_UNIT);
    }
    syntax_error(p, "type (Int, Double, String, Boolean, Unit)");
    return NULL;
}

AstType *parse_opt_type(Parser *p) {
    if (check(p, TOKEN_COLON)) {
        advance(p);
        return parse_type(p);
    }
    return NULL;
}

/* ==================== Parameters ==================== */

bool parse_parameters(Parser *p, AstParameterLists *param_lists) {
    AstParameterList *list = ast_parameter_lists_push(param_lists);

    if (check(p, TOKEN_RIGHT_PAREN)) {
        return true;
    }

    while (true) {
        if (!check(p, TOKEN_IDENTIFIER)) {
            return syntax_error(p, "parameter name");
        }
        position pos = { (size_t)p->current.line, (size_t)p->current.column };
        char *name = take_string(p);
        advance(p);

        if (!parser_expect(p, TOKEN_COLON, "':' after parameter name")) {
            free(name);
            return false;
        }

        AstType *type = parse_type(p);
        if (!type) {
            free(name);
            return false;
        }

        ast_parameter_list_push(list, (AstParameter){ .name = name, .type = type, .pos = pos });

        if (check(p, TOKEN_COMMA)) {
            advance(p);
        } else {
            break;
        }
    }

    return true;
}

/* ==================== Statements ==================== */

bool parse_statement(Parser *p, AstStatement **out_stmt) {
    position pos = { (size_t)p->current.line, (size_t)p->current.column };

    /* 1. Variable definition: val / var */
    if (check_keyword(p, KW_VAL) || check_keyword(p, KW_VAR)) {
        bool is_changeable = check_keyword(p, KW_VAR);
        advance(p);

        if (!check(p, TOKEN_IDENTIFIER)) {
            return syntax_error(p, "variable identifier");
        }
        char *name = take_string(p);
        advance(p);

        AstType *declared_type = parse_opt_type(p);
        if (p->error != ERR_OK) {
            free(name);
            return false;
        }

        if (!parser_expect_operator(p, OP_ASSIGN, "'=' in variable declaration")) {
            free(name);
            if (declared_type) ast_type_free(declared_type);
            return false;
        }

        AstExpression *init = NULL;
        if (!parse_expression(p, &init)) {
            free(name);
            if (declared_type) ast_type_free(declared_type);
            return false;
        }

        *out_stmt = ast_stmt_variable(name, is_changeable, declared_type, init, pos);
        return true;
    }

    /* 2. Return statement */
    if (check_keyword(p, KW_RETURN)) {
        advance(p);
        AstExpression *ret_val = NULL;
        if (!check(p, TOKEN_EOL) && !check(p, TOKEN_SEMICOLON) &&
            !check(p, TOKEN_DEDENT) && !check(p, TOKEN_EOF)) {
            if (!parse_expression(p, &ret_val)) {
                return false;
            }
        }
        *out_stmt = ast_stmt_return(ret_val, pos);
        return true;
    }

    /* 3. While loop */
    if (check_keyword(p, KW_WHILE)) {
        advance(p);
        AstExpression *cond = NULL;
        if (!parse_expression(p, &cond)) {
            return false;
        }

        if (!parser_expect_keyword(p, KW_DO, "'do'")) {
            ast_free_expression(cond);
            return false;
        }

        AstBLOCK *body = NULL;
        if (check(p, TOKEN_EOL)) {
            advance(p);
            if (!parse_block(p, &body)) {
                ast_free_expression(cond);
                return false;
            }
        } else {
            AstExpression *stmt_expr = NULL;
            if (!parse_expression(p, &stmt_expr)) {
                ast_free_expression(cond);
                return false;
            }
            body = ast_block_create(pos);
            ast_stmt_list_push(&body->statements, ast_stmt_expression(stmt_expr, pos));
        }

        *out_stmt = ast_stmt_while(cond, body, pos);
        return true;
    }

    /* 4. Assignment: identifier '=' expr */
    if (check(p, TOKEN_IDENTIFIER)) {
        Token *next = peek_token(p);
        if (next && next->type == TOKEN_OPERATOR && next->operator == OP_ASSIGN) {
            char *name = take_string(p);
            advance(p); /* consume identifier */
            advance(p); /* consume '=' */

            AstExpression *val = NULL;
            if (!parse_expression(p, &val)) {
                free(name);
                return false;
            }

            *out_stmt = ast_stmt_assign(name, val, pos);
            return true;
        }
    }

    /* 5. Expression statement (function calls, standalone expressions, if-expressions) */
    AstExpression *expr = NULL;
    if (!parse_expression(p, &expr)) {
        return false;
    }
    *out_stmt = ast_stmt_expression(expr, pos);
    return true;
}

/* ==================== Blocks ==================== */

bool parse_block(Parser *p, AstBLOCK **out_block) {
    while (check(p, TOKEN_EOL)) {
        advance(p);
    }

    if (!parser_expect(p, TOKEN_INDENT, "indented block")) {
        return false;
    }

    position pos = { (size_t)p->current.line, (size_t)p->current.column };
    AstBLOCK *block = ast_block_create(pos);

    while (true) {
        while (check(p, TOKEN_EOL)) {
            advance(p);
        }

        if (check(p, TOKEN_DEDENT)) {
            advance(p);
            break;
        }

        if (check(p, TOKEN_EOF)) {
            break;
        }

        AstStatement *stmt = NULL;
        if (!parse_statement(p, &stmt)) {
            ast_free_block(block);
            return false;
        }
        ast_stmt_list_push(&block->statements, stmt);

        if (check(p, TOKEN_SEMICOLON)) {
            advance(p);
        } else if (check(p, TOKEN_EOL)) {
            advance(p);
        } else if (check(p, TOKEN_DEDENT) || check(p, TOKEN_EOF) || p->current.newline_before) {
            continue;
        } else {
            syntax_error(p, "end of line, ';', or end of block");
            ast_free_block(block);
            return false;
        }
    }

    if (block->statements.count == 0) {
        syntax_error(p, "non-empty block body");
        ast_free_block(block);
        return false;
    }

    *out_block = block;
    return true;
}

/* ==================== Functions and Program ==================== */

bool parse_opt_main(Parser *p, bool *is_main) {
    if (check(p, TOKEN_AT_MAIN)) {
        advance(p);
        if (check(p, TOKEN_EOL)) {
            advance(p);
        }
        *is_main = true;
        return true;
    }
    if (check_keyword(p, KW_DEF)) {
        *is_main = false;
        return true;
    }
    return syntax_error(p, "'@main' or 'def'");
}

bool parse_function(Parser *p) {
    position pos = { (size_t)p->current.line, (size_t)p->current.column };
    bool is_main = false;
    if (!parse_opt_main(p, &is_main)) {
        return false;
    }

    if (!parser_expect_keyword(p, KW_DEF, "'def'")) {
        return false;
    }

    if (!check(p, TOKEN_IDENTIFIER)) {
        return syntax_error(p, "function name");
    }
    char *name = take_string(p);
    advance(p);

    if (!parser_expect(p, TOKEN_LEFT_PAREN, "'('")) {
        free(name);
        return false;
    }

    AstFunction *func = safe_calloc(1, sizeof(AstFunction));
    func->pos = pos;
    func->name = name;
    func->is_main = is_main;

    if (!parse_parameters(p, &func->parameters)) {
        ast_free_function(func);
        return false;
    }

    if (!parser_expect(p, TOKEN_RIGHT_PAREN, "')'")) {
        ast_free_function(func);
        return false;
    }

    /* Support for PARAMLISTS extension: multiple parameter lists def f()(x: Int) */
    while (check(p, TOKEN_LEFT_PAREN)) {
        advance(p);
        if (!parse_parameters(p, &func->parameters)) {
            ast_free_function(func);
            return false;
        }
        if (!parser_expect(p, TOKEN_RIGHT_PAREN, "')'")) {
            ast_free_function(func);
            return false;
        }
    }

    func->return_type = parse_opt_type(p);
    if (p->error != ERR_OK) {
        ast_free_function(func);
        return false;
    }

    if (!parser_expect_operator(p, OP_ASSIGN, "'='")) {
        ast_free_function(func);
        return false;
    }

    if (check(p, TOKEN_EOL)) {
        advance(p);
        if (!parse_block(p, &func->inside)) {
            ast_free_function(func);
            return false;
        }
    } else {
        /* Single expression body on same line */
        AstExpression *expr = NULL;
        if (!parse_expression(p, &expr)) {
            ast_free_function(func);
            return false;
        }
        func->inside = ast_block_create(pos);
        ast_stmt_list_push(&func->inside->statements, ast_stmt_expression(expr, pos));
        if (check(p, TOKEN_EOL)) {
            advance(p);
        }
    }

    ast_definition_push(p->program, (AstDefinition){ .kind = AST_DEF_FUNCTION, .data.function = func });
    return true;
}

bool parse_definitions(Parser *p) {
    while (check_keyword(p, KW_DEF) || check(p, TOKEN_AT_MAIN)) {
        if (!parse_function(p)) {
            return false;
        }
        while (check(p, TOKEN_EOL)) {
            advance(p);
        }
    }

    if (check(p, TOKEN_EOF)) {
        return true;
    }

    return syntax_error(p, "'def' or end of file");
}

/* 2. <prolog> -> IMPORT ID DOT MUL EOL */
bool parse_prolog(Parser *p) {
    if (!parser_expect_keyword(p, KW_IMPORT, "'import'")) {
        return false;
    }

    if (!check(p, TOKEN_IDENTIFIER) || strcmp(p->current.string, "ifj26") != 0) {
        return syntax_error(p, "'ifj26'");
    }
    advance(p);

    if (!parser_expect(p, TOKEN_DOT, "'.'")) {
        return false;
    }
    if (!parser_expect_operator(p, OP_MUL, "'*'")) {
        return false;
    }
    if (!parser_expect(p, TOKEN_EOL, "end of line after prolog")) {
        return false;
    }
    while (check(p, TOKEN_EOL)) {
        advance(p);
    }
    return true;
}

/* 1. <program> -> <prolog> <definitions> EOF */
bool parse_program(Parser *p) {
    if (!parse_prolog(p)) {
        return false;
    }
    if (!parse_definitions(p)) {
        return false;
    }
    if (!parser_expect(p, TOKEN_EOF, "end of file")) {
        return false;
    }
    return true;
}

/* ==================== Public entry point ==================== */

ErrorCode parser_run(Scanner *scanner, AstProgram **out_ast) {
    AstProgram *prog = safe_malloc(sizeof(AstProgram));
    prog->items = NULL;
    prog->count = 0;
    prog->capacity = 0;

    Parser p;
    p.scanner = scanner;
    p.error = ERR_OK;
    p.has_lookahead = false;
    p.program = prog;
    memset(&p.current, 0, sizeof(Token));
    memset(&p.lookahead, 0, sizeof(Token));

    advance(&p);
    parse_program(&p);

    token_free(&p.current);
    if (p.has_lookahead) {
        token_free(&p.lookahead);
    }

    if (p.error != ERR_OK) {
        ast_free_program(prog);
        free(prog);
        *out_ast = NULL;
        return p.error;
    }

    *out_ast = prog;
    return ERR_OK;
}
