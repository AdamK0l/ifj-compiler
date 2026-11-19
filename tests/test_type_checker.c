/**
 * Project: IFJ26 / IAL compiler
 * File: test_type_checker.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "ast.h"
#include "errors.h"
#include "memory.h"
#include "semantics.h"
#include "symtable.h"
#include "type_checker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AstExpression *create_int_literal(int32_t val) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_INT;
    e->evaluated_type = NULL;
    e->data.int_value = val;
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstExpression *create_double_literal(double val) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_DOUBLE;
    e->evaluated_type = NULL;
    e->data.double_value = val;
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

static AstExpression *create_call_expr(const char *func_name, AstExpression **args, size_t arg_count) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_CALL;
    e->evaluated_type = NULL;
    e->data.call.callee = create_var_expr(func_name);

    e->data.call.arguments.count = 1;
    e->data.call.arguments.capacity = 1;
    e->data.call.arguments.items = (AstExpressionList *)safe_malloc(sizeof(AstExpressionList));
    e->data.call.arguments.items[0].count = arg_count;
    e->data.call.arguments.items[0].capacity = arg_count;
    e->data.call.arguments.items[0].items = (AstExpression **)safe_malloc(sizeof(AstExpression *) * arg_count);
    for (size_t i = 0; i < arg_count; i++) {
        e->data.call.arguments.items[0].items[i] = args[i];
    }

    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

int main(void) {
    printf("=== Running Type Checker Tests ===\n\n");

    Symtable *table = symtable_create();

    // Register builtins (print, toStr, readInt, etc.)
    AstProgram dummy_prog;
    dummy_prog.count = 1;
    dummy_prog.capacity = 1;
    dummy_prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));
    dummy_prog.items[0].kind = AST_DEF_FUNCTION;
    dummy_prog.items[0].data.function = (AstFunction *)safe_malloc(sizeof(AstFunction));
    dummy_prog.items[0].data.function->name = safe_strdup("main");
    dummy_prog.items[0].data.function->is_main = true;
    dummy_prog.items[0].data.function->return_type = ast_type_create(AST_TYPE_UNIT);
    dummy_prog.items[0].data.function->inside = NULL;
    dummy_prog.items[0].data.function->parameters.count = 0;
    dummy_prog.items[0].data.function->parameters.items = NULL;
    dummy_prog.items[0].data.function->pos.line = 1;
    dummy_prog.items[0].data.function->pos.column = 1;

    semantic_pass1(&dummy_prog, table);

    // Test 1: Literals
    {
        printf("[Test 1] Literals (Int, Double, String, Bool)... ");
        AstType *t = NULL;

        AstExpression *e_int = create_int_literal(42);
        assert(check_expression(e_int, table, &t) == ERR_OK && t->kind == AST_TYPE_INT);
        ast_free_expression(e_int);

        AstExpression *e_dbl = create_double_literal(3.14);
        assert(check_expression(e_dbl, table, &t) == ERR_OK && t->kind == AST_TYPE_DOUBLE);
        ast_free_expression(e_dbl);

        AstExpression *e_str = create_string_literal("hello");
        assert(check_expression(e_str, table, &t) == ERR_OK && t->kind == AST_TYPE_STRING);
        ast_free_expression(e_str);

        AstExpression *e_bool = create_bool_literal(true);
        assert(check_expression(e_bool, table, &t) == ERR_OK && t->kind == AST_TYPE_BOOLEAN);
        ast_free_expression(e_bool);

        printf("PASSED\n");
    }

    // Test 2: Arithmetic addition
    {
        printf("[Test 2] Arithmetic addition (Int+Int, Int+Double, Str+Str, Str+Int)... ");
        AstType *t = NULL;

        // 5 + 10 -> Int
        AstExpression *e1 = create_binary_expr(AST_ADD, create_int_literal(5), create_int_literal(10));
        assert(check_expression(e1, table, &t) == ERR_OK && t->kind == AST_TYPE_INT);
        ast_free_expression(e1);

        // 5 + 2.5 -> Double (implicit promotion)
        AstExpression *e2 = create_binary_expr(AST_ADD, create_int_literal(5), create_double_literal(2.5));
        assert(check_expression(e2, table, &t) == ERR_OK && t->kind == AST_TYPE_DOUBLE);
        ast_free_expression(e2);

        // "a" + "b" -> String
        AstExpression *e3 = create_binary_expr(AST_ADD, create_string_literal("a"), create_string_literal("b"));
        assert(check_expression(e3, table, &t) == ERR_OK && t->kind == AST_TYPE_STRING);
        ast_free_expression(e3);

        // "a" + 5 -> Error 6
        AstExpression *e4 = create_binary_expr(AST_ADD, create_string_literal("a"), create_int_literal(5));
        assert(check_expression(e4, table, &t) == ERR_SEM_TYPE);
        ast_free_expression(e4);

        printf("PASSED\n");
    }

    // Test 3: Relational comparisons
    {
        printf("[Test 3] Relational (<, ==)... ");
        AstType *t = NULL;

        // 5 < 10 -> Boolean
        AstExpression *e1 = create_binary_expr(AST_LESS, create_int_literal(5), create_int_literal(10));
        assert(check_expression(e1, table, &t) == ERR_OK && t->kind == AST_TYPE_BOOLEAN);
        ast_free_expression(e1);

        // 5.0 == 5 -> Boolean
        AstExpression *e2 = create_binary_expr(AST_EQUAL, create_double_literal(5.0), create_int_literal(5));
        assert(check_expression(e2, table, &t) == ERR_OK && t->kind == AST_TYPE_BOOLEAN);
        ast_free_expression(e2);

        // 5 == "five" -> Error 6
        AstExpression *e3 = create_binary_expr(AST_EQUAL, create_int_literal(5), create_string_literal("five"));
        assert(check_expression(e3, table, &t) == ERR_SEM_TYPE);
        ast_free_expression(e3);

        printf("PASSED\n");
    }

    // Test 4: Variables and Scoping
    {
        printf("[Test 4] Variable lookup and undefined error... ");
        AstType *t = NULL;

        symtable_push_scope(table);
        symtable_insert_variable(table, "my_var", type_get_int(), false);

        // my_var -> Int
        AstExpression *e1 = create_var_expr("my_var");
        assert(check_expression(e1, table, &t) == ERR_OK && t->kind == AST_TYPE_INT);
        ast_free_expression(e1);

        // unknown_var -> Error 3
        AstExpression *e2 = create_var_expr("unknown_var");
        assert(check_expression(e2, table, &t) == ERR_SEM_UNDEFINED);
        ast_free_expression(e2);

        symtable_pop_scope(table);
        printf("PASSED\n");
    }

    // Test 5: Built-in function call (print with Any, readInt)
    {
        printf("[Test 5] Builtin calls print(42) and print(\"hi\")... ");
        AstType *t = NULL;

        // print(42) -> Unit
        AstExpression *args1[] = { create_int_literal(42) };
        AstExpression *e1 = create_call_expr("print", args1, 1);
        assert(check_expression(e1, table, &t) == ERR_OK && t->kind == AST_TYPE_UNIT);
        ast_free_expression(e1);

        // print("hi") -> Unit
        AstExpression *args2[] = { create_string_literal("hi") };
        AstExpression *e2 = create_call_expr("print", args2, 1);
        assert(check_expression(e2, table, &t) == ERR_OK && t->kind == AST_TYPE_UNIT);
        ast_free_expression(e2);

        // readInt() -> Int
        AstExpression *e3 = create_call_expr("readInt", NULL, 0);
        assert(check_expression(e3, table, &t) == ERR_OK && t->kind == AST_TYPE_INT);
        ast_free_expression(e3);

        // readInt(42) (wrong arg count) -> Error 5
        AstExpression *args4[] = { create_int_literal(42) };
        AstExpression *e4 = create_call_expr("readInt", args4, 1);
        assert(check_expression(e4, table, &t) == ERR_SEM_CALL);
        ast_free_expression(e4);

        printf("PASSED\n");
    }

    // Cleanup
    ast_free_program(&dummy_prog);
    symtable_destroy(table);

    printf("\n>>> ALL TYPE CHECKER TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
