/**
 * Project: IFJ26 / IAL compiler
 * File: test_parser.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "ast.h"
#include "errors.h"
#include "parser.h"
#include "scanner.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *create_input_file(const char *source) {
    FILE *f = tmpfile();
    assert(f != NULL);
    fputs(source, f);
    rewind(f);
    return f;
}

static void test_simple_program(void) {
    const char *src =
        "import ifj26.*\n"
        "\n"
        "@main\n"
        "def main() =\n"
        "    val x = 10\n"
        "    var y: Int = x + 5 * 2\n"
        "    print(y)\n";

    FILE *f = create_input_file(src);
    Scanner scanner;
    ErrorCode err = scanner_init(&scanner, f);
    assert(err == ERR_OK);

    AstProgram *ast = NULL;
    err = parser_run(&scanner, &ast);
    assert(err == ERR_OK);
    assert(ast != NULL);
    assert(ast->count == 1);

    AstDefinition *def0 = &ast->items[0];
    assert(def0->kind == AST_DEF_FUNCTION);
    assert(strcmp(def0->data.function->name, "main") == 0);
    assert(def0->data.function->is_main == true);
    assert(def0->data.function->inside != NULL);
    assert(def0->data.function->inside->statements.count == 3);

    /* Check statement 0: val x = 10 */
    AstStatement *stmt0 = def0->data.function->inside->statements.items[0];
    assert(stmt0->kind == AST_STATEMENT_VARIABLE);
    assert(strcmp(stmt0->data.variable.name, "x") == 0);
    assert(stmt0->data.variable.is_changeable == false);
    assert(stmt0->data.variable.initializer->kind == AST_EXPR_INT);
    assert(stmt0->data.variable.initializer->data.int_value == 10);

    /* Check statement 1: var y: Int = x + 5 * 2 */
    AstStatement *stmt1 = def0->data.function->inside->statements.items[1];
    assert(stmt1->kind == AST_STATEMENT_VARIABLE);
    assert(strcmp(stmt1->data.variable.name, "y") == 0);
    assert(stmt1->data.variable.is_changeable == true);
    assert(stmt1->data.variable.declared_type != NULL);
    assert(stmt1->data.variable.declared_type->kind == AST_TYPE_INT);
    assert(stmt1->data.variable.initializer->kind == AST_EXPR_BINARY);
    assert(stmt1->data.variable.initializer->data.binary.op == AST_ADD);

    /* Check statement 2: print(y) */
    AstStatement *stmt2 = def0->data.function->inside->statements.items[2];
    assert(stmt2->kind == AST_STATEMENT_EXPRESSION);
    assert(stmt2->data.expression->kind == AST_EXPR_CALL);

    ast_free_program(ast);
    free(ast);
    scanner_destroy(&scanner);
    fclose(f);
    printf("test_simple_program passed.\n");
}

static void test_functions_and_parameters(void) {
    const char *src =
        "import ifj26.*\n"
        "\n"
        "def add(a: Int, b: Int): Int =\n"
        "    return a + b\n"
        "\n"
        "@main\n"
        "def main() =\n"
        "    val sum = add(3, 4)\n"
        "    print(sum)\n";

    FILE *f = create_input_file(src);
    Scanner scanner;
    ErrorCode err = scanner_init(&scanner, f);
    assert(err == ERR_OK);

    AstProgram *ast = NULL;
    err = parser_run(&scanner, &ast);
    assert(err == ERR_OK);
    assert(ast != NULL);
    assert(ast->count == 2);

    /* Check add function */
    AstFunction *fn_add = ast->items[0].data.function;
    assert(strcmp(fn_add->name, "add") == 0);
    assert(fn_add->is_main == false);
    assert(fn_add->return_type != NULL);
    assert(fn_add->return_type->kind == AST_TYPE_INT);
    assert(fn_add->parameters.count == 1);
    assert(fn_add->parameters.items[0].count == 2);
    assert(strcmp(fn_add->parameters.items[0].items[0].name, "a") == 0);
    assert(strcmp(fn_add->parameters.items[0].items[1].name, "b") == 0);

    /* Check body of add */
    assert(fn_add->inside->statements.count == 1);
    assert(fn_add->inside->statements.items[0]->kind == AST_STATEMENT_RETURN);

    ast_free_program(ast);
    free(ast);
    scanner_destroy(&scanner);
    fclose(f);
    printf("test_functions_and_parameters passed.\n");
}

static void test_while_and_if(void) {
    const char *src =
        "import ifj26.*\n"
        "\n"
        "@main\n"
        "def main() =\n"
        "    var i = 0\n"
        "    while i < 10 do\n"
        "        if i == 5 then\n"
        "            print(\"half\\n\")\n"
        "        else\n"
        "            print(i)\n"
        "        i = i + 1\n";

    FILE *f = create_input_file(src);
    Scanner scanner;
    ErrorCode err = scanner_init(&scanner, f);
    assert(err == ERR_OK);

    AstProgram *ast = NULL;
    err = parser_run(&scanner, &ast);
    assert(err == ERR_OK);
    assert(ast != NULL);
    assert(ast->count == 1);

    AstFunction *fn_main = ast->items[0].data.function;
    assert(fn_main->inside->statements.count == 2);

    /* Check while statement */
    AstStatement *while_stmt = fn_main->inside->statements.items[1];
    assert(while_stmt->kind == AST_STATEMENT_WHILE);
    assert(while_stmt->data.while_loop.condition->kind == AST_EXPR_BINARY);
    assert(while_stmt->data.while_loop.condition->data.binary.op == AST_LESS);
    assert(while_stmt->data.while_loop.inside->statements.count == 2);

    /* Inside while: if statement, then assignment i = i + 1 */
    AstStatement *if_stmt = while_stmt->data.while_loop.inside->statements.items[0];
    assert(if_stmt->kind == AST_STATEMENT_EXPRESSION);
    assert(if_stmt->data.expression->kind == AST_EXPR_IF);

    AstStatement *assign_stmt = while_stmt->data.while_loop.inside->statements.items[1];
    assert(assign_stmt->kind == AST_STATEMENT_ASSIGN);
    assert(strcmp(assign_stmt->data.assignment.name, "i") == 0);

    ast_free_program(ast);
    free(ast);
    scanner_destroy(&scanner);
    fclose(f);
    printf("test_while_and_if passed.\n");
}

static void test_syntax_errors(void) {
    /* Missing prolog */
    {
        const char *src =
            "@main\n"
            "def main() =\n"
            "    val x = 1\n";
        FILE *f = create_input_file(src);
        Scanner scanner;
        assert(scanner_init(&scanner, f) == ERR_OK);
        AstProgram *ast = NULL;
        ErrorCode err = parser_run(&scanner, &ast);
        assert(err == ERR_SYNTAX);
        assert(ast == NULL);
        scanner_destroy(&scanner);
        fclose(f);
    }

    /* Missing function name */
    {
        const char *src =
            "import ifj26.*\n"
            "def () =\n"
            "    val x = 1\n";
        FILE *f = create_input_file(src);
        Scanner scanner;
        assert(scanner_init(&scanner, f) == ERR_OK);
        AstProgram *ast = NULL;
        ErrorCode err = parser_run(&scanner, &ast);
        assert(err == ERR_SYNTAX);
        assert(ast == NULL);
        scanner_destroy(&scanner);
        fclose(f);
    }
    printf("test_syntax_errors passed.\n");
}

int main(void) {
    test_simple_program();
    test_functions_and_parameters();
    test_while_and_if();
    test_syntax_errors();
    printf("\n>>> ALL PARSER TESTS PASSED! <<<\n");
    return 0;
}
