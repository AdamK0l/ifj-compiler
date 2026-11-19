/**
 * Project: IFJ26 / IAL compiler
 * File: parser.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 */

#include "parser_internal.h"
#include <stdlib.h> 
#include <string.h>

/* ==================== Token helpers ==================== */

/* Frees the current token and reads the next one from the scanner. */
void advance(Parser *p) {
    token_free(&p->current);
    ErrorCode err = scanner_get_next_token(p->scanner, &p->current);
    if(err != ERR_OK){
        if(p->error == ERR_OK) {
            p->error = err;
        }
        p->current.type = TOKEN_EOF;
    }
}

/* Takes the current token's string, so advance will not free it. */
char *take_string(Parser *p){
    char *s = p->current.string;
    p->current.string = NULL;
    return s;
}

/* Returns true if the current token has the given type. */
bool check(Parser *p, TokenType type){
    return(p->current.type == type);
}

bool check_keyword(Parser *p, KeywordType kw){
    bool b = token_is_keyword(&p->current, kw);
    return b;
}

/* ==================== Error handling ==================== */

/* Reports the first syntax error and sets ERR_SYNTAX. */
bool syntax_error(Parser *p, const char *expected){
    if(p->error == ERR_OK){
        position pos = {p->current.line, p->current.column};
        error_print(ERR_SYNTAX, pos, "expected %s", expected);
        p->error = ERR_SYNTAX;
    }
    return false;
}

/* ==================== Expect helpers ==================== */

/* Consumes the current token if it has the given type, otherwise reports a syntax error. */
bool parser_expect(Parser *p, TokenType type, const char *expected){
    if (check(p, type)){
        advance(p);
        return true;
    } else {
        syntax_error(p, expected);
        return false;
    }
}

bool check_operator(Parser *p, OperatorType op){
    bool b = token_is_operator(&p->current, op);
    return b;
}

bool parser_expect_keyword(Parser *p, KeywordType type, const char *expected){
    if (check_keyword(p, type)){
        advance(p);
        return true;
    } else {
        syntax_error(p, expected);
        return false;
    }
}

bool parser_expect_operator(Parser *p, OperatorType type, const char *expected){
    if (check_operator(p, type)){
        advance(p);
        return true;
    } else {
        syntax_error(p, expected);
        return false;
    }
}

/* ==================== Parse functions ==================== */

/* 2. <prolog> -> IMPORT ID DOT MUL EOL */
bool parse_prolog(Parser *p){
    if(!parser_expect_keyword(p, KW_IMPORT, "'import'")) return false;

    if(!check(p, TOKEN_IDENTIFIER) || strcmp(p->current.string, "ifj26") != 0) {
        return syntax_error(p, "'ifj26'");
    }
    advance(p);

    if(!parser_expect(p, TOKEN_DOT, "'.'")) return false;
    if(!parser_expect_operator(p, OP_MUL, "'*'")) return false;
    if(!parser_expect(p, TOKEN_EOL, "end of line")) return false;
    return true;
}