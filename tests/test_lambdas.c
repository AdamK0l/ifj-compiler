/**
 * Project: IFJ26 / IAL compiler
 * File: test_lambdas.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 *
 * Comprehensive unit tests for LAMBDAS extension (anonymous functions & function values).
 */

#include "ast.h"
#include "semantics.h"
#include "symtable.h"
#include "type_checker.h"
#include "errors.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ==================== AST Builders ==================== */

static AstProgram *make_program(void) {
    return safe_calloc(1, sizeof(AstProgram));
}

static void program_add_def(AstProgram *p, AstDefinition def) {
    if (p->count >= p->capacity) {
        size_t new_cap = p->capacity == 0 ? 4 : p->capacity * 2;
        p->items = safe_realloc(p->items, new_cap * sizeof(AstDefinition));
        p->capacity = new_cap;
    }
    p->items[p->count++] = def;
}

static AstBLOCK *make_block(void) {
    return safe_calloc(1, sizeof(AstBLOCK));
}

static void block_add_stmt(AstBLOCK *b, AstStatement *s) {
    if (b->statements.count >= b->statements.capacity) {
        size_t new_cap = b->statements.capacity == 0 ? 4 : b->statements.capacity * 2;
        b->statements.items = safe_realloc(b->statements.items, new_cap * sizeof(AstStatement *));
        b->statements.capacity = new_cap;
    }
    b->statements.items[b->statements.count++] = s;
}

static AstStatement *make_var_stmt(const char *name, AstType *declared_type, AstExpression *init, bool is_changeable) {
    AstStatement *s = safe_calloc(1, sizeof(AstStatement));
    s->kind = AST_STATEMENT_VARIABLE;
    s->data.variable.name = safe_strdup(name);
    s->data.variable.declared_type = declared_type;
    s->data.variable.initializer = init;
    s->data.variable.is_changeable = is_changeable;
    return s;
}

static AstStatement *make_expr_stmt(AstExpression *expr) {
    AstStatement *s = safe_calloc(1, sizeof(AstStatement));
    s->kind = AST_STATEMENT_EXPRESSION;
    s->data.expression = expr;
    return s;
}

static AstStatement *make_return_stmt(AstExpression *expr) {
    AstStatement *s = safe_calloc(1, sizeof(AstStatement));
    s->kind = AST_STATEMENT_RETURN;
    s->data.return_value = expr;
    return s;
}

static AstExpression *make_expr_int(int32_t val) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_INT;
    e->data.int_value = val;
    return e;
}

static AstExpression *make_expr_name(const char *name) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_NAME;
    e->data.name = safe_strdup(name);
    return e;
}

static AstExpression *make_expr_binary(AstBinaryOp op, AstExpression *l, AstExpression *r) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_BINARY;
    e->data.binary.op = op;
    e->data.binary.left = l;
    e->data.binary.right = r;
    return e;
}

static AstExpression *make_call(AstExpression *callee, AstArgumentList args) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_CALL;
    e->data.call.callee = callee;
    e->data.call.arguments = args;
    return e;
}

static AstExpression *make_lambda(AstParameterList params, AstBLOCK *body) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_LAMBDA;
    e->data.lambda.parameters = params;
    e->data.lambda.inside = body;
    return e;
}

static AstFunction *make_function(const char *name, bool is_main, AstParameterLists params, AstType *ret_type, AstBLOCK *body) {
    AstFunction *f = safe_calloc(1, sizeof(AstFunction));
    f->name = safe_strdup(name);
    f->is_main = is_main;
    f->parameters = params;
    f->return_type = ret_type;
    f->inside = body;
    return f;
}

static AstParameterLists empty_parameter_lists(void) {
    AstParameterLists pls;
    pls.items = NULL;
    pls.count = 0;
    pls.capacity = 0;
    return pls;
}

static AstParameterLists one_param_list(AstParameterList pl) {
    AstParameterLists pls;
    pls.items = safe_calloc(1, sizeof(AstParameterList));
    pls.count = 1;
    pls.capacity = 1;
    pls.items[0] = pl;
    return pls;
}

static AstArgumentList one_arg_list(AstExpression **args, size_t count) {
    AstArgumentList al;
    al.items = safe_calloc(1, sizeof(AstExpressionList));
    al.count = 1;
    al.capacity = 1;
    al.items[0].items = safe_calloc(count, sizeof(AstExpression *));
    for (size_t i = 0; i < count; i++) {
        al.items[0].items[i] = args[i];
    }
    al.items[0].count = count;
    al.items[0].capacity = count;
    return al;
}

static AstType *make_fn_type(AstType **params, size_t param_count, AstType *ret) {
    AstType **cloned_p = NULL;
    if (param_count > 0 && params != NULL) {
        cloned_p = safe_malloc(sizeof(AstType *) * param_count);
        for (size_t i = 0; i < param_count; i++) {
            cloned_p[i] = ast_type_clone(params[i]);
        }
    }
    return ast_type_create_function(cloned_p, param_count, ast_type_clone(ret));
}

/* ==================== Tests ==================== */

void test_lambdas_success(void) {
    printf("[TEST] Running test_lambdas_success...\n");
    AstProgram *prog = make_program();

    // 1. def foo(x: Int): Int = x + 5
    {
        AstParameterList pl;
        pl.items = safe_calloc(1, sizeof(AstParameter));
        pl.items[0].name = safe_strdup("x");
        pl.items[0].type = ast_type_create(AST_TYPE_INT);
        pl.count = 1;
        pl.capacity = 1;

        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_binary(AST_ADD, make_expr_name("x"), make_expr_int(5))));

        AstFunction *fn = make_function("foo", false, one_param_list(pl), ast_type_create(AST_TYPE_INT), b);
        AstDefinition def; def.kind = AST_DEF_FUNCTION; def.data.function = fn;
        program_add_def(prog, def);
    }

    // 2. def bar(a: Int, f: (Int) => Boolean): Unit = val result = f(a)
    {
        AstType *p_int = ast_type_create(AST_TYPE_INT);
        AstType *p_bool = ast_type_create(AST_TYPE_BOOLEAN);
        AstType *fn_t = make_fn_type(&p_int, 1, p_bool);
        ast_type_free(p_int);
        ast_type_free(p_bool);

        AstParameterList pl;
        pl.items = safe_calloc(2, sizeof(AstParameter));
        pl.items[0].name = safe_strdup("a");
        pl.items[0].type = ast_type_create(AST_TYPE_INT);
        pl.items[1].name = safe_strdup("f");
        pl.items[1].type = fn_t;
        pl.count = 2;
        pl.capacity = 2;

        AstBLOCK *b = make_block();
        AstExpression *arg_a = make_expr_name("a");
        AstArgumentList args = one_arg_list(&arg_a, 1);
        block_add_stmt(b, make_var_stmt("result", ast_type_create(AST_TYPE_BOOLEAN), make_call(make_expr_name("f"), args), false));

        AstFunction *fn = make_function("bar", false, one_param_list(pl), ast_type_create(AST_TYPE_UNIT), b);
        AstDefinition def; def.kind = AST_DEF_FUNCTION; def.data.function = fn;
        program_add_def(prog, def);
    }

    // 3. @main def main(): Unit = ...
    {
        AstBLOCK *mb = make_block();

        // val add: (Int, Int) => Int = (left: Int, right: Int) => left + right
        {
            AstParameterList lparams;
            lparams.items = safe_calloc(2, sizeof(AstParameter));
            lparams.items[0].name = safe_strdup("left");
            lparams.items[0].type = ast_type_create(AST_TYPE_INT);
            lparams.items[1].name = safe_strdup("right");
            lparams.items[1].type = ast_type_create(AST_TYPE_INT);
            lparams.count = 2;
            lparams.capacity = 2;

            AstBLOCK *lb = make_block();
            block_add_stmt(lb, make_expr_stmt(make_expr_binary(AST_ADD, make_expr_name("left"), make_expr_name("right"))));

            AstExpression *lambda_add = make_lambda(lparams, lb);

            AstType *two_ints[2] = { ast_type_create(AST_TYPE_INT), ast_type_create(AST_TYPE_INT) };
            AstType *t_int = ast_type_create(AST_TYPE_INT);
            AstType *add_type = make_fn_type(two_ints, 2, t_int);
            ast_type_free(two_ints[0]);
            ast_type_free(two_ints[1]);
            ast_type_free(t_int);

            block_add_stmt(mb, make_var_stmt("add", add_type, lambda_add, false));
        }

        // val addFive: (Int) => Int = foo
        {
            AstType *p_int = ast_type_create(AST_TYPE_INT);
            AstType *addFive_type = make_fn_type(&p_int, 1, p_int);
            ast_type_free(p_int);

            block_add_stmt(mb, make_var_stmt("addFive", addFive_type, make_expr_name("foo"), false));
        }

        // val x = add(10, 8)
        {
            AstExpression *args_arr[2] = { make_expr_int(10), make_expr_int(8) };
            AstArgumentList al = one_arg_list(args_arr, 2);
            block_add_stmt(mb, make_var_stmt("x", ast_type_create(AST_TYPE_INT), make_call(make_expr_name("add"), al), false));
        }

        // val y = addFive(x)
        {
            AstExpression *arg_x = make_expr_name("x");
            AstArgumentList al = one_arg_list(&arg_x, 1);
            block_add_stmt(mb, make_var_stmt("y", ast_type_create(AST_TYPE_INT), make_call(make_expr_name("addFive"), al), false));
        }

        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition def; def.kind = AST_DEF_FUNCTION; def.data.function = main_fn;
        program_add_def(prog, def);
    }

    int res = analyze_semantics(prog);
    assert(res == ERR_OK);

    ast_free_program(prog);
    free(prog);
    printf("[Test 1] Lambda definition, assignment and execution... PASSED\n");
    printf("[Test 2] Higher-order functions with lambda parameter... PASSED\n");
}

void test_lambdas_errors(void) {
    // 1. Assigning named function without explicit variable type -> ERR_SEM_TYPE (6)
    {
        AstProgram *prog = make_program();
        AstFunction *f = make_function("foo", false, empty_parameter_lists(), ast_type_create(AST_TYPE_INT), make_block());
        AstDefinition df; df.kind = AST_DEF_FUNCTION; df.data.function = f;
        program_add_def(prog, df);

        AstBLOCK *mb = make_block();
        // val f = foo (missing explicit type)
        block_add_stmt(mb, make_var_stmt("f", NULL, make_expr_name("foo"), false));
        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_TYPE);
        ast_free_program(prog);
        free(prog);
        printf("[Test 3] Assigning function without explicit type -> Error 6... PASSED\n");
    }

    // 2. Return statement inside a lambda -> ERR_SEM_OTHER (10)
    {
        AstProgram *prog = make_program();
        AstBLOCK *lb = make_block();
        block_add_stmt(lb, make_return_stmt(make_expr_int(42)));

        AstParameterList lparams;
        lparams.items = NULL; lparams.count = 0; lparams.capacity = 0;
        AstExpression *l = make_lambda(lparams, lb);

        AstBLOCK *mb = make_block();
        block_add_stmt(mb, make_expr_stmt(l));
        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_OTHER);
        ast_free_program(prog);
        free(prog);
        printf("[Test 4] Return statement inside lambda forbidden -> Error 10... PASSED\n");
    }

    // 3. Duplicate parameter in lambda -> ERR_SEM_REDEFINITION (4)
    {
        AstProgram *prog = make_program();
        AstParameterList lparams;
        lparams.items = safe_calloc(2, sizeof(AstParameter));
        lparams.items[0].name = safe_strdup("x");
        lparams.items[0].type = ast_type_create(AST_TYPE_INT);
        lparams.items[1].name = safe_strdup("x");
        lparams.items[1].type = ast_type_create(AST_TYPE_INT);
        lparams.count = 2; lparams.capacity = 2;

        AstExpression *l = make_lambda(lparams, make_block());
        AstBLOCK *mb = make_block();
        block_add_stmt(mb, make_expr_stmt(l));
        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_REDEFINITION);
        ast_free_program(prog);
        free(prog);
        printf("[Test 5] Duplicate parameter in lambda -> Error 4... PASSED\n");
    }

    // 4. Calling function value with wrong argument count -> ERR_SEM_CALL (5)
    {
        AstProgram *prog = make_program();
        AstBLOCK *mb = make_block();

        AstParameterList lparams;
        lparams.items = safe_calloc(1, sizeof(AstParameter));
        lparams.items[0].name = safe_strdup("x");
        lparams.items[0].type = ast_type_create(AST_TYPE_INT);
        lparams.count = 1; lparams.capacity = 1;
        AstExpression *l = make_lambda(lparams, make_block());

        AstType *p_int = ast_type_create(AST_TYPE_INT);
        AstType *p_unit = ast_type_create(AST_TYPE_UNIT);
        AstType *fn_t = make_fn_type(&p_int, 1, p_unit);
        ast_type_free(p_int);
        ast_type_free(p_unit);

        block_add_stmt(mb, make_var_stmt("fn", fn_t, l, false));
        // Calling fn(1, 2) when it expects 1 arg
        AstExpression *args_arr[2] = { make_expr_int(1), make_expr_int(2) };
        block_add_stmt(mb, make_expr_stmt(make_call(make_expr_name("fn"), one_arg_list(args_arr, 2))));

        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_CALL);
        ast_free_program(prog);
        free(prog);
    }

    // 5. Calling a non-function variable -> ERR_SEM_TYPE (6)
    {
        AstProgram *prog = make_program();
        AstBLOCK *mb = make_block();
        block_add_stmt(mb, make_var_stmt("num", ast_type_create(AST_TYPE_INT), make_expr_int(42), false));
        AstExpression *arg = make_expr_int(1);
        block_add_stmt(mb, make_expr_stmt(make_call(make_expr_name("num"), one_arg_list(&arg, 1))));

        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_TYPE);
        ast_free_program(prog);
        free(prog);
        printf("[Test 6] Function value call validation (wrong arity & non-callable) -> Error 5/6... PASSED\n");
    }
}

int main(void) {
    printf("=== Running LAMBDAS Extension Tests ===\n\n");
    test_lambdas_success();
    test_lambdas_errors();
    printf("\n>>> ALL 6 LAMBDAS TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
