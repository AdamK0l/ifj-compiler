/**
 * Project: IFJ26 / IAL compiler
 * File: parser_internal.h
 * Authors:
 *   Peter Takáč (xtakacp00)
 */

#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "scanner.h"
#include "token.h"
#include "errors.h"
#include "ast.h"


typedef struct {
    Scanner *scanner;
    Token current;
    ErrorCode error;
} Parser;

void advance(Parser *p);
char *take_string(Parser *p);
bool check(Parser *p, TokenType type);
bool check_keyword(Parser *p, KeywordType kw);
bool syntax_error(Parser *p, const char *expected);
bool parser_expect(Parser *p, TokenType type, const char *expected);
bool check_operator(Parser *p, OperatorType op);
bool parser_expect_keyword(Parser *p, KeywordType type, const char *expected);
bool parser_expect_operator(Parser *p, OperatorType type, const char *expected);
bool parse_prolog(Parser *p);

 #endif