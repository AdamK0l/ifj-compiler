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

    if(!parse_function_body(p)) return false;

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

/* 
    15. <parameters_next>   -> COMMA <param_after_comma>      PREDICT: { COMMA }
    16. <parameters_next>   -> ε                              PREDICT: { RIGHT_PAREN }
    43. <param_after_comma> -> <parameter> <parameters_next>  PREDICT: { ID } 
    44. <param_after_comma> -> ε                              PREDICT: { RIGHT_PAREN } 
*/
bool parse_parameters_next(Parser *p, AstParameterList *list){
    if(check(p, TOKEN_COMMA)) {
        advance(p);
        if(check(p, TOKEN_RIGHT_PAREN)) return true;
        if(!parse_parameter(p, list)) return false;
        return parse_parameters_next(p, list);
    } 
    if(check(p, TOKEN_RIGHT_PAREN)) return true;

    return syntax_error(p, "',' or ')'");
    
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

/* 41. <stmt_end> -> EOL         PREDICT: { EOL }
   42. <stmt_end> -> SEMICOLON   PREDICT: { SEMICOLON } 
*/
bool parse_stmt_end(Parser *p){
    if(check(p, TOKEN_EOL)) {  
        advance(p);
        return true;
    }
    if(check(p, TOKEN_SEMICOLON)){
        advance(p);
        return true;
    }
    return syntax_error(p, "end of line or ';'");
}

bool is_expr_start(Parser *p){
    return  check(p, TOKEN_INT_LITERAL) || check(p, TOKEN_DOUBLE_LITERAL) || 
            check(p, TOKEN_STRING_LITERAL) || check(p, TOKEN_LEFT_PAREN) || check_operator(p, OP_MINUS);
}

/* 21. <statement> -> <var_def> <stmt_end>          PREDICT: { VAR, VAL }
   24. <statement> -> <while_stmt>                  PREDICT: { WHILE }
   25. <statement> -> <if_stmt>                     PREDICT: { IF }
   32. <statement> -> RETURN <opt_expr> <stmt_end>  PREDICT: { RETURN }
   35. <statement> -> ID <id_tail> <stmt_end>       PREDICT: { ID }
   38. <statement> -> expr <stmt_end>               PREDICT: { INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN, MINUS } 
*/
bool parse_statement(Parser *p){
    if(check_keyword(p, KW_VAR) || check_keyword(p, KW_VAL)) {
        if(!parse_var_def(p)) return false;
        return parse_stmt_end(p);
    }
    if(check_keyword(p, KW_WHILE)) {
        return parse_while(p);
    }
    if(check_keyword(p, KW_IF)) {
        return parse_if(p);
    }
    if(check_keyword(p, KW_RETURN)) {
        return parse_return(p);
    }
    if(check(p, TOKEN_IDENTIFIER)) {
        return parse_id_statement(p);
    }
    if(is_expr_start(p)) {
        if(!parse_expression(p)) return false;
        return parse_stmt_end(p);
    }
    return syntax_error(p, "statement");
}

/* 19. <statement_next> -> <statement> <statement_next>
       PREDICT: { VAR, VAL, WHILE, IF, RETURN, ID, INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN, MINUS }
   20. <statement_next> -> ε   PREDICT: { DEDENT } 
*/
bool parse_statement_next(Parser *p){
    if(check(p, TOKEN_DEDENT)) { 
        return true;
    } 
    if(!parse_statement(p)) return false;
    return parse_statement_next(p);
}

/*26. <while_stmt> -> WHILE expr DO EOL <block>*/
bool parse_while(Parser *p){
    if(!parser_expect_keyword(p, KW_WHILE, "'while'")) return false;

    if(!parse_expression(p)) return false;

    if(!parser_expect_keyword(p, KW_DO, "'do'")) return false;

    if(!parser_expect(p, TOKEN_EOL, "end of line")) return false;

    if(!parse_block(p))return false;
    return true;
}

/* 27. <if_stmt> -> IF expr THEN EOL <block> <opt_else> PREDICT: { IF } */ 
bool parse_if(Parser *p){
    if(!parser_expect_keyword(p, KW_IF, "'if'")) return false;

    if(!parse_expression(p)) return false;

    if(!parser_expect_keyword(p, KW_THEN, "'then'")) return false;

    if(!parser_expect(p, TOKEN_EOL, "end of line")) return false;

    if(!parse_block(p)) return false;

    if(!parse_opt_else(p)) return false;

    return true;
}

/*
    28. <opt_else> -> ELSE <else_tail>  PREDICT: { ELSE }
    29. <opt_else> -> epsilon           PREDICT: { VAR, VAL, WHILE, IF, RETURN, ID, INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN, MINUS, DEDENT }
*/
bool parse_opt_else(Parser *p){
    if(check_keyword(p, KW_ELSE)){
        advance(p);
        return parse_else_tail(p);
    }

    return true;
}
/*
    30. <else_tail> -> EOL <block> PREDICT: { EOL }
    31. <else_tail> -> <if_stmt>   PREDICT: { IF } 
*/
bool parse_else_tail(Parser *p){
    
    if(check(p, TOKEN_EOL)){
        advance(p);
        if(!parse_block(p)) return false;
        return true;
    }

    if(check_keyword(p, KW_IF)){
        
        return parse_if(p);
    }

    return syntax_error(p, "end of line or 'if'");
}

/* 
    32. <statement> -> RETURN <opt_expr> <stmt_end>   PREDICT: { RETURN }
   33. <opt_expr>   -> expr     PREDICT: { ID, INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN, MINUS }
   34. <opt_expr>   -> ε        PREDICT: { EOL, SEMICOLON } 
*/
bool parse_return(Parser *p) {
    if(!parser_expect_keyword(p, KW_RETURN, "'return'")) return false;

    if(check(p, TOKEN_IDENTIFIER) || is_expr_start(p)) {
        if(!parse_expression(p)) return false;
    }

    return parse_stmt_end(p);
}

/* 
    35. <statement> -> ID <id_tail> <stmt_end>   PREDICT: { ID }
    36. <id_tail>   -> ASSIGN expr                PREDICT: { ASSIGN }
    37. <id_tail>   -> <expr_rest>                PREDICT: { LEFT_PAREN, DOT, operators, EOL, SEMICOLON } 
*/
bool parse_id_statement(Parser *p){
    if(!check(p, TOKEN_IDENTIFIER)) return syntax_error(p, "identifier");
    position pos = {p->current.line, p->current.column};
    char *name = take_string(p);
    advance(p);

    if(check_operator(p, OP_ASSIGN)) { 
        advance(p);
        if(!parse_expression(p)) {
            free(name);
            return false;
        }
        free(name);   // TODO: put into assign AST node
        return parse_stmt_end(p);
    }

    /* 37. call / member / expression starting with ID (takes ownership of name) */
    if(!parse_expression_rest(p, name, pos)) return false;
    return parse_stmt_end(p);
}

/* 
    39. <function_body> -> EOL <block>   PREDICT: { EOL }
    40. <function_body> -> expr EOL      PREDICT: { ID, INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN, MINUS } 
*/
bool parse_function_body(Parser *p){
    if(check(p, TOKEN_EOL)) {
        advance(p);
        return parse_block(p);
    }
    if(check(p, TOKEN_IDENTIFIER) || is_expr_start(p)) {
        if(!parse_expression(p)) return false;
        if(check(p, TOKEN_EOF)) return true;   /* last line of file without LF */
        return parser_expect(p, TOKEN_EOL, "end of line");
    }
    return syntax_error(p, "end of line or expression");
}