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

bool check_operator(Parser *p, OperatorType op){
    bool b = token_is_operator(&p->current, op);
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

ErrorCode parser_run(Scanner *scanner) {
    Parser p;
    p.scanner = scanner;
    p.error = ERR_OK;
    p.current.string = NULL;

    advance(&p);
    parse_program(&p);

    token_free(&p.current);
    return p.error;
}


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

/*1.  <program> -> <prolog> <definitions> EOF */
bool parse_program(Parser *p){
    if(!parse_prolog(p)) return false;

    if(!parse_definitions(p)) return false;

    if(!parser_expect(p, TOKEN_EOF, "end of file")) return false;
    return true;
}

/* 3. <definitions> -> <function> <definitions>   PREDICT: { DEF, AT_MAIN }
   4. <definitions> -> ε                          PREDICT: { EOF } */
bool parse_definitions(Parser *p) {
    if(check_keyword(p, KW_DEF) || check(p, TOKEN_AT_MAIN)){
        if(!parse_function(p)) return false;
        return parse_definitions(p);

    }
    if(check(p, TOKEN_EOF)) {
        return true;
    }
    return syntax_error(p, "'def' or end of file");
}

/* 5. <function> -> <opt_main> DEF ID ( <parameters> ) <opt_type> ASSIGN EOL <block> */
bool parse_function(Parser *p){
    if(!parse_opt_main(p)) return false;

    if(!parser_expect_keyword(p, KW_DEF, "'def'")) return false;

    if(!check(p, TOKEN_IDENTIFIER)) return syntax_error(p, "function name");
    char *name = take_string(p);
    advance(p);
    free(name); //temporary free

    if(!parser_expect(p, TOKEN_LEFT_PAREN, "'('")) return false;

    AstParameterList params = {.items = NULL, .count = 0, .capacity = 0};
    if(!parse_parameters(p, &params)){
        ast_parameter_list_free(&params);
        return false;
    }
    ast_parameter_list_free(&params); // temporary free, TODO: put into function AST node

    if(!parser_expect(p, TOKEN_RIGHT_PAREN, "')'")) return false;

    AstType *ret_type = NULL;
    if(!parse_opt_type(p, &ret_type)) return false;
    ast_type_free(ret_type);//temporary free to avoid leaks til i dont create an AST node 

    if(!parser_expect_operator(p, OP_ASSIGN, "'='")) return false;

    if(!parser_expect(p, TOKEN_EOL, "end of line")) return false;

    if(!parse_block(p)) return false;

    return true;
}

bool parse_parameters(Parser *p, AstParameterList *list){
    if(check(p, TOKEN_IDENTIFIER)) {
        
        if(!parse_parameter(p, list)) return false;
        return parse_parameters_next(p, list);
    } else if(check(p, TOKEN_RIGHT_PAREN)) return true; 

    return syntax_error(p, "'parameter or ')'");
    
}

bool parse_opt_main(Parser *p){
    if(check(p, TOKEN_AT_MAIN)) {
        advance(p);
        if(check(p, TOKEN_EOL)) {
            advance(p);
        }
        return true;
    }
    if (check_keyword(p, KW_DEF)) {
        return true;
    }
    return syntax_error(p, "'@main' or 'def'");

}

/*
8.  <type>	-> INT			PREDICT: { INT }
9.  <type>  -> DOUBLE		PREDICT: { DOUBLE }
10. <type>	-> STRING		PREDICT: { STRING }
11. <type>	-> UNIT			PREDICT: { UNIT }
12. <type>	-> BOOLEAN		PREDICT: { BOOLEAN }
*/
AstType *parse_type(Parser *p){
    AstTypeKind kind;

    if(check_keyword(p, KW_INT)) {
        kind = AST_TYPE_INT;
    }else if (check_keyword(p, KW_DOUBLE)){
        kind = AST_TYPE_DOUBLE;
    }else if (check_keyword(p, KW_STRING)){
        kind = AST_TYPE_STRING;
    }else if (check_keyword(p, KW_BOOLEAN)){
        kind =  AST_TYPE_BOOLEAN;
    }else if (check_keyword(p, KW_UNIT)) {
        kind = AST_TYPE_UNIT;
    }else {
        syntax_error(p, "type");
        return NULL;
    }
    advance(p);
    return ast_type_create(kind);
    
}

/* 
6.  <opt_type>	-> COLON <type>	     PREDICT: { COLON }
7.  <opt_type>	-> epsilon		     PREDICT: { ASSIGN }
*/
bool parse_opt_type(Parser *p, AstType **out){
    *out = NULL;

    if(check(p, TOKEN_COLON)) {
        advance(p);
        *out = parse_type(p);
        return *out != NULL;
    }
    if(check_operator(p, OP_ASSIGN)) {
        return true;
    }
    return syntax_error(p, "':' or '='");
}

bool parse_block(Parser *p){
    if(!parser_expect(p, TOKEN_INDENT, "indented block")) return false;

    if(!parse_statement(p)) return false;

    if(!parse_statement_next(p)) return false;

    if(!parser_expect(p, TOKEN_DEDENT, "end of block")) return false;
    return true;
}

bool parse_parameter(Parser *p, AstParameterList *list){
    if(!check(p, TOKEN_IDENTIFIER)) return syntax_error(p, "parameter name");
    position pos = {p->current.line, p->current.column };
    char *name = take_string(p);
    advance(p);

    if(!parser_expect(p, TOKEN_COLON, "':'")){
        free(name);
        return false;
    }

    AstType *type = parse_type(p);
    if(type == NULL){
        free(name);
        return false;
    }

    AstParameter param = {.name = name, .type = type, .pos = pos};
    ast_parameter_list_push(list, param);
    return true;
}

bool parse_parameters_next(Parser *p, AstParameterList *list){
    if(check(p, TOKEN_COMMA)) {
        advance(p);
        if(!parse_parameter(p, list)) return false;
        return parse_parameters_next(p, list);
    } else if(check(p, TOKEN_RIGHT_PAREN)) return true; 

    return syntax_error(p, "',' or ')'");
    
}

bool parse_statement(Parser *p){
    if(!parse_var_def(p)) return false;

    if(!parser_expect(p, TOKEN_EOL, "'end of line'")) return false;
    return true;
}

bool parse_statement_next(Parser *p){
    if(check_keyword(p, KW_VAR) || check_keyword(p, KW_VAL)){
        if(!parse_statement(p)) return false;
        return parse_statement_next(p);
    }
    if(check(p, TOKEN_DEDENT)){
        return true;
    }
    return syntax_error(p, "statement or end of block");
    
}

bool parse_var_def(Parser *p){
    bool is_mutable; // TODO: put in var AST node
    if(check_keyword(p, KW_VAR)) {
        is_mutable = true;
    } else if(check_keyword(p, KW_VAL)) {
        is_mutable = false;
    } else{
        return syntax_error(p, "'var' or 'val'");
    }
    advance(p);

    if(!check(p, TOKEN_IDENTIFIER)) return syntax_error(p, "variable or value name");
    char *name = take_string(p);
    advance(p);
    (void)is_mutable; // TODO: put in var AST node

    AstType *type = NULL;
    if(!parse_opt_type(p, &type)){
        free(name); // TODO: put in AST node
        return false;
    } 

    if(!parser_expect_operator(p, OP_ASSIGN, "'='")){
        free(name);
        if(type != NULL) ast_type_free(type);
        return false;
    }

    if(!parse_expression(p)){
        free(name);
        ast_type_free(type);
        return false;
    }

    free(name);            // TODO: put into variable AST node
    ast_type_free(type);   // TODO: put into variable AST node
    return true;
}