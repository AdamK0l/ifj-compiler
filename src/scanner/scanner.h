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
    bool line_has_token;  // True, if current line has a token
    bool newline_seen;    // True, if there was a line break since last token
    int parenth_depth;    // Number of open parentheses
    int *indent_stack;    // Indentation levels, bottom is always 0
    int indent_count;     // Number of levels on the stack
    int indent_capacity;  // Allocated size of the stack
    bool indent_checked;  // True, if the indentation on the current line was checked
    int pending_dedents;  // Number of DEDENT tokens that have to be returned
    bool at_line_start;   // True, if no token was read on the current line
    bool tab_in_indent;   // True, if there was a tab before the first token on the line
    int line_indent;      // Spaces at the start of the line, default -1
    bool after_comment;   // True, if a comment ended with line break on this line
} Scanner;

// Initialises the scanner to read from input, returns ERR_OK or ERR_INTERNAL
ErrorCode scanner_init(Scanner *scanner, FILE *input);

// Frees the memory of the scanner
void scanner_destroy(Scanner *scanner);

// Reads the next token into token, returns: ERR_OK, ERR_LEXICAL, ERR_INTERNAL
// At EOF returns TOKEN_EOF
ErrorCode scanner_get_next_token(Scanner *scanner, Token *token);

#endif
