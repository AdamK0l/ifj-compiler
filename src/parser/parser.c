/**
 * Project: IFJ26 / IAL compiler
 * File: parser.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 */

#include "parser_internal.h"
#include <stdlib.h> 

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

char *take_string(Parser *p){
    char *s = p->current.string;
    p->current.string = NULL;
    return s;
}

bool check(Parser *p, TokenType type){
    return(p->current.type == type);
}

bool check_keyword(Parser *p, KeywordType kw){
    bool b = token_is_keyword(&p->current, kw);
    return b;
}

bool syntax_error(Parser *p, const char *expected){
    if(p->error == ERR_OK){
        position pos = {p->current.line, p->current.column};
        error_print(ERR_SYNTAX, pos, "expected %s", expected);
        p->error = ERR_SYNTAX;
    }
    return false;
}

bool parser_expect(Parser *p, TokenType type, const char *expected){
    if (check(p, type)){
        advance(p);
        return true;
    } else {
        syntax_error(p, expected);
        return false;
    }
}