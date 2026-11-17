/**
 * Project: IFJ26 / IAL compiler
 * File: token.c
 * Authors:
 *   Bálint Marcell Šógor (xsogorb00)
 */

#include "token.h"

#include <stdlib.h>

void token_free(Token *token) {
    free(token->string);
    token->string = NULL;
}

bool token_is_keyword(const Token *token, KeywordType keyword) {
    return token->type == TOKEN_KEYWORD && token->keyword == keyword;
}

bool token_is_operator(const Token *token, OperatorType operator) {
    return token->type == TOKEN_KEYWORD && token->operator == operator;
}