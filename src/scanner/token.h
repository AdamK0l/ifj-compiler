/**
 * Project: IFJ26 / IAL compiler
 * File: token.h
 * Authors:
 *   Bálint Marcell Šógor (xsogorb00)
 */

#ifndef TOKEN
#define TOKEN

#include <stdbool.h>

typedef enum {
    // tokens for code layout
    TOKEN_EOF,
    TOKEN_EOL,
    TOKEN_INDENT,
    TOKEN_DEDENT,
    // identifiers and literals
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,
    TOKEN_DOUBLE_LITERAL,
    TOKEN_STRING_LITERAL,
    // ifj26 lang keyword
    TOKEN_KEYWORD,
    // @main annotation
    TOKEN_AT_MAIN,
    TOKEN_OPERATOR,
    // structure identifiers
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_DOT,
    // a standalone "_"
    TOKEN_UNDERSCORE,
    // INTERPOLATEDS extention
    TOKEN_ISTRING_BEGIN,
    TOKEN_ISTRING_PART,
    TOKEN_ISTRING_ID,
    TOKEN_ISTRING_EXPR_BEGIN,
    TOKEN_ISTRING_EXPR_END,
    TOKEN_ISTRING_END
} TokenType;

typedef struct {
    TokenType type;
    char text[64];       // for keywords and operators
    char *string;        // identifier or string value
    long long int_value; // value of int (if applicable)
    double double_value; // value of double (if applicable)
    int line;            // line of first char in token
    int column;          // column of first char in token
    bool newline_before; // true if there was a new line since last token
} Token;

// frees the string of the token
void token_free(Token *token);

#endif