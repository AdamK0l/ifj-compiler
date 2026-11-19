/**
 * Project: IFJ26 / IAL compiler
 * File: token.h
 * Authors:
 *   Bálint Marcell Šógor (xsogorb00)
 */

#ifndef TOKEN_H
#define TOKEN_H

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
    // keyword and operator
    TOKEN_KEYWORD,
    TOKEN_OPERATOR,
    // @main annotation
    TOKEN_AT_MAIN,
    // structure identifiers
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_DOT,
    // a standalone "_"
    TOKEN_UNDERSCORE,
    // INTERPOLATEDS extension
    TOKEN_ISTRING_BEGIN,
    TOKEN_ISTRING_PART,
    TOKEN_ISTRING_ID,
    TOKEN_ISTRING_EXPR_BEGIN,
    TOKEN_ISTRING_EXPR_END,
    TOKEN_ISTRING_END
} TokenType;

typedef enum {
    // basic keywords
    KW_IMPORT,
    KW_DEF,
    KW_DO,
    KW_WHILE,
    KW_IF,
    KW_ELSE,
    KW_THEN,
    KW_INT,
    KW_DOUBLE,
    KW_STRING,
    KW_RETURN,
    KW_UNIT,
    KW_VAL,
    KW_VAR,
    // keywords from extensions
    KW_EXTENSION,
    KW_BOOLEAN,
    KW_TRUE,
    KW_FALSE,
    KW_FOR,
    KW_CASE,
    KW_CLASS,
    KW_LIST,
    KW_MATCH,
    KW_TYPE,
    KW_COUNT,
} KeywordType;

typedef enum {
    OP_PLUS,
    OP_MINUS,
    OP_MUL,
    OP_DIV,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_EQUAL,
    OP_GREATER_EQUAL,
    OP_GREATER,
    OP_NOT_EQUAL,
    OP_ASSIGN,
    OP_NOT,
    OP_AND,
    OP_OR,
    OP_LEFT_ARR,
    OP_BIG_ARR,
    OP_COUNT,
} OperatorType;

typedef struct {
    TokenType type;
    KeywordType keyword;   // valid if type == TOKEN_KEYWORD
    OperatorType operator; // valid if type == TOKEN_OPERATOR
    char *string;          // identifier or string value
    long long int_value;   // value of int (if applicable)
    double double_value;   // value of double (if applicable)
    int line;              // line of first char in token
    int column;            // column of first char in token
    bool newline_before;   // true if there was a new line since last token
} Token;

// frees the string of the token
void token_free(Token *token);

// returns true if the token type is a keyword and there is a keyword
bool token_is_keyword(const Token *token, KeywordType keyword);

// returns true if the token type is an operator and there is an operator
bool token_is_operator(const Token *token, OperatorType operator);

#endif // TOKEN_H
