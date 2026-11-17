/**
 * Project: IFJ26 / IAL compiler
 * File: scanner.c
 * Authors:
 *   Bálint Marcell Šógor (xsogorb00)
 */

#include "scanner.h"
#include <ctype.h>

// Returns true for space, tab, \r and newline
static bool _is_whitespace(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Moves to the next character and updates line and column
static void _next_char(Scanner *scanner) {
    // At EOF do nothing
    if (scanner->current == EOF) {
        return;
    }

    // At a newline character move to the next line, otherwise move along the current line
    if (scanner->current == '\n') {
        scanner->line++;
        scanner->column = 1;
    } else {
        scanner->column++;
    }

    // Shift the characters, next->current and next->(read a new one)
    scanner->current = scanner->next;
    // At EOF dont call fgetc (it would just wait)
    if (scanner->next != EOF) {
        scanner->next = fgetc(scanner->input);
    }
}

// Prints an error for the current character and returns ERR_LEXICAL
static ErrorCode _lexical_error(Scanner *scanner) {
    // Position of the wrong character
    position pos;
    pos.line = scanner->line;
    pos.column = scanner->column;

    // Printable characters use first if branch, others use the else branch
    if (isprint(scanner->current)) {
        error_print(ERR_LEXICAL, pos, "unexpected character '%c'", scanner->current);
    } else {
        error_print(ERR_LEXICAL, pos, "unexpected character with code %d", scanner->current);
    }
    return ERR_LEXICAL;
}

ErrorCode scanner_init(Scanner *scanner, FILE *input) {
    // Check function inputs
    if (scanner == NULL || input == NULL) {
        return ERR_INTERNAL;
    }

    // Feed the scanner the input and initialise line and column counters
    scanner->input = input;
    scanner->line = 1;
    scanner->column = 1;

    // Initialise lexem buffer, internal error if it fails
    if (!dstring_init(&scanner->buffer)) {
        return ERR_INTERNAL;
    }

    // Reading the first 2 characters
    scanner->current = fgetc(input);
    scanner->next = EOF;
    if (scanner->current != EOF) {
        scanner->next = fgetc(input);
    }

    return ERR_OK;
}

void scanner_destroy(Scanner *scanner) {
    if (scanner == NULL) {
        return;
    }

    dstring_free(&scanner->buffer);
    scanner->input = NULL;
}

ErrorCode scanner_get_next_token(Scanner *scanner, Token *token) {
    // Initialise with empty token
    Token empty = {0};
    *token = empty;

    // for the time being skip all whitespaces
    while (_is_whitespace(scanner->current)) {
        _next_char(scanner);
    }

    // The token starts at the current character
    token->line = scanner->line;
    token->column = scanner->column;

    // End of the input, every next call returns TOKEN_EOF
    if (scanner->current == EOF) {
        token->type = TOKEN_EOF;
        return ERR_OK;
    }

    // for the time being no other token is recognised
    return _lexical_error(scanner);
}
