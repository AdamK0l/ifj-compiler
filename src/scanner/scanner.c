/**
 * Project: IFJ26 / IAL compiler
 * File: scanner.c
 * Authors:
 *   Bálint Marcell Šógor (xsogorb00)
 */

#include "scanner.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Text of every keyword, index in the array is its KeywordType
static const char *const KEYWORD_NAMES[KW_COUNT] = {
    [KW_IMPORT] = "import",
    [KW_DEF] = "def",
    [KW_DO] = "do",
    [KW_WHILE] = "while",
    [KW_IF] = "if",
    [KW_ELSE] = "else",
    [KW_THEN] = "then",
    [KW_INT] = "Int",
    [KW_DOUBLE] = "Double",
    [KW_STRING] = "String",
    [KW_RETURN] = "return",
    [KW_UNIT] = "Unit",
    [KW_VAL] = "val",
    [KW_VAR] = "var",

    [KW_EXTENSION] = "extension",
    [KW_BOOLEAN] = "Boolean",
    [KW_TRUE] = "true",
    [KW_FALSE] = "false",
    [KW_FOR] = "for",
    [KW_CASE] = "case",
    [KW_CLASS] = "class",
    [KW_LIST] = "List",
    [KW_MATCH] = "match",
    [KW_TYPE] = "type",
};

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

// Adds the current character to the buffer and moves to the next one
static ErrorCode _add_char(Scanner *scanner) {
    if (!dstring_append_char(&scanner->buffer, (char)scanner->current)) {
        error_print_no_pos(ERR_INTERNAL, "out of memory");
        return ERR_INTERNAL;
    }
    _next_char(scanner);
    return ERR_OK;
}

// Returns true if the character is alphanumeric or '_'
static bool _is_alphanum(int c) {
    return isalnum(c) || c == '_';
}

// Reads alphanumeric or '_' characters into the buffer
static ErrorCode _read_word(Scanner *scanner) {
    dstring_clear(&scanner->buffer);
    while (_is_alphanum(scanner->current)) {
        ErrorCode result = _add_char(scanner);
        if (result != ERR_OK) {
            return result;
        }
    }
    return ERR_OK;
}

// Reads after '@', only @main is a valid token
static ErrorCode _read_main(Scanner *scanner, Token *token) {
    // Collects the word after '@' into the buffer
    ErrorCode result = _read_word(scanner);
    if (result != ERR_OK) {
        return result;
    }

    // The word in the buffer must equal "main", any other form throws an error
    if (dstring_compare_str(&scanner->buffer, "main") != 0) {
        return _token_error(token, "expected @main");
    }
    return _make_token(token, TOKEN_AT_MAIN);
}

// Copies the text from the buffer into the string of the token
static ErrorCode _copy_buffer(Scanner *scanner, Token *token) {
    // +1 for '\0'
    token->string = malloc(scanner->buffer.length + 1);
    if (token->string == NULL) {
        error_print_no_pos(ERR_INTERNAL, "out of memory");
        return ERR_INTERNAL;
    }
    strcpy(token->string, scanner->buffer.data);
    return ERR_OK;
}

// Searches the word in the keyword table, returns tue if it finds it and sets the keyword
static bool _find_keyword(const DynamicString *word, KeywordType *keyword) {
    for (int i = 0; i < KW_COUNT; i++) {
        if (dstring_compare_str(word, KEYWORD_NAMES[i]) == 0) {
            *keyword = (KeywordType)i;
            return true;
        }
    }
    return false;
}

// Reads the identifier or keyword, first character must be a letter or '_'
static ErrorCode _read_identifier(Scanner *scanner, Token *token) {
    // Collects the word into the buffer
    ErrorCode result = _read_word(scanner);
    if (result != ERR_OK) {
        return result;
    }

    // The '_' is a special token
    if (dstring_compare_str(&scanner->buffer, "_") == 0) {
        return _make_token(token, TOKEN_UNDERSCORE);
    }

    /* Keywords have the same properties like identifiers (starts with a letter)
    so we can search them with the same function.
    */
    KeywordType keyword;
    if (_find_keyword(&scanner->buffer, &keyword)) {
        token->type = TOKEN_KEYWORD;
        token->keyword = keyword;
        return ERR_OK;
    }

    // The token type is set and the name gets copied into the string of the token
    token->type = TOKEN_IDENTIFIER;
    return _copy_buffer(scanner, token);
}

// Reads an integer, the first char is a digit
static ErrorCode _read_number(Scanner *scanner, Token *token) {
    // Collects the digits into the cleared buffer, '_' is skipped if inside the number
    dstring_clear(&scanner->buffer);
    while (isdigit(scanner->current) || scanner->current == '_') {
        if (scanner->current == '_') {
            while (scanner->current == '_') {
                _next_char(scanner);
            }
            if (!isdigit(scanner->current)) {
                return _token_error(token, "'_' in a number must be followed by a digit");
            }
        }

        ErrorCode result = _add_char(scanner);
        if (result != ERR_OK) {
            return result;
        }
    }

    // for the time being doubles are not recognised
    // The text of the number is converted to long long on base 10
    token->type = TOKEN_INT_LITERAL;
    token->int_value = strtoll(scanner->buffer.data, NULL, 10);
    return ERR_OK;
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

    // For identifiers, they start with a letter or '_'
    if (isalpha(scanner->current) || scanner->current == '_') {
        return _read_identifier(scanner, token);
    }

    // For numbers that satrt with a digit
    if (isdigit(scanner->current)) {
        return _read_number(scanner, token);
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

    // The @main annotation
    case '@':
        _next_char(scanner);
        return _read_main(scanner, token);

    default:
        // for the time being strings are not recognised.
        return _lexical_error(scanner);
    }
}
