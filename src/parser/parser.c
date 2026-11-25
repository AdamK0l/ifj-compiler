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

/* Position of the current token. */
position current_pos(Parser *p){
    position pos = {(size_t)p->current.line, (size_t)p->current.column};
    return pos;
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

/* Tokens that can start an expression (without ID).
   { INT_LIT, DOUBLE_LIT, STRING_LIT, TRUE, FALSE, LEFT_PAREN, MINUS, NOT } */
bool is_expr_start(Parser *p){
    return is_stmt_expr_start(p) || check_operator(p, OP_MINUS) || check_operator(p, OP_NOT);
}

/* Tokens that can start an expression statement (without ID).
   A logical line cannot start with an operator, so no MINUS / NOT here.
   { INT_LIT, DOUBLE_LIT, STRING_LIT, TRUE, FALSE, LEFT_PAREN } */
bool is_stmt_expr_start(Parser *p){
    return check(p, TOKEN_INT_LITERAL) || check(p, TOKEN_DOUBLE_LITERAL) ||
           check(p, TOKEN_STRING_LITERAL) || check_keyword(p, KW_TRUE) ||
           check_keyword(p, KW_FALSE) || check(p, TOKEN_LEFT_PAREN);
}

/* ==================== Error handling ==================== */

/* Reports the first syntax error and sets ERR_SYNTAX. */
bool syntax_error(Parser *p, const char *expected){
    if(p->error == ERR_OK){
        position pos = current_pos(p);
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

/* ==================== Entry point ==================== */

ErrorCode parser_run(Scanner *scanner, AstProgram **out) {
    *out = NULL;

    Parser p;
    memset(&p, 0, sizeof(p));
    p.scanner = scanner;
    p.error = ERR_OK;

    AstProgram *program = ast_alloc(sizeof(AstProgram));

    advance(&p);
    bool ok = parse_program(&p, program);
    token_free(&p.current);

    if(!ok || p.error != ERR_OK) {
        ast_free_program(program);
        free(program);
        return (p.error != ERR_OK) ? p.error : ERR_SYNTAX;
    }
    *out = program;
    return ERR_OK;
}

/* ==================== Program and definitions ==================== */

/* 1. <program> -> <prolog> <definitions> EOF */
bool parse_program(Parser *p, AstProgram *program){
    if(!parse_prolog(p)) return false;

    if(!parse_definitions(p, program)) return false;

    if(!parser_expect(p, TOKEN_EOF, "end of file")) return false;
    return true;
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

/* 3. <definitions> -> <function> <definitions>   PREDICT: { DEF, AT_MAIN }
   4. <definitions> -> ε                          PREDICT: { EOF } */
bool parse_definitions(Parser *p, AstProgram *program) {
    if(check_keyword(p, KW_DEF) || check(p, TOKEN_AT_MAIN)){
        AstFunction *func = parse_function(p);
        if(func == NULL) return false;

        AstDefinition def = {.kind = AST_DEF_FUNCTION, .data.function = func};
        ast_definition_push(program, def);
        return parse_definitions(p, program);
    }
    if(check(p, TOKEN_EOF)) {
        return true;
    }
    return syntax_error(p, "'def' or end of file");
}

/* 5. <function> -> <opt_main> DEF ID LEFT_PAREN <parameters> RIGHT_PAREN <opt_type> ASSIGN <function_body> */
AstFunction *parse_function(Parser *p){
    position pos = current_pos(p);

    bool is_main = false;
    if(!parse_opt_main(p, &is_main)) return NULL;

    if(!parser_expect_keyword(p, KW_DEF, "'def'")) return NULL;

    if(!check(p, TOKEN_IDENTIFIER)) {
        syntax_error(p, "function name");
        return NULL;
    }
    char *name = take_string(p);
    advance(p);

    AstParameterLists params = {.items = NULL, .count = 0, .capacity = 0};
    AstParameterList *list = ast_parameter_lists_push(&params);
    AstType *ret_type = NULL;

    if(!parser_expect(p, TOKEN_LEFT_PAREN, "'('") ||
       !parse_parameters(p, list) ||
       !parser_expect(p, TOKEN_RIGHT_PAREN, "')'") ||
       !parse_opt_type(p, &ret_type) ||
       !parser_expect_operator(p, OP_ASSIGN, "'='")) {
        free(name);
        ast_parameter_lists_free(&params);
        ast_type_free(ret_type);
        return NULL;
    }

    AstBLOCK *body = parse_function_body(p);
    if(body == NULL) {
        free(name);
        ast_parameter_lists_free(&params);
        ast_type_free(ret_type);
        return NULL;
    }

    AstFunction *func = ast_function_create(is_main, name, ret_type, body, pos);
    func->parameters = params;
    return func;
}

/* 5.1 <opt_main> -> AT_MAIN <opt_eol>   PREDICT: { AT_MAIN }
   5.2 <opt_main> -> ε                   PREDICT: { DEF }
   5.3 <opt_eol>  -> EOL                 PREDICT: { EOL }
   5.4 <opt_eol>  -> ε                   PREDICT: { DEF } */
bool parse_opt_main(Parser *p, bool *is_main){
    *is_main = false;
    if(check(p, TOKEN_AT_MAIN)) {
        advance(p);
        if(check(p, TOKEN_EOL)) {
            advance(p);
        }
        *is_main = true;
        return true;
    }
    if (check_keyword(p, KW_DEF)) {
        return true;
    }
    return syntax_error(p, "'@main' or 'def'");
}

/* 13. <parameters> -> <parameter> <parameters_next>   PREDICT: { ID }
   14. <parameters> -> ε                               PREDICT: { RIGHT_PAREN } */
bool parse_parameters(Parser *p, AstParameterList *list){
    if(check(p, TOKEN_IDENTIFIER)) {
        if(!parse_parameter(p, list)) return false;
        return parse_parameters_next(p, list);
    }
    if(check(p, TOKEN_RIGHT_PAREN)) return true;

    return syntax_error(p, "parameter or ')'");
}

/* 15. <parameters_next>   -> COMMA <param_after_comma>      PREDICT: { COMMA }
   16. <parameters_next>   -> ε                              PREDICT: { RIGHT_PAREN }
   43. <param_after_comma> -> <parameter> <parameters_next>  PREDICT: { ID }
   44. <param_after_comma> -> ε                              PREDICT: { RIGHT_PAREN } */
bool parse_parameters_next(Parser *p, AstParameterList *list){
    if(check(p, TOKEN_COMMA)) {
        advance(p);
        if(check(p, TOKEN_RIGHT_PAREN)) return true;   /* 44. trailing comma */
        if(!parse_parameter(p, list)) return false;    /* 43. */
        return parse_parameters_next(p, list);
    }
    if(check(p, TOKEN_RIGHT_PAREN)) return true;       /* 16. */
    return syntax_error(p, "',' or ')'");
}

/* 17. <parameter> -> ID COLON <type> */
bool parse_parameter(Parser *p, AstParameterList *list){
    if(!check(p, TOKEN_IDENTIFIER)) return syntax_error(p, "parameter name");
    position pos = current_pos(p);
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

/* 6. <opt_type> -> COLON <type>   PREDICT: { COLON }
   7. <opt_type> -> ε              PREDICT: { ASSIGN } */
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

/* 8.  <type> -> INT       PREDICT: { INT }
   9.  <type> -> DOUBLE    PREDICT: { DOUBLE }
   10. <type> -> STRING    PREDICT: { STRING }
   11. <type> -> UNIT      PREDICT: { UNIT }
   12. <type> -> BOOLEAN   PREDICT: { BOOLEAN } */
AstType *parse_type(Parser *p){
    AstTypeKind kind;

    if(check_keyword(p, KW_INT)) {
        kind = AST_TYPE_INT;
    }else if (check_keyword(p, KW_DOUBLE)){
        kind = AST_TYPE_DOUBLE;
    }else if (check_keyword(p, KW_STRING)){
        kind = AST_TYPE_STRING;
    }else if (check_keyword(p, KW_BOOLEAN)){
        kind = AST_TYPE_BOOLEAN;
    }else if (check_keyword(p, KW_UNIT)) {
        kind = AST_TYPE_UNIT;
    }else {
        syntax_error(p, "type");
        return NULL;
    }
    advance(p);
    return ast_type_create(kind);
}

/* 39. <function_body> -> EOL <block>   PREDICT: { EOL }
   40. <function_body> -> expr EOL      PREDICT: { ID, INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN, MINUS }
   A one-line body becomes a block with one expression statement. */
AstBLOCK *parse_function_body(Parser *p){
    if(check(p, TOKEN_EOL)) {
        advance(p);
        return parse_block(p);
    }
    if(check(p, TOKEN_IDENTIFIER) || is_expr_start(p)) {
        position pos = current_pos(p);
        AstExpression *expr = parse_expression(p);
        if(expr == NULL) return NULL;

        if(!check(p, TOKEN_EOF) && !parser_expect(p, TOKEN_EOL, "end of line")) {
            ast_free_expression(expr);
            return NULL;
        }
        AstBLOCK *block = ast_block_create(pos);
        ast_stmt_list_push(&block->statements, ast_stmt_expression(expr, pos));
        return block;
    }
    syntax_error(p, "end of line or expression");
    return NULL;
}

/* ==================== Blocks and statements ==================== */

/* 18. <block> -> INDENT <statement> <statement_next> DEDENT */
AstBLOCK *parse_block(Parser *p){
    position pos = current_pos(p);
    if(!parser_expect(p, TOKEN_INDENT, "indented block")) return NULL;

    AstBLOCK *block = ast_block_create(pos);

    if(!parse_statement(p, &block->statements) ||
       !parse_statement_next(p, &block->statements) ||
       !parser_expect(p, TOKEN_DEDENT, "end of block")) {
        ast_free_block(block);
        return NULL;
    }
    return block;
}

/* 19. <statement_next> -> <statement> <statement_next>
       PREDICT: { VAR, VAL, WHILE, IF, RETURN, ID, INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN }
   20. <statement_next> -> ε   PREDICT: { DEDENT } */
bool parse_statement_next(Parser *p, AstStatementList *list){
    if(check(p, TOKEN_DEDENT)) {
        return true;
    }
    if(!parse_statement(p, list)) return false;
    return parse_statement_next(p, list);
}

/* 21. <statement> -> <var_def> <stmt_end>          PREDICT: { VAR, VAL }
   24. <statement> -> <while_stmt>                  PREDICT: { WHILE }
   25. <statement> -> <if_stmt>                     PREDICT: { IF }
   32. <statement> -> RETURN <opt_expr> <stmt_end>  PREDICT: { RETURN }
   35. <statement> -> ID <id_tail> <stmt_end>       PREDICT: { ID }
   38. <statement> -> expr <stmt_end>               PREDICT: { INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN }
   The new statement is pushed into list (so it is freed with the block on error). */
bool parse_statement(Parser *p, AstStatementList *list){
    position pos = current_pos(p);
    AstStatement *stmt = NULL;

    if(check_keyword(p, KW_VAR) || check_keyword(p, KW_VAL)) {
        stmt = parse_var_def(p);
        if(stmt == NULL) return false;
        ast_stmt_list_push(list, stmt);
        return parse_stmt_end(p);
    }
    if(check_keyword(p, KW_WHILE)) {
        stmt = parse_while(p);
        if(stmt == NULL) return false;
        ast_stmt_list_push(list, stmt);
        return true;
    }
    if(check_keyword(p, KW_IF)) {
        AstExpression *if_expr = parse_if(p);
        if(if_expr == NULL) return false;
        ast_stmt_list_push(list, ast_stmt_expression(if_expr, pos));
        return true;
    }
    if(check_keyword(p, KW_RETURN)) {
        stmt = parse_return(p);
        if(stmt == NULL) return false;
        ast_stmt_list_push(list, stmt);
        return parse_stmt_end(p);
    }
    if(check(p, TOKEN_IDENTIFIER)) {
        stmt = parse_id_statement(p);
        if(stmt == NULL) return false;
        ast_stmt_list_push(list, stmt);
        return parse_stmt_end(p);
    }
    if(is_stmt_expr_start(p)) {
        AstExpression *expr = parse_expression(p);
        if(expr == NULL) return false;
        ast_stmt_list_push(list, ast_stmt_expression(expr, pos));
        return parse_stmt_end(p);
    }
    return syntax_error(p, "statement");
}

/* 41. <stmt_end> -> EOL         PREDICT: { EOL }
   42. <stmt_end> -> SEMICOLON   PREDICT: { SEMICOLON }
   A ';' at the end of a line may be followed by EOL. */
bool parse_stmt_end(Parser *p){
    if(check(p, TOKEN_EOL)) {
        advance(p);
        return true;
    }
    if(check(p, TOKEN_SEMICOLON)){
        advance(p);
        if(check(p, TOKEN_EOL)) advance(p);
        return true;
    }
    return syntax_error(p, "end of line or ';'");
}

/* 22. <var_def> -> VAR ID <opt_type> ASSIGN expr   PREDICT: { VAR }
   23. <var_def> -> VAL ID <opt_type> ASSIGN expr   PREDICT: { VAL } */
AstStatement *parse_var_def(Parser *p){
    position pos = current_pos(p);
    bool is_mutable;
    if(check_keyword(p, KW_VAR)) {
        is_mutable = true;
    } else if(check_keyword(p, KW_VAL)) {
        is_mutable = false;
    } else{
        syntax_error(p, "'var' or 'val'");
        return NULL;
    }
    advance(p);

    if(!check(p, TOKEN_IDENTIFIER)) {
        syntax_error(p, "variable or value name");
        return NULL;
    }
    char *name = take_string(p);
    advance(p);

    AstType *type = NULL;
    if(!parse_opt_type(p, &type)){
        free(name);
        return NULL;
    }

    if(!parser_expect_operator(p, OP_ASSIGN, "'='")){
        free(name);
        ast_type_free(type);
        return NULL;
    }

    AstExpression *init = parse_expression(p);
    if(init == NULL){
        free(name);
        ast_type_free(type);
        return NULL;
    }

    return ast_stmt_variable(name, is_mutable, type, init, pos);
}

/* 26. <while_stmt> -> WHILE expr DO EOL <block> */
AstStatement *parse_while(Parser *p){
    position pos = current_pos(p);
    if(!parser_expect_keyword(p, KW_WHILE, "'while'")) return NULL;

    AstExpression *cond = parse_expression(p);
    if(cond == NULL) return NULL;

    if(!parser_expect_keyword(p, KW_DO, "'do'") ||
       !parser_expect(p, TOKEN_EOL, "end of line")) {
        ast_free_expression(cond);
        return NULL;
    }

    AstBLOCK *body = parse_block(p);
    if(body == NULL) {
        ast_free_expression(cond);
        return NULL;
    }
    return ast_stmt_while(cond, body, pos);
}

/* 27. <if_stmt> -> IF expr THEN EOL <block> <opt_else>   PREDICT: { IF }
   The if statement is an AST_EXPR_IF (used as an expression statement). */
AstExpression *parse_if(Parser *p){
    position pos = current_pos(p);
    if(!parser_expect_keyword(p, KW_IF, "'if'")) return NULL;

    AstExpression *cond = parse_expression(p);
    if(cond == NULL) return NULL;

    if(!parser_expect_keyword(p, KW_THEN, "'then'") ||
       !parser_expect(p, TOKEN_EOL, "end of line")) {
        ast_free_expression(cond);
        return NULL;
    }

    AstBLOCK *then_block = parse_block(p);
    if(then_block == NULL) {
        ast_free_expression(cond);
        return NULL;
    }

    AstBLOCK *else_block = NULL;
    if(!parse_opt_else(p, &else_block)) {
        ast_free_expression(cond);
        ast_free_block(then_block);
        return NULL;
    }
    return ast_expr_conditional(cond, then_block, else_block, pos);
}

/* 28. <opt_else> -> ELSE <else_tail>   PREDICT: { ELSE }
   29. <opt_else> -> ε                  PREDICT: { VAR, VAL, WHILE, IF, RETURN, ID, INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN, DEDENT }
   *out is NULL when there is no else branch. */
bool parse_opt_else(Parser *p, AstBLOCK **out){
    *out = NULL;
    if(check_keyword(p, KW_ELSE)){
        advance(p);
        *out = parse_else_tail(p);
        return *out != NULL;
    }
    return true;
}

/* 30. <else_tail> -> EOL <block>   PREDICT: { EOL }
   31. <else_tail> -> <if_stmt>     PREDICT: { IF }
   "else if" becomes an else block with one statement: the nested if. */
AstBLOCK *parse_else_tail(Parser *p){
    if(check(p, TOKEN_EOL)){
        advance(p);
        return parse_block(p);
    }
    if(check_keyword(p, KW_IF)){
        position pos = current_pos(p);
        AstExpression *nested = parse_if(p);
        if(nested == NULL) return NULL;

        AstBLOCK *block = ast_block_create(pos);
        ast_stmt_list_push(&block->statements, ast_stmt_expression(nested, pos));
        return block;
    }
    syntax_error(p, "end of line or 'if'");
    return NULL;
}

/* 32. <statement> -> RETURN <opt_expr> <stmt_end>   (stmt_end is read by parse_statement)
   33. <opt_expr>  -> expr     PREDICT: { ID, INT_LIT, DOUBLE_LIT, STRING_LIT, LEFT_PAREN, MINUS }
   34. <opt_expr>  -> ε        PREDICT: { EOL, SEMICOLON } */
AstStatement *parse_return(Parser *p){
    position pos = current_pos(p);
    if(!parser_expect_keyword(p, KW_RETURN, "'return'")) return NULL;

    AstExpression *value = NULL;     /* NULL = return without value */
    if(check(p, TOKEN_IDENTIFIER) || is_expr_start(p)) {
        value = parse_expression(p);
        if(value == NULL) return NULL;
    }
    return ast_stmt_return(value, pos);
}

/* 35. <statement> -> ID <id_tail> <stmt_end>   (stmt_end is read by parse_statement)
   36. <id_tail>   -> ASSIGN expr                PREDICT: { ASSIGN }
   37. <id_tail>   -> <expr_rest>                PREDICT: { LEFT_PAREN, DOT, operators, EOL, SEMICOLON } */
AstStatement *parse_id_statement(Parser *p){
    position pos = current_pos(p);
    if(!check(p, TOKEN_IDENTIFIER)) {
        syntax_error(p, "identifier");
        return NULL;
    }
    char *name = take_string(p);
    advance(p);

    if(check_operator(p, OP_ASSIGN)) {          /* 36. assignment */
        advance(p);
        AstExpression *value = parse_expression(p);
        if(value == NULL) {
            free(name);
            return NULL;
        }
        return ast_stmt_assign(name, value, pos);
    }

    /* 37. call / member / expression starting with ID (takes ownership of name) */
    AstExpression *expr = parse_expression_rest(p, name, pos);
    if(expr == NULL) return NULL;
    return ast_stmt_expression(expr, pos);
}
