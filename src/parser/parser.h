/**
 * Project: IFJ26 / IAL compiler
 * File: parser.h
 * Authors:
 *   Peter Takáč (xtakacp00)
 *   Adam Kolacek (xkolaca00)
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "errors.h"
#include "scanner.h"

/**
 * Runs the syntactic analyzer (parser) on the tokens from scanner.
 * On success (ERR_OK), sets *out_ast to the newly allocated AstProgram.
 * On syntax or other error, returns the error code and frees any partially built AST.
 */
ErrorCode parser_run(Scanner *scanner, AstProgram **out_ast);

#endif // PARSER_H
