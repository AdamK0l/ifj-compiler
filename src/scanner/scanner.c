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

// Returns true for space, tab, \r
static bool _is_whitespace(int c) {
    return c == ' ' || c == '\t' || c == '\r';
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
    // This rejects any number that starts with a 0 (01 or 0_1) except the 0 itself
    if (scanner->current == '0' && (isdigit(scanner->next) || scanner->next == '_')) {
        return _token_error(token, "an integer literal must not have a leading 0");
    }

    // Collects the digits into the cleared buffer, '_' is skipped if inside the number
    dstring_clear(&scanner->buffer);
    while (isdigit(scanner->current) || scanner->current == '_') {
        if (scanner->current == '_') {
            // There can be multiple '_' between two digits
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

// Pushes new indentation level in the stack
static ErrorCode _push_indent(Scanner *scanner, int indent) {
    // If the stack is full it gets bigger
    if (scanner->indent_count == scanner->indent_capacity) {
        int new_capacity = scanner->indent_capacity * 2;
        int *new_stack = realloc(scanner->indent_stack, new_capacity * sizeof(int));
        if (new_stack == NULL) {
            error_print_no_pos(ERR_INTERNAL, "out of memory");
            return ERR_INTERNAL;
        }
        scanner->indent_stack = new_stack;
        scanner->indent_capacity = new_capacity;
    }

    scanner->indent_stack[scanner->indent_count] = indent;
    scanner->indent_count++;
    return ERR_OK;
}

// Compare the indent in the new line with the top of the indent stack
// made_token is true if an INDENT or DEDENT token was created
static ErrorCode _check_indent(Scanner *scanner, Token *token, bool *made_token) {
    *made_token = false;

    // The indent is the number of spaces before the first token in the line
    // (measured in the whitespace loop)
    int indent = scanner->line_indent;
    int top = scanner->indent_stack[scanner->indent_count - 1];

    // A bigger indent opens a new block
    if (indent > top) {
        ErrorCode result = _push_indent(scanner, indent);
        if (result != ERR_OK) {
            return result;
        }
        *made_token = true;
        return _make_token(token, TOKEN_INDENT);
    }

    // A smaller indent closes block, there is one DEDENT for every closed level
    while (indent < scanner->indent_stack[scanner->indent_count - 1]) {
        scanner->indent_count--;
        scanner->pending_dedents++;
    }

    // The indent must be the same as on the outer levels
    if (indent != scanner->indent_stack[scanner->indent_count - 1]) {
        return _token_error(token, "indentation does not match any outer level");
    }

    // The first DEDENT is returned now, others with the next calls
    if (scanner->pending_dedents > 0) {
        scanner->pending_dedents--;
        *made_token = true;
        return _make_token(token, TOKEN_DEDENT);
    }
    return ERR_OK;
}

// Skips block comments, nested comments are allowed
// had_newline is true, if there was a line break inside the comment
static ErrorCode _skip_block_comment(Scanner *scanner, Token *token, bool *had_newline) {
    // An error is reported at the start of the comment
    token->line = scanner->line;
    token->column = scanner->column;
    *had_newline = false;

    // Skips the opening tag (/*)
    _next_char(scanner);
    _next_char(scanner);
    int depth = 1;

    // The depth counts open comments, it ends when it goes back to 0
    while (depth > 0) {
        if (scanner->current == EOF) {
            return _token_error(token, "unterminated block comment");
        }

        if (scanner->current == '/' && scanner->next == '*') {
            _next_char(scanner);
            _next_char(scanner);
            depth++;
        } else if (scanner->current == '*' && scanner->next == '/') {
            _next_char(scanner);
            _next_char(scanner);
            depth--;
        } else {
            if (scanner->current == '\n') {
                *had_newline = true;
            }
            _next_char(scanner);
        }
    }
    return ERR_OK;
}

ErrorCode scanner_init(Scanner *scanner, FILE *input) {
    // Check function inputs
    if (scanner == NULL || input == NULL) {
        return ERR_INTERNAL;
    }

    // Feed the scanner the input and initialise the variables
    scanner->input = input;
    scanner->line = 1;
    scanner->column = 1;
    scanner->line_has_token = false;
    scanner->parenth_depth = 0;
    scanner->newline_seen = false;
    scanner->indent_checked = false;
    scanner->pending_dedents = 0;
    scanner->at_line_start = true;
    scanner->tab_in_indent = false;
    scanner->line_indent = -1;

    // Initialise lexem buffer, internal error if it fails
    if (!dstring_init(&scanner->buffer)) {
        return ERR_INTERNAL;
    }

    // Initialise indent stack, starts with level 0
    scanner->indent_capacity = 8;
    scanner->indent_stack = malloc(scanner->indent_capacity * sizeof(int));
    if (scanner->indent_stack == NULL) {
        dstring_free(&scanner->buffer);
        return ERR_INTERNAL;
    }
    scanner->indent_stack[0] = 0;
    scanner->indent_count = 1;

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

    free(scanner->indent_stack);
    scanner->indent_stack = NULL;
}

// Reads one token from current character, whitespace is already skipped
static ErrorCode _read_token(Scanner *scanner, Token *token) {
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

ErrorCode scanner_get_next_token(Scanner *scanner, Token *token) {
    // Initialise with empty token
    Token empty = {0};
    *token = empty;

    // DEDENT tokens that are waiting from the last indent check
    if (scanner->pending_dedents > 0) {
        scanner->pending_dedents--;
        token->line = scanner->line;
        token->column = scanner->column;
        return _make_token(token, TOKEN_DEDENT);
    }

    // Skips whitespaces and empty lines, line break after a token ends the line with EOL
    while (true) {
        // Spaces, tabs and \r is skipped
        while (_is_whitespace(scanner->current)) {
            // Tab is not allowed before the first token in the line
            if (scanner->current == '\t' && scanner->at_line_start) {
                scanner->tab_in_indent = true;
            }
            _next_char(scanner);
        }

        // The indentation is measured from the first non-whitespace character
        if (scanner->line_indent < 0) {
            scanner->line_indent = scanner->column - 1;
        }

        // A line comment is skipped until \n or EOF, line break is handled below
        if (scanner->current == '/' && scanner->next == '/') {
            while (scanner->current != '\n' && scanner->current != EOF) {
                _next_char(scanner);
            }
            continue;
        }

        //
        if (scanner->current == '/' && scanner->next == '*') {
            bool had_newline;
            ErrorCode result = _skip_block_comment(scanner, token, &had_newline);
            if (result != ERR_OK) {
                return result;
            }
            // for the time being a line break inside a comment is whitespace
            continue;
        }

        if (scanner->current != '\n') {
            break;
        }

        // If the line had a token, put and EOL token at the line break
        // Inside parentheses line break is only whitespace
        if (scanner->line_has_token && scanner->parenth_depth == 0) {
            token->line = scanner->line;
            token->column = scanner->column;
            _next_char(scanner);
            scanner->line_has_token = false;
            scanner->newline_seen = true;
            scanner->indent_checked = false;
            scanner->at_line_start = true;
            scanner->tab_in_indent = false;
            scanner->line_indent = -1;
            return _make_token(token, TOKEN_EOL);
        }

        // Empty lines and line breaks inside parentheses produce nothing
        _next_char(scanner);
        scanner->newline_seen = true;
        scanner->at_line_start = true;
        scanner->tab_in_indent = false;
        scanner->line_indent = -1;
    }

    // The token starts at the current character
    token->line = scanner->line;
    token->column = scanner->column;

    // End of the input, every next call returns TOKEN_EOF
    if (scanner->current == EOF) {
        // If the last line has no line break at the end, but it still needs EOL before EOF
        if (scanner->line_has_token) {
            scanner->line_has_token = false;
            return _make_token(token, TOKEN_EOL);
        }

        // Every block that is still open needs to be closed with DEDENT
        if (scanner->indent_count > 1) {
            scanner->indent_count--;
            return _make_token(token, TOKEN_DEDENT);
        }

        token->type = TOKEN_EOF;
        return ERR_OK;
    }

    // Tab is the first token in line and line is not empty
    if (scanner->tab_in_indent) {
        return _token_error(token, "tab is not allowed in indentation");
    }

    if (scanner->parenth_depth > 0 && scanner->at_line_start && scanner->current != ')') {
        int top = scanner->indent_stack[scanner->indent_count - 1];
        if (scanner->line_indent <= top) {
            return _token_error(token, "continuation line must be indented more");
        }
    }

    // At the start of a line indentation is checked
    if (!scanner->line_has_token && !scanner->indent_checked) {
        scanner->indent_checked = true;
        bool made_token;
        ErrorCode result = _check_indent(scanner, token, &made_token);
        if (result != ERR_OK || made_token) {
            return result;
        }
    }

    ErrorCode result = _read_token(scanner, token);
    if (result != ERR_OK) {
        return result;
    }

    // The line has a token now, so the next line break gives EOL
    scanner->line_has_token = true;

    // The parser needs to know if the token is the first one in its line
    token->newline_before = scanner->newline_seen;
    scanner->newline_seen = false;
    scanner->at_line_start = false;

    // Counts the open parentheses, a wrong ')' is reported by the parser
    if (token->type == TOKEN_LEFT_PAREN) {
        scanner->parenth_depth++;
    } else if (token->type == TOKEN_RIGHT_PAREN && scanner->parenth_depth > 0) {
        scanner->parenth_depth--;
    }

    return ERR_OK;
}
