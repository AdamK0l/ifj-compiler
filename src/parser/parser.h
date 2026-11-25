/**
 * Project: IFJ26 / IAL compiler
 * File: parser.h
 * Authors:
 *   Peter Takáč (xtakacp00)
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "errors.h"
#include "scanner.h"

/* Parses the whole program from the scanner.
   On success returns ERR_OK and *out is a heap-allocated AstProgram
   (the caller frees it with ast_free_program + free).
   On error returns the error code and *out is NULL. */
ErrorCode parser_run(Scanner *scanner, AstProgram **out);

#endif
