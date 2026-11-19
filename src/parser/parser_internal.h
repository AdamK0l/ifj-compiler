/**
 * Project: IFJ26 / IAL compiler
 * File: parser_internal.h
 * Authors:
 *   Peter Takáč (xtakacp00)
 *   Adam Kolacek (xkolaca00)
 */

#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "ast.h"
#include "errors.h"
#include "parser.h"
#include "scanner.h"
#include "token.h"

typedef struct {
    Scanner *scanner;
    Token current;
    Token lookahead;
    bool has_lookahead;
    ErrorCode error;
    AstProgram *program;
} Parser;

/* Token navigation and inspection */
void advance(Parser *p);
Token *peek_token(Parser *p);
char *take_string(Parser *p);
bool check(Parser *p, TokenType type);
bool check_keyword(Parser *p, KeywordType kw);
bool check_operator(Parser *p, OperatorType op);

/* Error and expectation helpers */
bool syntax_error(Parser *p, const char *expected);
bool parser_expect(Parser *p, TokenType type, const char *expected);
bool parser_expect_keyword(Parser *p, KeywordType type, const char *expected);
bool parser_expect_operator(Parser *p, OperatorType type, const char *expected);

/* Program and function grammar */
bool parse_prolog(Parser *p);
bool parse_program(Parser *p);
bool parse_definitions(Parser *p);
bool parse_function(Parser *p);
bool parse_opt_main(Parser *p, bool *is_main);
bool parse_parameters(Parser *p, AstParameterLists *param_lists);
AstType *parse_type(Parser *p);
AstType *parse_opt_type(Parser *p);

/* Statements and blocks */
bool parse_block(Parser *p, AstBLOCK **out_block);
bool parse_statement(Parser *p, AstStatement **out_stmt);

/* Expressions (defined in expr_parser.c) */
bool parse_expression(Parser *p, AstExpression **out_expr);

#endif // PARSER_INTERNAL_H
