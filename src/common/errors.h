/**
 * Project: IFJ26 / IAL compiler
 * File: errors.h
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#ifndef ERRORS_H
#define ERRORS_H

#include "ast.h"
#include <stdarg.h>

/**
 * Standard IFJ26 compiler return / exit codes defined in assignment section 6.
 */
typedef enum {
    ERR_OK = 0,                 // Successful compilation
    ERR_LEXICAL = 1,            // Lexical analysis error (invalid lexeme, indentation)
    ERR_SYNTAX = 2,             // Syntax error, missing import ifj26.*
    ERR_SEM_UNDEFINED = 3,      // Undefined variable, function, or type; missing @main
    ERR_SEM_REDEFINITION = 4,   // Redefinition of variable, function, or parameter
    ERR_SEM_CALL = 5,           // Invalid argument count/type in call, invalid @main parameters
    ERR_SEM_TYPE = 6,           // Type mismatch, invalid type inference
    ERR_SEM_MUTABILITY = 7,     // Assignment to immutable variable (val) or parameter
    ERR_SEM_OTHER = 10,         // Other semantic error (e.g. return outside function, step 0)
    ERR_RUNTIME_SEMANTICS = 40, // Runtime semantic error (used by interpreter ic26int)
    ERR_INTERNAL = 99           // Internal compiler failure (memory allocation failed, etc.)
} ErrorCode;

/**
 * Formats and prints an error message to stderr without terminating the process.
 *
 * @param code Error code defining the category.
 * @param pos Position in the source file (line, column).
 * @param format Printf-like format string.
 * @param ... Additional arguments for format string.
 */
void error_print(ErrorCode code, position pos, const char *format, ...);

/**
 * Formats and prints an error message without source position (e.g. general errors).
 *
 * @param code Error code defining the category.
 * @param format Printf-like format string.
 * @param ... Additional arguments for format string.
 */
void error_print_no_pos(ErrorCode code, const char *format, ...);

/**
 * Formats and prints an error message to stderr, then terminates the program with the given code.
 *
 * @param code Error code defining the category.
 * @param pos Position in the source file (line, column).
 * @param format Printf-like format string.
 * @param ... Additional arguments for format string.
 */
void error_exit(ErrorCode code, position pos, const char *format, ...);

/**
 * Formats and prints an error message without position, then terminates the program with the given code.
 *
 * @param code Error code defining the category.
 * @param format Printf-like format string.
 * @param ... Additional arguments for format string.
 */
void error_exit_no_pos(ErrorCode code, const char *format, ...);

#endif // ERRORS_H
