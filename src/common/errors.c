/**
 * Project: IFJ26 / IAL compiler
 * File: errors.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static const char *_error_code_to_string(ErrorCode code) {
    switch (code) {
        case ERR_OK:
            return "OK";
        case ERR_LEXICAL:
            return "Lexical Error";
        case ERR_SYNTAX:
            return "Syntax Error";
        case ERR_SEM_UNDEFINED:
            return "Semantic Error (Undefined identifier)";
        case ERR_SEM_REDEFINITION:
            return "Semantic Error (Redefinition)";
        case ERR_SEM_CALL:
            return "Semantic Error (Call/Argument mismatch)";
        case ERR_SEM_TYPE:
            return "Semantic Error (Type mismatch)";
        case ERR_SEM_MUTABILITY:
            return "Semantic Error (Mutability violation)";
        case ERR_SEM_OTHER:
            return "Semantic Error";
        case ERR_RUNTIME_SEMANTICS:
            return "Runtime Semantic Error";
        case ERR_INTERNAL:
            return "Internal Compiler Error";
        default:
            return "Unknown Error";
    }
}

void error_print(ErrorCode code, position pos, const char *format, ...) {
    fprintf(stderr, "[%s] Line %zu, Column %zu: ", _error_code_to_string(code), pos.line, pos.column);
    if (format != NULL) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
    fprintf(stderr, "\n");
}

void error_print_no_pos(ErrorCode code, const char *format, ...) {
    fprintf(stderr, "[%s]: ", _error_code_to_string(code));
    if (format != NULL) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
    fprintf(stderr, "\n");
}

void error_exit(ErrorCode code, position pos, const char *format, ...) {
    if (pos.line > 0 || pos.column > 0) {
        fprintf(stderr, "[%s] Line %zu, Column %zu: ", _error_code_to_string(code), pos.line, pos.column);
    } else {
        fprintf(stderr, "[%s]: ", _error_code_to_string(code));
    }

    if (format != NULL) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
    fprintf(stderr, "\n");

    exit((int)code);
}

void error_exit_no_pos(ErrorCode code, const char *format, ...) {
    error_exit(code, (position){.line = 0, .column = 0}, format);
}
