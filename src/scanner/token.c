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