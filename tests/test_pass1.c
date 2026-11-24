/**
 * Project: IFJ26 / IAL compiler
 * File: test_pass1.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "ast.h"
#include "errors.h"
#include "memory.h"
#include "semantics.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AstFunction *create_dummy_func(const char *name, bool is_main, AstType *ret_type) {
    AstFunction *func = (AstFunction *)safe_malloc(sizeof(AstFunction));
    func->name = safe_strdup(name);
    func->is_main = is_main;
    func->return_type = ret_type;
    func->inside = NULL;
    func->pos.line = 1;
    func->pos.column = 1;
    func->parameters.items = NULL;
    func->parameters.count = 0;
    func->parameters.capacity = 0;
    return func;
}

static void add_parameter(AstFunction *func, const char *param_name, AstType *type) {
    if (func->parameters.count == 0) {
        func->parameters.count = 1;
        func->parameters.capacity = 1;
        func->parameters.items = (AstParameterList *)safe_malloc(sizeof(AstParameterList));
        func->parameters.items[0].items = NULL;
        func->parameters.items[0].count = 0;
        func->parameters.items[0].capacity = 0;
    }
    AstParameterList *list = &func->parameters.items[0];
    list->count++;
    list->items = (AstParameter *)safe_realloc(list->items, sizeof(AstParameter) * list->count);
    list->items[list->count - 1].name = safe_strdup(param_name);
    list->items[list->count - 1].type = type;
    list->items[list->count - 1].pos.line = 1;
    list->items[list->count - 1].pos.column = 1;
}

static int run_pass1_test(AstProgram *prog) {
    Symtable *tbl = symtable_create();
    int res = semantic_pass1(prog, tbl);
    symtable_destroy(tbl);
    return res;
}

int main(void) {
    printf("=== Running Semantic Pass 1 Tests ===\n\n");

    // Test 1: Valid program (@main + helper function)
    {
        printf("[Test 1] Valid program with @main and add()... ");
        AstProgram prog;
        prog.count = 2;
        prog.capacity = 2;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 2);

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_dummy_func("main", true, ast_type_create(AST_TYPE_UNIT));

        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_dummy_func("add", false, ast_type_create(AST_TYPE_INT));
        add_parameter(prog.items[1].data.function, "x", ast_type_create(AST_TYPE_INT));
        add_parameter(prog.items[1].data.function, "y", ast_type_create(AST_TYPE_INT));

        int res = run_pass1_test(&prog);
        assert(res == ERR_OK);
        ast_free_program(&prog);
        printf("PASSED (result: %d)\n", res);
    }

    // Test 2: Missing @main
    {
        printf("[Test 2] Missing @main... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_dummy_func("foo", false, ast_type_create(AST_TYPE_UNIT));

        int res = run_pass1_test(&prog);
        assert(res == ERR_SEM_UNDEFINED);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_UNDEFINED 3)\n", res);
    }

    // Test 3: Redefined user function
    {
        printf("[Test 3] Redefined function 'add'... ");
        AstProgram prog;
        prog.count = 3;
        prog.capacity = 3;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 3);

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_dummy_func("main", true, ast_type_create(AST_TYPE_UNIT));

        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_dummy_func("add", false, ast_type_create(AST_TYPE_INT));

        prog.items[2].kind = AST_DEF_FUNCTION;
        prog.items[2].data.function = create_dummy_func("add", false, ast_type_create(AST_TYPE_DOUBLE));

        int res = run_pass1_test(&prog);
        assert(res == ERR_SEM_REDEFINITION);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_REDEFINITION 4)\n", res);
    }

    // Test 4: Redefined parameter in function
    {
        printf("[Test 4] Duplicate parameter name (x, x)... ");
        AstProgram prog;
        prog.count = 2;
        prog.capacity = 2;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 2);

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_dummy_func("main", true, ast_type_create(AST_TYPE_UNIT));

        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_dummy_func("add", false, ast_type_create(AST_TYPE_INT));
        add_parameter(prog.items[1].data.function, "x", ast_type_create(AST_TYPE_INT));
        add_parameter(prog.items[1].data.function, "x", ast_type_create(AST_TYPE_INT));

        int res = run_pass1_test(&prog);
        assert(res == ERR_SEM_REDEFINITION);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_REDEFINITION 4)\n", res);
    }

    // Test 5: @main with parameters
    {
        printf("[Test 5] @main with parameter... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_dummy_func("main", true, ast_type_create(AST_TYPE_UNIT));
        add_parameter(prog.items[0].data.function, "x", ast_type_create(AST_TYPE_INT));

        int res = run_pass1_test(&prog);
        assert(res == ERR_SEM_CALL);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_CALL 5)\n", res);
    }

    // Test 6: @main with invalid return type
    {
        printf("[Test 6] @main returning Int instead of Unit... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_dummy_func("main", true, ast_type_create(AST_TYPE_INT));

        int res = run_pass1_test(&prog);
        assert(res == ERR_SEM_TYPE);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_TYPE 6)\n", res);
    }

    // Test 7: Redefined built-in function (e.g. def print())
    {
        printf("[Test 7] Redefining built-in function 'print'... ");
        AstProgram prog;
        prog.count = 2;
        prog.capacity = 2;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 2);

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_dummy_func("main", true, ast_type_create(AST_TYPE_UNIT));

        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_dummy_func("print", false, ast_type_create(AST_TYPE_UNIT));

        int res = run_pass1_test(&prog);
        assert(res == ERR_SEM_REDEFINITION);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_REDEFINITION 4)\n", res);
    }

    printf("\n>>> ALL PASS 1 TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
