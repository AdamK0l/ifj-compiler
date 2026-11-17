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

// Makes a token of the given type
static ErrorCode _make_token(Token *token, TokenType type) {
    token->type = type;
    return ERR_OK;
}

// Makes an operator token
static ErrorCode _operator_token(Token *token, OperatorType operator) {
    token->type = TOKEN_OPERATOR;
    token->operator = operator;
    return ERR_OK;
}

// If the current character is the expected one eats it, and returns true
static bool _match(Scanner *scanner, int expected) {
    if (scanner->current != expected) {
        return false;
    }
    _next_char(scanner);
    return true;
}

// Prints error at the token and returns ERR_LEXICAL
static ErrorCode _token_error(const Token *token, const char *message) {
    position pos;
    pos.line = token->line;
    pos.column = token->column;

    error_print(ERR_LEXICAL, pos, "%s", message);
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

    switch (scanner->current) {
    // Parentheses, for the time being it doesnt handle indentations
    case '(':
        _next_char(scanner);
        return _make_token(token, TOKEN_LEFT_PAREN);
    case ')':
        _next_char(scanner);
        return _make_token(token, TOKEN_RIGHT_PAREN);
    // Punctuation
    case ':':
        _next_char(scanner);
        return _make_token(token, TOKEN_COLON);
    case ';':
        _next_char(scanner);
        return _make_token(token, TOKEN_SEMICOLON);
    case ',':
        _next_char(scanner);
        return _make_token(token, TOKEN_COMMA);
    // for the time being doubles with a dot are not recognised
    case '.':
        _next_char(scanner);
        return _make_token(token, TOKEN_DOT);

    // Operators
    case '+':
        _next_char(scanner);
        return _operator_token(token, OP_PLUS);
    case '-':
        _next_char(scanner);
        return _operator_token(token, OP_MINUS);
    case '*':
        _next_char(scanner);
        return _operator_token(token, OP_MUL);
    // for the time being comments are not recognised
    case '/':
        _next_char(scanner);
        return _operator_token(token, OP_DIV);

    // Variable length operators
    case '<':
        _next_char(scanner);
        if (_match(scanner, '=')) {
            return _operator_token(token, OP_LESS_EQUAL);
        }
        if (_match(scanner, '-')) {
            return _operator_token(token, OP_LEFT_ARR);
        }
        return _operator_token(token, OP_LESS);
    case '>':
        _next_char(scanner);
        if (_match(scanner, '=')) {
            return _operator_token(token, OP_GREATER_EQUAL);
        }
        return _operator_token(token, OP_GREATER);
    case '=':
        _next_char(scanner);
        if (_match(scanner, '=')) {
            return _operator_token(token, OP_EQUAL);
        }
        if (_match(scanner, '>')) {
            return _operator_token(token, OP_BIG_ARR);
        }
        return _operator_token(token, OP_ASSIGN);
    case '!':
        _next_char(scanner);
        if (_match(scanner, '=')) {
            return _operator_token(token, OP_NOT_EQUAL);
        }
        return _operator_token(token, OP_NOT);

    // Logical operators
    case '&':
        _next_char(scanner);
        if (_match(scanner, '&')) {
            return _operator_token(token, OP_AND);
        }
        return _token_error(token, "expected '&&', a single '&' is not allowed");
    case '|':
        _next_char(scanner);
        if (_match(scanner, '|')) {
            return _operator_token(token, OP_OR);
        }
        return _token_error(token, "expected '||', a single '|' is not allowed");

    default:
        // for the time being identifiers, numbers and strings are not recognised.
        return _lexical_error(scanner);
    }
}
