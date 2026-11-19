/*
 * Projekt IFJ26 - překladač jazyka IFJ26 do IFJcode26
 * codegen.h - generátor cílového kódu IFJcode26 z AST
 *
 * Autoři: <Jiří Janásek, xjanasj00>
 *
 *
 * * codegen.h defines the interface for the code generator that translates
 * a semantically checked AST (`AstProgram`) from IFJ26 into IFJcode26
 * target code written to the given output stream (`FILE *out`). It returns
 * 0 on success and 99 on internal error or inconsistent AST without
 * writing partial output to `out`.
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "ast.h"

/**
 * Generate the whole IFJcode26 program from the AST.
 * Nothing is written to out on error. Out of memory terminates with code 99.
 * @param program  AST after semantic analysis
 * @param out      output file (stdout in the compiler)
 * @return         0 on success, 99 on an inconsistent AST
 */
int codegen_program(const AstProgram *program, FILE *out);

#endif /* CODEGEN_H */
