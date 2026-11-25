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
    p->prev_type = p->current.type;
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

/* AI-generated (Claude, Anthropic). Prompt (summary): "Write a helper that returns true if the current token is an identifier with the given name; used for 'to', 'until' and 'by' in the for loop, which the scanner returns as identifiers." */
/* Returns true if the current token is the identifier with the given name
   (to, until, by are identifiers, not keywords). */
bool check_id(Parser *p, const char *name){
    return check(p, TOKEN_IDENTIFIER) && p->current.string != NULL &&
           strcmp(p->current.string, name) == 0;
}


bool is_expr_start(Parser *p){
    return is_stmt_expr_start(p) || check_operator(p, OP_MINUS) || check_operator(p, OP_NOT) ||
           check_keyword(p, KW_IF);
}


bool is_stmt_expr_start(Parser *p){
    return check(p, TOKEN_INT_LITERAL) || check(p, TOKEN_DOUBLE_LITERAL) ||
           check(p, TOKEN_STRING_LITERAL) || check_keyword(p, KW_TRUE) ||
           check_keyword(p, KW_FALSE) || check(p, TOKEN_LEFT_PAREN) ||
           check(p, TOKEN_ISTRING_BEGIN);
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
    p.prev_type = TOKEN_EOF;

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

/* 3. <definitions> -> <function> <definitions>    PREDICT: { DEF, AT_MAIN }
      <definitions> -> <extension> <definitions>   PREDICT: { EXTENSION }   (EXTKW)
   4. <definitions> -> ε                           PREDICT: { EOF } */
bool parse_definitions(Parser *p, AstProgram *program) {
    if(check_keyword(p, KW_DEF) || check(p, TOKEN_AT_MAIN)){
        AstFunction *func = parse_function(p, true);
        if(func == NULL) return false;

        AstDefinition def = {.kind = AST_DEF_FUNCTION, .data.function = func};
        ast_definition_push(program, def);
        return parse_definitions(p, program);
    }
    if(check_keyword(p, KW_EXTENSION)){
        AstExtension *ext = parse_extension(p);
        if(ext == NULL) return false;

        AstDefinition def = {.kind = AST_DEF_EXTENSION, .data.extension = ext};
        ast_definition_push(program, def);
        return parse_definitions(p, program);
    }
    if(check(p, TOKEN_EOF)) {
        return true;
    }
    return syntax_error(p, "'def', 'extension' or end of file");
}

/* 5. <function>    -> <opt_main> DEF ID <param_lists> <opt_type> ASSIGN <function_body>
      <param_lists> -> LEFT_PAREN <parameters> RIGHT_PAREN <param_lists>   PREDICT: { LEFT_PAREN }
      <param_lists> -> ε                                                  PREDICT: { COLON, ASSIGN }
*/
AstFunction *parse_function(Parser *p, bool allow_main){
    position pos = current_pos(p);

    bool is_main = false;
    if(allow_main) {
        if(!parse_opt_main(p, &is_main)) return NULL;
    }

    if(!parser_expect_keyword(p, KW_DEF, "'def'")) return NULL;

    if(!check(p, TOKEN_IDENTIFIER)) {
        syntax_error(p, "function name");
        return NULL;
    }
    char *name = take_string(p);
    advance(p);

    AstParameterLists params = {.items = NULL, .count = 0, .capacity = 0};
    AstType *ret_type = NULL;
    bool ok = true;

    while(ok && check(p, TOKEN_LEFT_PAREN)) {
        AstParameterList *list = ast_parameter_lists_push(&params);
        advance(p);
        ok = parse_parameters(p, list) && parser_expect(p, TOKEN_RIGHT_PAREN, "')'");
    }

    if(!ok || !parse_opt_type(p, &ret_type) ||
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

/* AI-generated (Claude, Anthropic). Prompt (summary): "Write parse_extension for the EXTKW extension: 
parse 'extension (name: Type)' followed by an indented block of one or more method definitions, 
and build an AstExtension with the receiver and the list of methods." */
/* EXTKW:
   <extension>    -> EXTENSION LEFT_PAREN <parameter> RIGHT_PAREN EOL INDENT <function> <methods_next> DEDENT
   <methods_next> -> <function> <methods_next>   PREDICT: { DEF }
   <methods_next> -> ε                           PREDICT: { DEDENT } */
AstExtension *parse_extension(Parser *p){
    position pos = current_pos(p);
    if(!parser_expect_keyword(p, KW_EXTENSION, "'extension'")) return NULL;
    if(!parser_expect(p, TOKEN_LEFT_PAREN, "'('")) return NULL;

    AstParameterList receiver = {.items = NULL, .count = 0, .capacity = 0};
    if(!parse_parameter(p, &receiver) ||
       !parser_expect(p, TOKEN_RIGHT_PAREN, "')'") ||
       !parser_expect(p, TOKEN_EOL, "end of line") ||
       !parser_expect(p, TOKEN_INDENT, "indented block of methods")) {
        ast_parameter_list_free(&receiver);
        return NULL;
    }

    AstExtension *ext = ast_alloc(sizeof(AstExtension));
    ext->pos = pos;
    ext->receiver = receiver.items[0];    
    free(receiver.items);

    do {
        if(!check_keyword(p, KW_DEF)) {
            syntax_error(p, "'def'");
            ast_free_extension(ext);
            return NULL;
        }
        AstFunction *method = parse_function(p, false);
        if(method == NULL) {
            ast_free_extension(ext);
            return NULL;
        }
        AstFunction **grown = realloc(ext->methods, (ext->method_count + 1) * sizeof(AstFunction *));
        if(grown == NULL) exit(99);
        ext->methods = grown;
        ext->methods[ext->method_count++] = method;
    } while(!check(p, TOKEN_DEDENT));

    advance(p);   
    return ext;
}

/* 5.1 <opt_main> -> AT_MAIN <opt_eol>   PREDICT: { AT_MAIN }
   5.2 <opt_main> -> epsilon             PREDICT: { DEF }
   5.3 <opt_eol>  -> EOL                 PREDICT: { EOL }
   5.4 <opt_eol>  -> epsilon             PREDICT: { DEF } */
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
   14. <parameters> -> epsilon                         PREDICT: { RIGHT_PAREN } */
bool parse_parameters(Parser *p, AstParameterList *list){
    if(check(p, TOKEN_IDENTIFIER)) {
        if(!parse_parameter(p, list)) return false;
        return parse_parameters_next(p, list);
    }
    if(check(p, TOKEN_RIGHT_PAREN)) return true;

    return syntax_error(p, "parameter or ')'");
}

/* 15. <parameters_next>   -> COMMA <param_after_comma>      PREDICT: { COMMA }
   16. <parameters_next>   -> epsilon                        PREDICT: { RIGHT_PAREN }
   43. <param_after_comma> -> <parameter> <parameters_next>  PREDICT: { ID }
   44. <param_after_comma> -> epsilon                        PREDICT: { RIGHT_PAREN } */
bool parse_parameters_next(Parser *p, AstParameterList *list){
    if(check(p, TOKEN_COMMA)) {
        advance(p);
        if(check(p, TOKEN_RIGHT_PAREN)) {              
            if(!p->current.newline_before) return syntax_error(p, "parameter (trailing ',' needs a line break)");
            return true;
        }
        if(!parse_parameter(p, list)) return false;    
        return parse_parameters_next(p, list);
    }
    if(check(p, TOKEN_RIGHT_PAREN)) return true;       
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
   7. <opt_type> -> epsilon        PREDICT: { ASSIGN } */
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
    }else if (check(p, TOKEN_LEFT_PAREN)) {
        return parse_function_type(p);            
    }else {
        syntax_error(p, "type");
        return NULL;
    }
    advance(p);
    return ast_type_create(kind);
}

/* AI-generated (Claude, Anthropic). Prompt (summary): "Write parse_function_type for the LAMBDAS extension: parse a function type '(T1, T2, ...) => R' (also '() => R') and return it with ast_type_create_function." */
/* LAMBDAS:
   <type>       -> LEFT_PAREN <type_list> RIGHT_PAREN BIG_ARR <type>   PREDICT: { LEFT_PAREN }
   <type_list>  -> <type> <type_next>   |  ε
   <type_next>  -> COMMA <type> <type_next>  |  ε */
AstType *parse_function_type(Parser *p){
    if(!parser_expect(p, TOKEN_LEFT_PAREN, "'('")) return NULL;

    AstType **params = NULL;
    size_t count = 0;
    bool ok = true;

    if(!check(p, TOKEN_RIGHT_PAREN)) {
        while(true) {
            AstType *t = parse_type(p);
            if(t == NULL) { ok = false; break; }
            AstType **grown = realloc(params, (count + 1) * sizeof(AstType *));
            if(grown == NULL) exit(99);
            params = grown;
            params[count++] = t;
            if(!check(p, TOKEN_COMMA)) break;
            advance(p);
        }
    }

    AstType *result = NULL;
    if(ok && parser_expect(p, TOKEN_RIGHT_PAREN, "')'") &&
       parser_expect_operator(p, OP_BIG_ARR, "'=>'")) {
        result = parse_type(p);
    }
    if(result == NULL) {
        for(size_t i = 0; i < count; i++) ast_type_free(params[i]);
        free(params);
        return NULL;
    }
    return ast_type_create_function(params, count, result);
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

        if(!check(p, TOKEN_EOF) && p->prev_type != TOKEN_DEDENT &&
           !parser_expect(p, TOKEN_EOL, "end of line")) {
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
       PREDICT: { VAR, VAL, WHILE, IF, FOR, RETURN, ID, INT_LIT, DOUBLE_LIT, STRING_LIT, TRUE, FALSE, LEFT_PAREN, ISTRING_BEGIN }
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
   25. <statement> -> <if_stmt> <stmt_end>          PREDICT: { IF }
       <statement> -> <for_stmt> <stmt_end>         PREDICT: { FOR }        
   32. <statement> -> RETURN <opt_expr> <stmt_end>  PREDICT: { RETURN }
   35. <statement> -> ID <id_tail> <stmt_end>       PREDICT: { ID }
   38. <statement> -> expr <stmt_end>               PREDICT: { INT_LIT, DOUBLE_LIT, STRING_LIT, TRUE, FALSE, LEFT_PAREN, ISTRING_BEGIN }
   <stmt_end> is skipped when the statement ended with a block (last token DEDENT).
   The new statement is pushed into list (so it is freed with the block on error). */
bool parse_statement(Parser *p, AstStatementList *list){
    position pos = current_pos(p);
    AstStatement *stmt = NULL;

    if(check_keyword(p, KW_VAR) || check_keyword(p, KW_VAL)) {
        stmt = parse_var_def(p);
    } else if(check_keyword(p, KW_WHILE)) {
        stmt = parse_while(p);
    } else if(check_keyword(p, KW_IF)) {
        AstExpression *if_expr = parse_if(p, false);
        if(if_expr != NULL) stmt = ast_stmt_expression(if_expr, pos);
    } else if(check_keyword(p, KW_FOR)) {
        stmt = parse_for(p);
    } else if(check_keyword(p, KW_RETURN)) {
        stmt = parse_return(p);
    } else if(check(p, TOKEN_IDENTIFIER)) {
        stmt = parse_id_statement(p);
    } else if(is_stmt_expr_start(p)) {
        AstExpression *expr = parse_expression(p);
        if(expr != NULL) stmt = ast_stmt_expression(expr, pos);
    } else {
        return syntax_error(p, "statement");
    }

    if(stmt == NULL) return false;
    ast_stmt_list_push(list, stmt);
    return parse_stmt_end(p);
}

/* 
    41. <stmt_end> -> EOL         PREDICT: { EOL }
    42. <stmt_end> -> SEMICOLON   PREDICT: { SEMICOLON }
*/
bool parse_stmt_end(Parser *p){
    if(p->prev_type == TOKEN_DEDENT) {
        return true;
    }
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

/* AI-generated (Claude, Anthropic). Prompt (summary): "Write parse_rhs: parse the right side of '=' in a variable definition or assignment, either an expression on the same line, or EOL INDENT expression DEDENT when the value is on the next, more indented line." */
/* Right side of '=' in var_def and assignment:
   <rhs> -> expr                               PREDICT: { expression start }
   <rhs> -> EOL INDENT expr <stmt_end> DEDENT  PREDICT: { EOL }   (value on the next, more indented line) */
AstExpression *parse_rhs(Parser *p){
    if(!check(p, TOKEN_EOL)) {
        return parse_expression(p);
    }
    advance(p);
    if(!parser_expect(p, TOKEN_INDENT, "indented expression")) return NULL;

    AstExpression *expr = parse_expression(p);
    if(expr == NULL) return NULL;

    if(!parse_stmt_end(p) || !parser_expect(p, TOKEN_DEDENT, "end of block")) {
        ast_free_expression(expr);
        return NULL;
    }
    return expr;
}

/* 22. <var_def> -> VAR ID <opt_type> ASSIGN <rhs>   PREDICT: { VAR }
   23. <var_def> -> VAL ID <opt_type> ASSIGN <rhs>   PREDICT: { VAL } */
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

    AstExpression *init = parse_rhs(p);
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

/* 27. <if_stmt> -> IF expr THEN <branch_body> <opt_else>   PREDICT: { IF } */
AstExpression *parse_if(Parser *p, bool require_else){
    position pos = current_pos(p);
    if(!parser_expect_keyword(p, KW_IF, "'if'")) return NULL;

    AstExpression *cond = parse_expression(p);
    if(cond == NULL) return NULL;

    if(!parser_expect_keyword(p, KW_THEN, "'then'")) {
        ast_free_expression(cond);
        return NULL;
    }

    AstBLOCK *then_block = parse_branch_body(p);
    if(then_block == NULL) {
        ast_free_expression(cond);
        return NULL;
    }

    AstBLOCK *else_block = NULL;
    if(!parse_opt_else(p, &else_block, require_else)) {
        ast_free_expression(cond);
        ast_free_block(then_block);
        return NULL;
    }
    return ast_expr_conditional(cond, then_block, else_block, pos);
}

/* 28. <opt_else> -> ELSE <else_tail>    PREDICT: { ELSE }
   29. <opt_else> -> epsilon             PREDICT: { FOLLOW(if) }
   *out is NULL when there is no else branch. */
bool parse_opt_else(Parser *p, AstBLOCK **out, bool require_else){
    *out = NULL;
    if(check_keyword(p, KW_ELSE)){
        advance(p);
        *out = parse_else_tail(p, require_else);
        return *out != NULL;
    }
    if(require_else) return syntax_error(p, "'else' (if used as an expression)");
    return true;
}

/* 
    30. <else_tail> -> <branch_body>   PREDICT: { EOL, expression start }
    31. <else_tail> -> <if_stmt>       PREDICT: { IF }
*/
AstBLOCK *parse_else_tail(Parser *p, bool require_else){
    if(check_keyword(p, KW_IF)){
        position pos = current_pos(p);
        AstExpression *nested = parse_if(p, require_else);
        if(nested == NULL) return NULL;

        AstBLOCK *block = ast_block_create(pos);
        ast_stmt_list_push(&block->statements, ast_stmt_expression(nested, pos));
        return block;
    }
    return parse_branch_body(p);
}

/* AI-generated (Claude, Anthropic). Prompt (summary): "Write parse_branch_body for the CONTROL extension: 
   the body of an if branch or a for loop is either EOL followed by an indented block, 
   or a one-line body (return, assignment/call starting with an identifier, or an expression) wrapped inti a block with one statement." */
/* Body of an if branch or a for loop (CONTROL):
   <branch_body> -> EOL <block>          PREDICT: { EOL }
   <branch_body> -> <simple_statement>   PREDICT: { RETURN, ID, expression start }   (one-line body)
   A one-line body becomes a block with one statement. */
AstBLOCK *parse_branch_body(Parser *p){
    if(check(p, TOKEN_EOL)) {
        advance(p);
        return parse_block(p);
    }

    position pos = current_pos(p);
    AstStatement *stmt = NULL;
    if(check_keyword(p, KW_RETURN)) {
        stmt = parse_return(p);
    } else if(check(p, TOKEN_IDENTIFIER)) {
        stmt = parse_id_statement(p);
    } else if(is_expr_start(p)) {
        AstExpression *expr = parse_expression(p);
        if(expr != NULL) stmt = ast_stmt_expression(expr, pos);
    } else {
        syntax_error(p, "end of line or one-line body");
        return NULL;
    }
    if(stmt == NULL) return NULL;

    AstBLOCK *block = ast_block_create(pos);
    ast_stmt_list_push(&block->statements, stmt);
    return block;
}

/* AI-generated (Claude, Anthropic). Prompt (summary): "Write parse_for for the CONTROL extension: 
parse 'for i <- from to|until to [by step] {if guard} do body', with a one or a multi-line indented header, 
and build an AST for statement with the guards." */
/* CONTROL:
62. <for_stmt>          -> FOR <for_header> DO <branch_body>
63. <for_header>        -> <generator> <guards>                         PREDICT: { ID }
64. <for_header>        -> EOL INDENT <generator> EOL <guard_lines> DEDENT       PREDICT: { EOL }
65. <generator>         -> ID LEFT_ARR expr <range>
66. <range>             -> TO expr <opt_by>                             PREDICT: { TO }
67. <range>             -> UNTIL expr                                   PREDICT: { UNTIL }
68. <opt_by>            -> BY expr                                      PREDICT: { BY }
69. <opt_by>            -> ε                                            PREDICT: { IF, DO, EOL }
70. <guards>            -> IF expr <guards>                             PREDICT: { IF }
71. <guards>            -> ε                                            PREDICT: { DO }
72. <guard_lines>       -> IF expr EOL <guard_lines>                    PREDICT: { IF }
73. <guard_lines>       -> ε                                            PREDICT: { DEDENT }
*/
AstStatement *parse_for(Parser *p){
    position pos = current_pos(p);
    if(!parser_expect_keyword(p, KW_FOR, "'for'")) return NULL;

    bool multiline = false;
    if(check(p, TOKEN_EOL)) {
        advance(p);
        if(!parser_expect(p, TOKEN_INDENT, "indented generator")) return NULL;
        multiline = true;
    }

    if(!check(p, TOKEN_IDENTIFIER)) {
        syntax_error(p, "loop variable");
        return NULL;
    }
    position var_pos = current_pos(p);
    char *var_name = take_string(p);
    advance(p);

    AstExpression *from = NULL, *to = NULL, *step = NULL;
    AstExpressionList guards = {.items = NULL, .count = 0, .capacity = 0};
    AstBLOCK *body = NULL;
    bool inclusive = true;
    bool ok = parser_expect_operator(p, OP_LEFT_ARR, "'<-'");

    if(ok) ok = (from = parse_expression(p)) != NULL;
    if(ok) {
        if(check_id(p, "to")) {
            inclusive = true;
        } else if(check_id(p, "until")) {
            inclusive = false;
        } else {
            ok = syntax_error(p, "'to' or 'until'");
        }
    }
    if(ok) {
        advance(p);
        ok = (to = parse_expression(p)) != NULL;
    }
    if(ok && inclusive && check_id(p, "by")) {
        advance(p);
        ok = (step = parse_expression(p)) != NULL;
    }
    if(ok && multiline) ok = parser_expect(p, TOKEN_EOL, "end of line");

    while(ok && check_keyword(p, KW_IF)) {
        advance(p);
        AstExpression *guard = parse_expression(p);
        if(guard == NULL) { ok = false; break; }
        ast_expr_list_push(&guards, guard);
        if(multiline) ok = parser_expect(p, TOKEN_EOL, "end of line");
    }
    if(ok && multiline) ok = parser_expect(p, TOKEN_DEDENT, "end of the for header");
    if(ok) ok = parser_expect_keyword(p, KW_DO, "'do'");
    if(ok) ok = (body = parse_branch_body(p)) != NULL;

    if(!ok) {
        free(var_name);
        ast_free_expression(from);
        ast_free_expression(to);
        ast_free_expression(step);
        for(size_t i = 0; i < guards.count; i++) ast_free_expression(guards.items[i]);
        free(guards.items);
        return NULL;
    }

    AstStatement *stmt = ast_stmt_for(var_name, var_pos, from, to, inclusive, step, body, pos);
    stmt->data.for_loop.guards = guards;
    return stmt;
}

/* 32. <statement> -> RETURN <opt_expr> <stmt_end>   (stmt_end is read by parse_statement)
   33. <opt_expr>  -> expr     PREDICT: { ID, expression start }
   34. <opt_expr>  -> ε        PREDICT: { EOL, SEMICOLON, ELSE, DEDENT } */
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
   36. <id_tail>   -> ASSIGN <rhs>               PREDICT: { ASSIGN }
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
        AstExpression *value = parse_rhs(p);
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
