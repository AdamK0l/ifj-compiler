/**
 * Project: IFJ26 / IAL compiler
 * File: test_valmem.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 *
 * Unit tests for VALMEM extension (member calls on values).
 */

#include "ast.h"
#include "symtable.h"
#include "type_checker.h"
#include "errors.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static AstExpression *make_literal_string(const char *val) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_STRING;
    e->data.string_value.data = safe_strdup(val);
    e->data.string_value.length = strlen(val);
    return e;
}

static AstExpression *make_literal_double(double val) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_DOUBLE;
    e->data.double_value = val;
    return e;
}

static AstExpression *make_literal_int(int32_t val) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_INT;
    e->data.int_value = val;
    return e;
}

static AstExpression *make_literal_bool(bool val) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_BOOLEAN;
    e->data.boolean_value = val;
    return e;
}

static AstExpression *make_member(AstExpression *receiver, const char *name) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_MEMBER;
    e->data.member.receiver = receiver;
    e->data.member.name = safe_strdup(name);
    return e;
}

void test_valmem_success(void) {
    Symtable *st = symtable_create();
    AstType *out_type = NULL;

    // 1. "hello".length -> Int
    {
        AstExpression *e = make_member(make_literal_string("hello"), "length");
        int res = check_expression(e, st, &out_type);
        assert(res == ERR_OK);
        assert(out_type->kind == AST_TYPE_INT);
        assert(e->evaluated_type != NULL && e->evaluated_type->kind == AST_TYPE_INT);
        assert(e->data.member.receiver->evaluated_type != NULL);
        assert(e->data.member.receiver->evaluated_type->kind == AST_TYPE_STRING);
        ast_free_expression(e);
        printf("[Test 1] Member .length on String -> Int... PASSED\n");
    }

    // 2. (3.14).toInt -> Int
    {
        AstExpression *e = make_member(make_literal_double(3.14), "toInt");
        int res = check_expression(e, st, &out_type);
        assert(res == ERR_OK);
        assert(out_type->kind == AST_TYPE_INT);
        assert(e->evaluated_type != NULL && e->evaluated_type->kind == AST_TYPE_INT);
        assert(e->data.member.receiver->evaluated_type != NULL);
        assert(e->data.member.receiver->evaluated_type->kind == AST_TYPE_DOUBLE);
        ast_free_expression(e);
        printf("[Test 2] Member .toInt on Double -> Int... PASSED\n");
    }

    // 3. (3.14).floor -> Double
    {
        AstExpression *e = make_member(make_literal_double(3.14), "floor");
        int res = check_expression(e, st, &out_type);
        assert(res == ERR_OK);
        assert(out_type->kind == AST_TYPE_DOUBLE);
        assert(e->evaluated_type != NULL && e->evaluated_type->kind == AST_TYPE_DOUBLE);
        assert(e->data.member.receiver->evaluated_type != NULL);
        assert(e->data.member.receiver->evaluated_type->kind == AST_TYPE_DOUBLE);
        ast_free_expression(e);
        printf("[Test 3] Member .floor on Double -> Double... PASSED\n");
    }

    // 4. (42).toString & (true).toString -> String
    {
        AstExpression *e1 = make_member(make_literal_int(42), "toString");
        int res1 = check_expression(e1, st, &out_type);
        assert(res1 == ERR_OK);
        assert(out_type->kind == AST_TYPE_STRING);
        ast_free_expression(e1);

        AstExpression *e2 = make_member(make_literal_bool(true), "toString");
        int res2 = check_expression(e2, st, &out_type);
        assert(res2 == ERR_OK);
        assert(out_type->kind == AST_TYPE_STRING);
        ast_free_expression(e2);
        printf("[Test 4] Member .toString on Int & Bool -> String... PASSED\n");
    }

    // 5. Chaining: (3.75).floor.toInt.toString -> String
    {
        AstExpression *e = make_member(
            make_member(
                make_member(make_literal_double(3.75), "floor"),
                "toInt"
            ),
            "toString"
        );
        int res = check_expression(e, st, &out_type);
        assert(res == ERR_OK);
        assert(out_type->kind == AST_TYPE_STRING);
        assert(e->evaluated_type->kind == AST_TYPE_STRING);
        ast_free_expression(e);
        printf("[Test 5] Member chaining .floor.toInt.toString... PASSED\n");
    }

    symtable_destroy(st);
}

void test_valmem_errors(void) {
    Symtable *st = symtable_create();
    AstType *out_type = NULL;

    // 6. Incompatible types: .toInt on String, .floor on Int, .length on Double -> ERR_SEM_TYPE (6)
    {
        AstExpression *e1 = make_member(make_literal_string("abc"), "toInt");
        int res1 = check_expression(e1, st, &out_type);
        assert(res1 == ERR_SEM_TYPE);
        ast_free_expression(e1);

        AstExpression *e2 = make_member(make_literal_int(42), "floor");
        int res2 = check_expression(e2, st, &out_type);
        assert(res2 == ERR_SEM_TYPE);
        ast_free_expression(e2);

        AstExpression *e3 = make_member(make_literal_double(3.14), "length");
        int res3 = check_expression(e3, st, &out_type);
        assert(res3 == ERR_SEM_TYPE);
        ast_free_expression(e3);
        printf("[Test 6] Incompatible member type errors -> Error 6... PASSED\n");
    }

    // 7. (42).nonExistent -> ERR_SEM_UNDEFINED (3)
    {
        AstExpression *e = make_member(make_literal_int(42), "nonExistent");
        int res = check_expression(e, st, &out_type);
        assert(res == ERR_SEM_UNDEFINED);
        ast_free_expression(e);
        printf("[Test 7] Undefined member .nonExistent -> Error 3... PASSED\n");
    }

    symtable_destroy(st);
}

int main(void) {
    printf("=== Running VALMEM Extension Tests ===\n\n");
    test_valmem_success();
    test_valmem_errors();
    printf("\n>>> ALL 7 VALMEM TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
