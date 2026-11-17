/**
 * Project: IFJ26 / IAL compiler
 * File: scanner.h
 * Authors:
 *   Bálint Marcell Šógor (xsogorb00)
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "dstring.h"
#include "errors.h"
#include "token.h"
#include <stdio.h>

typedef struct {
    FILE *input;          // File with the source code
    int current;          // Current character
    int next;             // Next character
    int line;             // Line of current character
    int column;           // Column of current character
    DynamicString buffer; // Text we are reading
} Scanner;

// Initialises the scanner to read from input, returns ERR_OK or ERR_INTERNAL
ErrorCode scanner_init(Scanner *scanner, FILE *input);

// Frees the memory of the scanner
void scanner_destroy(Scanner *scanner);

// Reads the next token into token, returns: ERR_OK, ERR_LEXICAL, ERR_INTERNAL
// At EOF returns TOKEN_EOF
ErrorCode scanner_get_next_token(Scanner *scanner, Token *token);

#endif
