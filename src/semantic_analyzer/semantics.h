/**
 * Project: IFJ26 / IAL compiler
 * File: semantics.h
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "symtable.h"

/**
 * Analyzes the semantics of the entire AST program.
 * Performs Pass 1 (built-ins & global function collection) and Pass 2 (function bodies & type checking).
 *
 * @param program Pointer to the AST root.
 * @return 0 (ERR_OK) on success, or ErrorCode on error.
 */
int analyze_semantics(AstProgram *program);

/**
 * Performs Pass 1 of semantic analysis:
 * - Registers all built-in functions into the global scope.
 * - Registers all user function signatures into the global scope (checks for redefinition -> Error 4).
 * - Validates the @main entry function (exactly 1, 0 params -> Error 5, Unit return -> Error 6, missing -> Error 3).
 *
 * @param program Pointer to the AST root.
 * @param table Pointer to the Symtable (containing the global scope).
 * @return 0 (ERR_OK) on success, or ErrorCode on error.
 */
int semantic_pass1(AstProgram *program, Symtable *table);

#endif // SEMANTICS_H
