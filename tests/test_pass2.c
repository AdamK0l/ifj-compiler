/**
 * Project: IFJ26 / IAL compiler
 * File: test_pass2.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "ast.h"
#include "errors.h"
#include "memory.h"
#include "semantics.h"
#include "type_checker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== Helper Constructors ==================== */

static AstExpression *create_int_literal(int32_t val) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_INT;
    e->evaluated_type = NULL;
    e->data.int_value = val;
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstExpression *create_string_literal(const char *str) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_STRING;
    e->evaluated_type = NULL;
    e->data.string_value.data = safe_strdup(str);
    e->data.string_value.length = strlen(str);
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstExpression *create_bool_literal(bool val) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_BOOLEAN;
    e->evaluated_type = NULL;
    e->data.boolean_value = val;
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstExpression *create_var_expr(const char *name) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_NAME;
    e->evaluated_type = NULL;
    e->data.name = safe_strdup(name);
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstExpression *create_binary_expr(AstBinaryOp op, AstExpression *l, AstExpression *r) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_BINARY;
    e->evaluated_type = NULL;
    e->data.binary.op = op;
    e->data.binary.left = l;
    e->data.binary.right = r;
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstBLOCK *create_empty_block(void) {
    AstBLOCK *b = (AstBLOCK *)safe_malloc(sizeof(AstBLOCK));
    b->pos.line = 1;
    b->pos.column = 1;
    b->statements.count = 0;
    b->statements.capacity = 0;
    b->statements.items = NULL;
    return b;
}

static void block_add_statement(AstBLOCK *block, AstStatement *stmt) {
    block->statements.count++;
    block->statements.items = (AstStatement **)safe_realloc(
        block->statements.items, sizeof(AstStatement *) * block->statements.count);
    block->statements.items[block->statements.count - 1] = stmt;
}

static AstStatement *create_var_decl(const char *name, bool is_changeable, AstType *declared_type, AstExpression *init) {
    AstStatement *s = (AstStatement *)safe_malloc(sizeof(AstStatement));
    s->kind = AST_STATEMENT_VARIABLE;
    s->pos.line = 1;
    s->pos.column = 1;
    s->data.variable.name = safe_strdup(name);
    s->data.variable.is_changeable = is_changeable;
    s->data.variable.declared_type = declared_type;
    s->data.variable.initializer = init;
    return s;
}

static AstStatement *create_assign(const char *name, AstExpression *val) {
    AstStatement *s = (AstStatement *)safe_malloc(sizeof(AstStatement));
    s->kind = AST_STATEMENT_ASSIGN;
    s->pos.line = 1;
    s->pos.column = 1;
    s->data.assignment.name = safe_strdup(name);
    s->data.assignment.value = val;
    return s;
}

static AstStatement *create_expr_stmt(AstExpression *expr) {
    AstStatement *s = (AstStatement *)safe_malloc(sizeof(AstStatement));
    s->kind = AST_STATEMENT_EXPRESSION;
    s->pos.line = 1;
    s->pos.column = 1;
    s->data.expression = expr;
    return s;
}

static AstStatement *create_while_stmt(AstExpression *cond, AstBLOCK *body) {
    AstStatement *s = (AstStatement *)safe_malloc(sizeof(AstStatement));
    s->kind = AST_STATEMENT_WHILE;
    s->pos.line = 1;
    s->pos.column = 1;
    s->data.while_loop.condition = cond;
    s->data.while_loop.inside = body;
    return s;
}

static AstStatement *create_return_stmt(AstExpression *ret_val) {
    AstStatement *s = (AstStatement *)safe_malloc(sizeof(AstStatement));
    s->kind = AST_STATEMENT_RETURN;
    s->pos.line = 1;
    s->pos.column = 1;
    s->data.return_value = ret_val;
    return s;
}

static AstFunction *create_func(const char *name, bool is_main, AstType *ret_type, AstBLOCK *body) {
    AstFunction *f = (AstFunction *)safe_malloc(sizeof(AstFunction));
    f->pos.line = 1;
    f->pos.column = 1;
    f->name = safe_strdup(name);
    f->is_main = is_main;
    f->return_type = ret_type;
    f->inside = body;
    f->parameters.count = 0;
    f->parameters.capacity = 0;
    f->parameters.items = NULL;
    return f;
}

static void func_add_param(AstFunction *func, const char *param_name, AstType *type) {
    if (func->parameters.count == 0) {
        func->parameters.count = 1;
        func->parameters.capacity = 1;
        func->parameters.items = (AstParameterList *)safe_malloc(sizeof(AstParameterList));
        func->parameters.items[0].count = 0;
        func->parameters.items[0].capacity = 0;
        func->parameters.items[0].items = NULL;
    }
    AstParameterList *l = &func->parameters.items[0];
    l->count++;
    l->items = (AstParameter *)safe_realloc(l->items, sizeof(AstParameter) * l->count);
    l->items[l->count - 1].name = safe_strdup(param_name);
    l->items[l->count - 1].type = type;
    l->items[l->count - 1].pos.line = 1;
    l->items[l->count - 1].pos.column = 1;
}

/* ==================== Test Runner ==================== */

int main(void) {
    printf("=== Running Semantic Pass 2 (Statements & Bodies) Tests ===\n\n");

    // Test 1: Full Factorial Program
    {
        printf("[Test 1] Full Factorial program... ");
        AstProgram prog;
        prog.count = 2;
        prog.capacity = 2;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 2);

        // def factorial(n: Int): Int =
        //   var result = 1
        //   var i: Int = n
        //   while i > 1 do { result = result * i; i = i - 1 }
        //   result
        AstBLOCK *fact_body = create_empty_block();
        block_add_statement(fact_body, create_var_decl("result", true, NULL, create_int_literal(1)));
        block_add_statement(fact_body, create_var_decl("i", true, ast_type_create(AST_TYPE_INT), create_var_expr("n")));

        AstBLOCK *while_body = create_empty_block();
        block_add_statement(while_body, create_assign("result", create_binary_expr(AST_MULTIPLY, create_var_expr("result"), create_var_expr("i"))));
        block_add_statement(while_body, create_assign("i", create_binary_expr(AST_SUBTRACT, create_var_expr("i"), create_int_literal(1))));

        block_add_statement(fact_body, create_while_stmt(create_binary_expr(AST_GREATER, create_var_expr("i"), create_int_literal(1)), while_body));
        block_add_statement(fact_body, create_expr_stmt(create_var_expr("result")));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("factorial", false, ast_type_create(AST_TYPE_INT), fact_body);
        func_add_param(prog.items[0].data.function, "n", ast_type_create(AST_TYPE_INT));

        // @main def runFoo(): Unit =
        //   val n = 5
        //   ()
        AstBLOCK *main_body = create_empty_block();
        block_add_statement(main_body, create_var_decl("n", false, NULL, create_int_literal(5)));
        block_add_statement(main_body, create_expr_stmt(create_var_expr("n"))); // expression statement in Unit main

        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_func("runFoo", true, ast_type_create(AST_TYPE_UNIT), main_body);

        int res = analyze_semantics(&prog);
        assert(res == ERR_OK);
        ast_free_program(&prog);
        printf("PASSED (result: %d)\n", res);
    }

    // Test 2: Cannot assign to immutable variable (val) -> Error 7
    {
        printf("[Test 2] Mutability check on val (x = 2) -> Error 7... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        AstBLOCK *body = create_empty_block();
        block_add_statement(body, create_var_decl("x", false, NULL, create_int_literal(1))); // val x = 1
        block_add_statement(body, create_assign("x", create_int_literal(2)));                 // x = 2

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), body);

        int res = analyze_semantics(&prog);
        assert(res == ERR_SEM_MUTABILITY);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_MUTABILITY 7)\n", res);
    }

    // Test 3: Cannot assign to function parameter -> Error 7
    {
        printf("[Test 3] Mutability check on function param (n = 5) -> Error 7... ");
        AstProgram prog;
        prog.count = 2;
        prog.capacity = 2;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 2);

        // def foo(n: Int): Unit = { n = 5 }
        AstBLOCK *foo_body = create_empty_block();
        block_add_statement(foo_body, create_assign("n", create_int_literal(5)));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("foo", false, ast_type_create(AST_TYPE_UNIT), foo_body);
        func_add_param(prog.items[0].data.function, "n", ast_type_create(AST_TYPE_INT));

        // @main def main(): Unit = ()
        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), create_empty_block());

        int res = analyze_semantics(&prog);
        assert(res == ERR_SEM_MUTABILITY);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_MUTABILITY 7)\n", res);
    }

    // Test 4: Variable redefinition in same scope -> Error 4
    {
        printf("[Test 4] Variable redefinition in same scope (val x; val x) -> Error 4... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        AstBLOCK *body = create_empty_block();
        block_add_statement(body, create_var_decl("x", false, NULL, create_int_literal(1)));
        block_add_statement(body, create_var_decl("x", false, NULL, create_int_literal(2)));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), body);

        int res = analyze_semantics(&prog);
        assert(res == ERR_SEM_REDEFINITION);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_REDEFINITION 4)\n", res);
    }

    // Test 5: Variable shadowing in nested while block -> ALLOWED
    {
        printf("[Test 5] Variable shadowing in nested scope -> ALLOWED... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        AstBLOCK *body = create_empty_block();
        block_add_statement(body, create_var_decl("x", false, NULL, create_int_literal(1)));

        AstBLOCK *while_body = create_empty_block();
        block_add_statement(while_body, create_var_decl("x", false, NULL, create_string_literal("shadowed")));

        block_add_statement(body, create_while_stmt(create_bool_literal(true), while_body));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), body);

        int res = analyze_semantics(&prog);
        assert(res == ERR_OK);
        ast_free_program(&prog);
        printf("PASSED (result: %d)\n", res);
    }

    // Test 6: Type mismatch in variable initialization -> Error 6
    {
        printf("[Test 6] Type mismatch in val x: Int = \"str\" -> Error 6... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        AstBLOCK *body = create_empty_block();
        block_add_statement(body, create_var_decl("x", false, ast_type_create(AST_TYPE_INT), create_string_literal("str")));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), body);

        int res = analyze_semantics(&prog);
        assert(res == ERR_SEM_TYPE);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_TYPE 6)\n", res);
    }

    // Test 7: Condition of while is not Boolean -> Error 6
    {
        printf("[Test 7] Condition in while (42) -> Error 6... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        AstBLOCK *body = create_empty_block();
        block_add_statement(body, create_while_stmt(create_int_literal(42), create_empty_block()));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), body);

        int res = analyze_semantics(&prog);
        assert(res == ERR_SEM_TYPE);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_TYPE 6)\n", res);
    }

    // Test 8: Function return type mismatch in body -> Error 6
    {
        printf("[Test 8] Return type mismatch in function body -> Error 6... ");
        AstProgram prog;
        prog.count = 2;
        prog.capacity = 2;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 2);

        // def foo(): Int = "str"
        AstBLOCK *foo_body = create_empty_block();
        block_add_statement(foo_body, create_expr_stmt(create_string_literal("str")));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("foo", false, ast_type_create(AST_TYPE_INT), foo_body);

        // @main def main(): Unit = ()
        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), create_empty_block());

        int res = analyze_semantics(&prog);
        assert(res == ERR_SEM_TYPE);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_TYPE 6)\n", res);
    }

    // Test 9: Explicit return statement type mismatch -> Error 6
    {
        printf("[Test 9] Explicit return statement type mismatch -> Error 6... ");
        AstProgram prog;
        prog.count = 2;
        prog.capacity = 2;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 2);

        // def foo(): Int = { return "str" }
        AstBLOCK *foo_body = create_empty_block();
        block_add_statement(foo_body, create_return_stmt(create_string_literal("str")));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("foo", false, ast_type_create(AST_TYPE_INT), foo_body);

        // @main def main(): Unit = ()
        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), create_empty_block());

        int res = analyze_semantics(&prog);
        assert(res == ERR_SEM_TYPE);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_TYPE 6)\n", res);
    }

    printf("\n>>> ALL PASS 2 TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
