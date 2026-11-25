/**
 * Project: IFJ26 / IAL compiler
 * File: main.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 *   Jiří Janásek (xjanasj00)
 *   Peter Takáč (xtakacp00)
 *   Bálint Marcell Šógor (xsogorb00)
 */

#include "ast.h"
#include "codegen.h"
#include "errors.h"
#include "parser.h"
#include "scanner.h"
#include "semantics.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Scanner scanner;
    ErrorCode err = scanner_init(&scanner, stdin);
    if (err != ERR_OK) {
        return (int)err;
    }

    AstProgram *program = NULL;
    err = parser_run(&scanner, &program);
    scanner_destroy(&scanner);

    if (err != ERR_OK) {
        return (int)err;
    }

    int sem_err = analyze_semantics(program);
    if (sem_err != ERR_OK) {
        ast_free_program(program);
        free(program);
        return sem_err;
    }

    int gen_err = codegen_program(program, stdout);
    ast_free_program(program);
    free(program);

    if (gen_err != 0) {
        return gen_err;
    }

    return ERR_OK;
}
