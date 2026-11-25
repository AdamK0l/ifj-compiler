/**
 * Project: IFJ26 / IAL compiler
 * File: parser_test.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 *
 * Parser test driver: reads a program from stdin, prints the parser result.
 * Usage: ./parser_test < file.ifj          (result only)
 *        ./parser_test --dump < file.ifj   (result + AST dump)
 */

#include "ast.h"
#include "parser.h"
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    bool dump = (argc > 1 && strcmp(argv[1], "--dump") == 0);

    Scanner scanner;
    ErrorCode err = scanner_init(&scanner, stdin);
    if (err != ERR_OK) {
        printf("parser result: %d\n", err);
        return err;
    }

    AstProgram *program = NULL;
    err = parser_run(&scanner, &program);
    scanner_destroy(&scanner);

    if (err == ERR_OK && dump) {
        ast_dump_program(stdout, program);
    }
    if (program != NULL) {
        ast_free_program(program);
        free(program);
    }

    printf("parser result: %d\n", err);
    return err;
}
