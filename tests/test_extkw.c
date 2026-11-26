/**
 * Project: IFJ26 / IAL compiler
 * File: test_extkw.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 *
 * Comprehensive unit tests for EXTKW extension (extension methods).
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

/* ==================== AST Builder Helpers ==================== */

static AstProgram *make_program(void) {
    AstProgram *p = safe_calloc(1, sizeof(AstProgram));
    return p;
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

static AstExpression *make_expr_int(int32_t val) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_INT;
    e->data.int_value = val;
    return e;
}

static AstExpression *make_expr_double(double val) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_DOUBLE;
    e->data.double_value = val;
    return e;
}

static AstExpression *make_expr_string(const char *val) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_STRING;
    e->data.string_value.data = safe_strdup(val);
    e->data.string_value.length = strlen(val);
    return e;
}

static AstExpression *make_expr_name(const char *name) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_NAME;
    e->data.name = safe_strdup(name);
    return e;
}

static AstExpression *make_member(AstExpression *receiver, const char *name) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_MEMBER;
    e->data.member.receiver = receiver;
    e->data.member.name = safe_strdup(name);
    return e;
}

static AstExpression *make_member_call(AstExpression *receiver, const char *name, AstArgumentList args) {
    AstExpression *e = safe_calloc(1, sizeof(AstExpression));
    e->kind = AST_EXPR_CALL;
    e->data.call.callee = make_member(receiver, name);
    e->data.call.arguments = args;
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

static AstExtension *make_extension(const char *receiver_name, AstType *receiver_type) {
    AstExtension *ext = safe_calloc(1, sizeof(AstExtension));
    ext->receiver.name = safe_strdup(receiver_name);
    ext->receiver.type = receiver_type;
    return ext;
}

static void extension_add_method(AstExtension *ext, AstFunction *fn) {
    ext->methods = safe_realloc(ext->methods, (ext->method_count + 1) * sizeof(AstFunction *));
    ext->methods[ext->method_count++] = fn;
}

/* Helper to construct AstParameterLists with 0 parameter lists */
static AstParameterLists empty_parameter_lists(void) {
    AstParameterLists pls;
    pls.items = NULL;
    pls.count = 0;
    pls.capacity = 0;
    return pls;
}

/* Helper to construct AstParameterLists with 1 empty parameter list: () */
static AstParameterLists one_empty_parameter_list(void) {
    AstParameterLists pls;
    pls.items = safe_calloc(1, sizeof(AstParameterList));
    pls.count = 1;
    pls.capacity = 1;
    return pls;
}

/* Helper to construct AstParameterLists with 1 parameter list containing 1 param: (name: type) */
static AstParameterLists one_param_list(const char *name, AstType *type) {
    AstParameterLists pls;
    pls.items = safe_calloc(1, sizeof(AstParameterList));
    pls.count = 1;
    pls.capacity = 1;
    pls.items[0].items = safe_calloc(1, sizeof(AstParameter));
    pls.items[0].items[0].name = safe_strdup(name);
    pls.items[0].items[0].type = type;
    pls.items[0].count = 1;
    pls.items[0].capacity = 1;
    return pls;
}

/* Helper to construct AstArgumentList with 1 empty argument list: () */
static AstArgumentList one_empty_argument_list(void) {
    AstArgumentList al;
    al.items = safe_calloc(1, sizeof(AstExpressionList));
    al.count = 1;
    al.capacity = 1;
    return al;
}

/* Helper to construct AstArgumentList with 1 argument list containing 1 expr: (arg) */
static AstArgumentList one_arg_list(AstExpression *arg) {
    AstArgumentList al;
    al.items = safe_calloc(1, sizeof(AstExpressionList));
    al.count = 1;
    al.capacity = 1;
    al.items[0].items = safe_calloc(1, sizeof(AstExpression *));
    al.items[0].items[0] = arg;
    al.items[0].count = 1;
    al.items[0].capacity = 1;
    return al;
}

/* ==================== Test Cases ==================== */

void test_extkw_success(void) {
    printf("[TEST] Running test_extkw_success...\n");
    AstProgram *prog = make_program();

    // extension (value: Int)
    //   def twice: Int = value * 2
    //   def foo(): String = "foo"
    //   def plus(other: Int): Int = value + other
    AstExtension *ext = make_extension("value", ast_type_create(AST_TYPE_INT));

    // 1. def twice: Int
    {
        AstBLOCK *b = make_block();
        AstExpression *mul = safe_calloc(1, sizeof(AstExpression));
        mul->kind = AST_EXPR_BINARY;
        mul->data.binary.op = AST_MULTIPLY;
        mul->data.binary.left = make_expr_name("value");
        mul->data.binary.right = make_expr_int(2);
        block_add_stmt(b, make_expr_stmt(mul));
        AstFunction *fn = make_function("twice", false, empty_parameter_lists(), ast_type_create(AST_TYPE_INT), b);
        extension_add_method(ext, fn);
    }

    // 2. def foo(): String
    {
        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_string("foo")));
        AstFunction *fn = make_function("foo", false, one_empty_parameter_list(), ast_type_create(AST_TYPE_STRING), b);
        extension_add_method(ext, fn);
    }

    // 3. def plus(other: Int): Int
    {
        AstBLOCK *b = make_block();
        AstExpression *add = safe_calloc(1, sizeof(AstExpression));
        add->kind = AST_EXPR_BINARY;
        add->data.binary.op = AST_ADD;
        add->data.binary.left = make_expr_name("value");
        add->data.binary.right = make_expr_name("other");
        block_add_stmt(b, make_expr_stmt(add));
        AstFunction *fn = make_function("plus", false, one_param_list("other", ast_type_create(AST_TYPE_INT)), ast_type_create(AST_TYPE_INT), b);
        extension_add_method(ext, fn);
    }

    AstDefinition def_ext;
    def_ext.kind = AST_DEF_EXTENSION;
    def_ext.data.extension = ext;
    program_add_def(prog, def_ext);

    // @main def main(): Unit =
    //   val x: Int = 6
    //   val y: Int = x.twice
    //   val z: String = x.foo()
    //   val w: Int = x.plus(12)
    {
        AstBLOCK *main_block = make_block();
        block_add_stmt(main_block, make_var_stmt("x", ast_type_create(AST_TYPE_INT), make_expr_int(6), false));
        block_add_stmt(main_block, make_var_stmt("y", ast_type_create(AST_TYPE_INT), make_member(make_expr_name("x"), "twice"), false));
        block_add_stmt(main_block, make_var_stmt("z", ast_type_create(AST_TYPE_STRING), make_member_call(make_expr_name("x"), "foo", one_empty_argument_list()), false));
        block_add_stmt(main_block, make_var_stmt("w", ast_type_create(AST_TYPE_INT), make_member_call(make_expr_name("x"), "plus", one_arg_list(make_expr_int(12))), false));

        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), main_block);
        AstDefinition def_main;
        def_main.kind = AST_DEF_FUNCTION;
        def_main.data.function = main_fn;
        program_add_def(prog, def_main);
    }

    int res = analyze_semantics(prog);
    assert(res == ERR_OK);

    ast_free_program(prog);
    free(prog);
    printf("[Test 1] Extension methods (parameterless & with params)... PASSED\n");
}

void test_extkw_redefinition_error(void) {
    AstProgram *prog = make_program();

    // extension (v: Int) def m: Int = 1
    // extension (v: Int) def m: Int = 2 -> Redefinition! (Error 4)
    AstExtension *ext1 = make_extension("v", ast_type_create(AST_TYPE_INT));
    {
        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_int(1)));
        extension_add_method(ext1, make_function("m", false, empty_parameter_lists(), ast_type_create(AST_TYPE_INT), b));
    }
    AstDefinition d1; d1.kind = AST_DEF_EXTENSION; d1.data.extension = ext1;
    program_add_def(prog, d1);

    AstExtension *ext2 = make_extension("v", ast_type_create(AST_TYPE_INT));
    {
        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_int(2)));
        extension_add_method(ext2, make_function("m", false, empty_parameter_lists(), ast_type_create(AST_TYPE_INT), b));
    }
    AstDefinition d2; d2.kind = AST_DEF_EXTENSION; d2.data.extension = ext2;
    program_add_def(prog, d2);

    // Dummy @main
    AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), make_block());
    AstDefinition def_main; def_main.kind = AST_DEF_FUNCTION; def_main.data.function = main_fn;
    program_add_def(prog, def_main);

    int res = analyze_semantics(prog);
    assert(res == ERR_SEM_REDEFINITION);

    ast_free_program(prog);
    free(prog);
    printf("[Test 2] Duplicate extension method for same receiver type -> Error 4... PASSED\n");
}

void test_extkw_call_errors(void) {
    // 1. Calling parameterless method with parens: x.twice() -> ERR_SEM_CALL (5)
    {
        AstProgram *prog = make_program();
        AstExtension *ext = make_extension("v", ast_type_create(AST_TYPE_INT));
        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_int(1)));
        extension_add_method(ext, make_function("twice", false, empty_parameter_lists(), ast_type_create(AST_TYPE_INT), b));
        AstDefinition d1; d1.kind = AST_DEF_EXTENSION; d1.data.extension = ext;
        program_add_def(prog, d1);

        AstBLOCK *mb = make_block();
        block_add_stmt(mb, make_expr_stmt(make_member_call(make_expr_int(5), "twice", one_empty_argument_list())));
        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_CALL);
        ast_free_program(prog);
        free(prog);
        printf("[Test 3] Parameterless method called with parens -> Error 5... PASSED\n");
    }

    // 2. Calling method with () as bare member: x.foo -> ERR_SEM_CALL (5)
    {
        AstProgram *prog = make_program();
        AstExtension *ext = make_extension("v", ast_type_create(AST_TYPE_INT));
        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_string("s")));
        extension_add_method(ext, make_function("foo", false, one_empty_parameter_list(), ast_type_create(AST_TYPE_STRING), b));
        AstDefinition d1; d1.kind = AST_DEF_EXTENSION; d1.data.extension = ext;
        program_add_def(prog, d1);

        AstBLOCK *mb = make_block();
        block_add_stmt(mb, make_expr_stmt(make_member(make_expr_int(5), "foo")));
        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_CALL);
        ast_free_program(prog);
        free(prog);
        printf("[Test 4] Parameterized method called without parens -> Error 5... PASSED\n");
    }

    // 3. Calling method with incompatible argument type: x.plus("abc") -> ERR_SEM_CALL (5)
    {
        AstProgram *prog = make_program();
        AstExtension *ext = make_extension("v", ast_type_create(AST_TYPE_INT));
        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_int(1)));
        extension_add_method(ext, make_function("plus", false, one_param_list("other", ast_type_create(AST_TYPE_INT)), ast_type_create(AST_TYPE_INT), b));
        AstDefinition d1; d1.kind = AST_DEF_EXTENSION; d1.data.extension = ext;
        program_add_def(prog, d1);

        AstBLOCK *mb = make_block();
        block_add_stmt(mb, make_expr_stmt(make_member_call(make_expr_int(5), "plus", one_arg_list(make_expr_string("abc")))));
        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_CALL);
        ast_free_program(prog);
        free(prog);
        printf("[Test 5] Incompatible argument type -> Error 5... PASSED\n");
    }

    // 4. Calling unknown method: x.bar() -> ERR_SEM_UNDEFINED (3)
    {
        AstProgram *prog = make_program();
        AstBLOCK *mb = make_block();
        block_add_stmt(mb, make_expr_stmt(make_member_call(make_expr_int(5), "bar", one_empty_argument_list())));
        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);

        int res = analyze_semantics(prog);
        assert(res == ERR_SEM_UNDEFINED);
        ast_free_program(prog);
        free(prog);
        printf("[Test 6] Unknown extension method -> Error 3... PASSED\n");
    }
}

void test_extkw_polymorphism(void) {
    AstProgram *prog = make_program();

    // extension (v: Int) def getVal: Int = v
    {
        AstExtension *ext = make_extension("v", ast_type_create(AST_TYPE_INT));
        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_name("v")));
        extension_add_method(ext, make_function("getVal", false, empty_parameter_lists(), ast_type_create(AST_TYPE_INT), b));
        AstDefinition d; d.kind = AST_DEF_EXTENSION; d.data.extension = ext;
        program_add_def(prog, d);
    }

    // extension (v: Double) def getVal: Double = v
    {
        AstExtension *ext = make_extension("v", ast_type_create(AST_TYPE_DOUBLE));
        AstBLOCK *b = make_block();
        block_add_stmt(b, make_expr_stmt(make_expr_name("v")));
        extension_add_method(ext, make_function("getVal", false, empty_parameter_lists(), ast_type_create(AST_TYPE_DOUBLE), b));
        AstDefinition d; d.kind = AST_DEF_EXTENSION; d.data.extension = ext;
        program_add_def(prog, d);
    }

    // @main def main(): Unit =
    //   val i: Int = (10).getVal
    //   val d: Double = (3.14).getVal
    {
        AstBLOCK *mb = make_block();
        block_add_stmt(mb, make_var_stmt("i", ast_type_create(AST_TYPE_INT), make_member(make_expr_int(10), "getVal"), false));
        block_add_stmt(mb, make_var_stmt("d", ast_type_create(AST_TYPE_DOUBLE), make_member(make_expr_double(3.14), "getVal"), false));
        AstFunction *main_fn = make_function("main", true, empty_parameter_lists(), ast_type_create(AST_TYPE_UNIT), mb);
        AstDefinition dm; dm.kind = AST_DEF_FUNCTION; dm.data.function = main_fn;
        program_add_def(prog, dm);
    }

    int res = analyze_semantics(prog);
    assert(res == ERR_OK);

    ast_free_program(prog);
    free(prog);
    printf("[Test 7] Receiver type polymorphism (same method for Int & Double)... PASSED\n");
}

int main(void) {
    printf("=== Running EXTKW Extension Tests ===\n\n");
    test_extkw_success();
    test_extkw_redefinition_error();
    test_extkw_call_errors();
    test_extkw_polymorphism();
    printf("\n>>> ALL 7 EXTKW TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
