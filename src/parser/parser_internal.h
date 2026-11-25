/**
 * Project: IFJ26 / IAL compiler
 * File: parser_internal.h
 * Authors:
 *   Peter Takáč (xtakacp00)
 */

#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "ast.h"
#include "errors.h"
#include "parser.h"
#include "scanner.h"
#include "token.h"
#include <stdbool.h>

typedef struct {
    Scanner *scanner;
    Token current;
    TokenType prev_type;   /* type of the last consumed token  */
    ErrorCode error;
} Parser;

/* ==================== Token helpers ==================== */
void advance(Parser *p);
char *take_string(Parser *p);
position current_pos(Parser *p);
bool check(Parser *p, TokenType type);
bool check_keyword(Parser *p, KeywordType kw);
bool check_operator(Parser *p, OperatorType op);
bool check_id(Parser *p, const char *name);
bool is_expr_start(Parser *p);
bool is_stmt_expr_start(Parser *p);

/* ==================== Errors and expect ==================== */
bool syntax_error(Parser *p, const char *expected);
bool parser_expect(Parser *p, TokenType type, const char *expected);
bool parser_expect_keyword(Parser *p, KeywordType kw, const char *expected);
bool parser_expect_operator(Parser *p, OperatorType op, const char *expected);

/* ==================== Program and definitions ==================== */
bool parse_program(Parser *p, AstProgram *program);
bool parse_prolog(Parser *p);
bool parse_definitions(Parser *p, AstProgram *program);
AstFunction *parse_function(Parser *p, bool allow_main);
AstExtension *parse_extension(Parser *p);
bool parse_opt_main(Parser *p, bool *is_main);
bool parse_parameters(Parser *p, AstParameterList *list);
bool parse_parameters_next(Parser *p, AstParameterList *list);
bool parse_parameter(Parser *p, AstParameterList *list);
bool parse_opt_type(Parser *p, AstType **out);
AstType *parse_type(Parser *p);
AstBLOCK *parse_function_body(Parser *p);
AstType *parse_function_type(Parser *p);

/* ==================== Blocks and statements ==================== */
AstBLOCK *parse_block(Parser *p);
bool parse_statement(Parser *p, AstStatementList *list);
bool parse_statement_next(Parser *p, AstStatementList *list);
bool parse_stmt_end(Parser *p);
AstStatement *parse_var_def(Parser *p);
AstStatement *parse_while(Parser *p);
AstExpression *parse_if(Parser *p, bool require_else);
bool parse_opt_else(Parser *p, AstBLOCK **out, bool require_else);
AstBLOCK *parse_else_tail(Parser *p, bool require_else);
AstBLOCK *parse_branch_body(Parser *p);
AstStatement *parse_for(Parser *p);
AstExpression *parse_rhs(Parser *p);
AstStatement *parse_return(Parser *p);
AstStatement *parse_id_statement(Parser *p);

/* ==================== Expressions (expr_parser.c) ==================== */
AstExpression *parse_expression(Parser *p);
AstExpression *parse_expression_rest(Parser *p, char *first_id, position pos);

#endif
