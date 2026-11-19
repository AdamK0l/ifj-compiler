/**
 * Project: IFJ26 / IAL compiler
 * File: test_extensions.c
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

/* ==================== Helpers ==================== */

static AstExpression *create_int(int32_t val) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_INT;
    e->evaluated_type = NULL;
    e->data.int_value = val;
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstExpression *create_bool(bool val) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_BOOLEAN;
    e->evaluated_type = NULL;
    e->data.boolean_value = val;
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstExpression *create_var(const char *name) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_NAME;
    e->evaluated_type = NULL;
    e->data.name = safe_strdup(name);
    e->pos.line = 1;
    e->pos.column = 1;
    return e;
}

static AstBLOCK *create_block(void) {
    AstBLOCK *b = (AstBLOCK *)safe_malloc(sizeof(AstBLOCK));
    b->pos.line = 1;
    b->pos.column = 1;
    b->statements.count = 0;
    b->statements.capacity = 0;
    b->statements.items = NULL;
    return b;
}

static void block_add(AstBLOCK *b, AstStatement *s) {
    b->statements.count++;
    b->statements.items = (AstStatement **)safe_realloc(b->statements.items, sizeof(AstStatement *) * b->statements.count);
    b->statements.items[b->statements.count - 1] = s;
}

static AstStatement *create_var_decl(const char *name, bool is_ch, AstType *t, AstExpression *init) {
    AstStatement *s = (AstStatement *)safe_malloc(sizeof(AstStatement));
    s->kind = AST_STATEMENT_VARIABLE;
    s->pos.line = 1;
    s->pos.column = 1;
    s->data.variable.name = safe_strdup(name);
    s->data.variable.is_changeable = is_ch;
    s->data.variable.declared_type = t;
    s->data.variable.initializer = init;
    return s;
}

static AstStatement *create_expr_stmt(AstExpression *e) {
    AstStatement *s = (AstStatement *)safe_malloc(sizeof(AstStatement));
    s->kind = AST_STATEMENT_EXPRESSION;
    s->pos.line = 1;
    s->pos.column = 1;
    s->data.expression = e;
    return s;
}

static AstExpression *create_if_expr(AstExpression *cond, AstBLOCK *then_b, AstBLOCK *else_b) {
    AstExpression *e = (AstExpression *)safe_malloc(sizeof(AstExpression));
    e->kind = AST_EXPR_IF;
    e->evaluated_type = NULL;
    e->pos.line = 1;
    e->pos.column = 1;
    e->data.conditional.condition = cond;
    e->data.conditional.then_branch = then_b;
    e->data.conditional.else_branch = else_b;
    return e;
}

static AstFunction *create_func(const char *name, bool is_main, AstType *ret, AstBLOCK *body) {
    AstFunction *f = (AstFunction *)safe_malloc(sizeof(AstFunction));
    f->pos.line = 1;
    f->pos.column = 1;
    f->name = safe_strdup(name);
    f->is_main = is_main;
    f->return_type = ret;
    f->inside = body;
    f->parameters.count = 0;
    f->parameters.capacity = 0;
    f->parameters.items = NULL;
    return f;
}

/* ==================== Tests ==================== */

int main(void) {
    printf("=== Running Extensions Tests (BOOLTHEN, CONTROL, PARAMLISTS, INTERPOLATEDS, FUNEXP) ===\n\n");

    // Test 1: BOOLTHEN - if expression without else (evaluates to Unit)
    {
        printf("[Test 1] BOOLTHEN: 'if cond then ()' without else -> Unit... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        // @main def main(): Unit =
        //   val x = if true then ()
        AstBLOCK *then_b = create_block();
        AstExpression *if_expr = create_if_expr(create_bool(true), then_b, NULL);

        AstBLOCK *main_b = create_block();
        block_add(main_b, create_var_decl("x", false, NULL, if_expr));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), main_b);

        int res = analyze_semantics(&prog);
        assert(res == ERR_OK);
        ast_free_program(&prog);
        printf("PASSED (result: %d)\n", res);
    }

    // Test 2: CONTROL - if expression with else producing value
    {
        printf("[Test 2] CONTROL: 'val abs = if x < 0 then 0 - x else x'... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        // @main def main(): Unit =
        //   val x = 5
        //   val abs: Int = if true then 10 else 20
        AstBLOCK *then_b = create_block();
        block_add(then_b, create_expr_stmt(create_int(10)));

        AstBLOCK *else_b = create_block();
        block_add(else_b, create_expr_stmt(create_int(20)));

        AstExpression *if_expr = create_if_expr(create_bool(true), then_b, else_b);

        AstBLOCK *main_b = create_block();
        block_add(main_b, create_var_decl("abs", false, ast_type_create(AST_TYPE_INT), if_expr));

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), main_b);

        int res = analyze_semantics(&prog);
        assert(res == ERR_OK);
        ast_free_program(&prog);
        printf("PASSED (result: %d)\n", res);
    }

    // Test 3: CONTROL - for loop with guard
    {
        printf("[Test 3] CONTROL: for loop with guard... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        AstStatement *for_stmt = (AstStatement *)safe_malloc(sizeof(AstStatement));
        for_stmt->kind = AST_STATEMENT_FOR;
        for_stmt->pos.line = 1;
        for_stmt->pos.column = 1;
        for_stmt->data.for_loop.variable_name = safe_strdup("i");
        for_stmt->data.for_loop.variable_pos.line = 1;
        for_stmt->data.for_loop.variable_pos.column = 1;
        for_stmt->data.for_loop.from = create_int(1);
        for_stmt->data.for_loop.to = create_int(10);
        for_stmt->data.for_loop.inclusive = true;
        for_stmt->data.for_loop.step = create_int(2);
        for_stmt->data.for_loop.inside = create_block();

        // guard: if i > 3
        for_stmt->data.for_loop.guards.count = 1;
        for_stmt->data.for_loop.guards.capacity = 1;
        for_stmt->data.for_loop.guards.items = (AstExpression **)safe_malloc(sizeof(AstExpression *));
        for_stmt->data.for_loop.guards.items[0] = create_bool(true);

        AstBLOCK *main_b = create_block();
        block_add(main_b, for_stmt);

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), main_b);

        int res = analyze_semantics(&prog);
        assert(res == ERR_OK);
        ast_free_program(&prog);
        printf("PASSED (result: %d)\n", res);
    }

    // Test 4: CONTROL - for loop with step == 0 -> Error 10
    {
        printf("[Test 4] CONTROL: for loop with step 0 -> Error 10... ");
        AstProgram prog;
        prog.count = 1;
        prog.capacity = 1;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition));

        AstStatement *for_stmt = (AstStatement *)safe_malloc(sizeof(AstStatement));
        for_stmt->kind = AST_STATEMENT_FOR;
        for_stmt->pos.line = 1;
        for_stmt->pos.column = 1;
        for_stmt->data.for_loop.variable_name = safe_strdup("i");
        for_stmt->data.for_loop.variable_pos.line = 1;
        for_stmt->data.for_loop.variable_pos.column = 1;
        for_stmt->data.for_loop.from = create_int(1);
        for_stmt->data.for_loop.to = create_int(10);
        for_stmt->data.for_loop.inclusive = true;
        for_stmt->data.for_loop.step = create_int(0); // STEP 0 -> ERROR 10
        for_stmt->data.for_loop.inside = create_block();
        for_stmt->data.for_loop.guards.count = 0;
        for_stmt->data.for_loop.guards.capacity = 0;
        for_stmt->data.for_loop.guards.items = NULL;

        AstBLOCK *main_b = create_block();
        block_add(main_b, for_stmt);

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), main_b);

        int res = analyze_semantics(&prog);
        assert(res == ERR_SEM_OTHER);
        ast_free_program(&prog);
        printf("PASSED (result: %d, expected ERR_SEM_OTHER 10)\n", res);
    }

    // Test 5: PARAMLISTS - Multi-parameter list function
    {
        printf("[Test 5] PARAMLISTS: 'def f(a: Int)(b: Int): Int = a + b'... ");
        AstProgram prog;
        prog.count = 2;
        prog.capacity = 2;
        prog.items = (AstDefinition *)safe_malloc(sizeof(AstDefinition) * 2);

        // def f(a: Int)(b: Int): Int = a
        AstBLOCK *f_b = create_block();
        block_add(f_b, create_expr_stmt(create_var("a")));

        AstFunction *f = create_func("f", false, ast_type_create(AST_TYPE_INT), f_b);
        f->parameters.count = 2;
        f->parameters.capacity = 2;
        f->parameters.items = (AstParameterList *)safe_malloc(sizeof(AstParameterList) * 2);

        // 1st list: (a: Int)
        f->parameters.items[0].count = 1;
        f->parameters.items[0].capacity = 1;
        f->parameters.items[0].items = (AstParameter *)safe_malloc(sizeof(AstParameter));
        f->parameters.items[0].items[0].name = safe_strdup("a");
        f->parameters.items[0].items[0].type = ast_type_create(AST_TYPE_INT);
        f->parameters.items[0].items[0].pos.line = 1;
        f->parameters.items[0].items[0].pos.column = 1;

        // 2nd list: (b: Int)
        f->parameters.items[1].count = 1;
        f->parameters.items[1].capacity = 1;
        f->parameters.items[1].items = (AstParameter *)safe_malloc(sizeof(AstParameter));
        f->parameters.items[1].items[0].name = safe_strdup("b");
        f->parameters.items[1].items[0].type = ast_type_create(AST_TYPE_INT);
        f->parameters.items[1].items[0].pos.line = 1;
        f->parameters.items[1].items[0].pos.column = 1;

        prog.items[0].kind = AST_DEF_FUNCTION;
        prog.items[0].data.function = f;

        // @main def main(): Unit = ()
        prog.items[1].kind = AST_DEF_FUNCTION;
        prog.items[1].data.function = create_func("main", true, ast_type_create(AST_TYPE_UNIT), create_block());

        int res = analyze_semantics(&prog);
        assert(res == ERR_OK);
        ast_free_program(&prog);
        printf("PASSED (result: %d)\n", res);
    }

    // Test 6: INTERPOLATEDS - String interpolation
    {
        printf("[Test 6] INTERPOLATEDS: s\"Hello ${1 + 2}\"... ");
        AstType *t = NULL;
        Symtable *tbl = symtable_create();

        AstExpression *interp = (AstExpression *)safe_malloc(sizeof(AstExpression));
        interp->kind = AST_EXPR_INTERPOLATED_STRING;
        interp->evaluated_type = NULL;
        interp->pos.line = 1;
        interp->pos.column = 1;
        interp->data.interpolated_string.count = 2;
        interp->data.interpolated_string.items = (AstStringPart *)safe_malloc(sizeof(AstStringPart) * 2);

        // part 1: text "Hello "
        interp->data.interpolated_string.items[0].kind = AST_STRING_TEXT;
        interp->data.interpolated_string.items[0].data.text.data = safe_strdup("Hello ");
        interp->data.interpolated_string.items[0].data.text.length = 6;
        interp->data.interpolated_string.items[0].pos.line = 1;
        interp->data.interpolated_string.items[0].pos.column = 1;

        // part 2: expression 123
        interp->data.interpolated_string.items[1].kind = AST_STRING_EXPRESSION;
        interp->data.interpolated_string.items[1].data.expression = create_int(123);
        interp->data.interpolated_string.items[1].pos.line = 1;
        interp->data.interpolated_string.items[1].pos.column = 1;

        int res = check_expression(interp, tbl, &t);
        assert(res == ERR_OK);
        assert(t != NULL && t->kind == AST_TYPE_STRING);

        ast_free_expression(interp);
        symtable_destroy(tbl);
        printf("PASSED (result: String)\n");
    }

    printf("\n>>> ALL 6 EXTENSIONS TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
