/**
 * Project: IFJ26 / IAL compiler
 * File: type_checker.h
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "ast.h"
#include "symtable.h"
#include <stdbool.h>

/* ==================== Built-in Atomic Type Accessors ==================== */

AstType *type_get_int(void);
AstType *type_get_double(void);
AstType *type_get_string(void);
AstType *type_get_boolean(void);
AstType *type_get_unit(void);
AstType *type_get_any(void);

/* ==================== Type Compatibility Helpers ==================== */

/**
 * Checks whether an actual type is compatible with an expected type.
 * Supports exact equality, Any wildcard, and implicit Int -> Double promotion.
 *
 * @param expected Expected type (e.g., parameter or variable declared type).
 * @param actual Actual type (evaluated from an expression).
 * @return True if compatible, false otherwise.
 */
bool type_is_compatible(const AstType *expected, const AstType *actual);

/**
 * Checks whether two types are numbers (Int or Double).
 */
bool type_is_numeric(const AstType *type);

/* ==================== Expression Type Checking ==================== */

/**
 * Recursively checks and evaluates the type of an AST expression.
 * Performs bottom-up type inference, checks operator constraints and function calls,
 * and sets expr->evaluated_type to the resulting type.
 *
 * @param expr Pointer to AstExpression to check.
 * @param table Pointer to active Symtable.
 * @param out_type Pointer where the evaluated AstType* will be stored.
 * @return 0 (ERR_OK) on success, or an ErrorCode (3, 5, 6, etc.) on error.
 */
int check_expression(AstExpression *expr, Symtable *table, AstType **out_type);

#endif // TYPE_CHECKER_H
