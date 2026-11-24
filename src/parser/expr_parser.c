/**
 * Project: IFJ26 / IAL compiler
 * File: expr_parser.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 */

#include "parser_internal.h"

/* Temporary: accepts a single literal or identifier as an expression.
   TODO: replace with precedence parser. */
bool parse_expression(Parser *p){
    if(check(p, TOKEN_INT_LITERAL) || check(p, TOKEN_DOUBLE_LITERAL) ||
       check(p, TOKEN_STRING_LITERAL) || check(p, TOKEN_IDENTIFIER)) {
        advance(p);
        return true;
    }
    return syntax_error(p, "expression");
}