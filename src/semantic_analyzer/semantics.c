/**
 * Project: IFJ26 / IAL compiler
 * File: semantics.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "semantics.h"
#include "errors.h"
#include "memory.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ==================== Static Built-in Types ==================== */

static AstType type_int = { AST_TYPE_INT, { NULL, 0, NULL } };
static AstType type_double = { AST_TYPE_DOUBLE, { NULL, 0, NULL } };
static AstType type_string = { AST_TYPE_STRING, { NULL, 0, NULL } };
static AstType type_boolean = { AST_TYPE_BOOLEAN, { NULL, 0, NULL } };
static AstType type_unit = { AST_TYPE_UNIT, { NULL, 0, NULL } };
static AstType type_any = { AST_TYPE_ANY, { NULL, 0, NULL } };

/* ==================== Helper Functions ==================== */

/**
 * Allocates and populates an array of parameter types for built-in functions.
 */
static AstType **_alloc_params(size_t count, ...) {
    if (count == 0) {
        return NULL;
    }
    AstType **params = (AstType **)safe_malloc(sizeof(AstType *) * count);
    va_list args;
    va_start(args, count);
    for (size_t i = 0; i < count; i++) {
        params[i] = va_arg(args, AstType *);
    }
    va_end(args);
    return params;
}

/**
 * Registers all IFJ26 built-in functions into the global scope.
 */
static void _register_builtins(Symtable *table) {
    symtable_insert_function(table, "print", &type_unit, _alloc_params(1, &type_any), 1, true);
    symtable_insert_function(table, "readLine", &type_string, NULL, 0, true);
    symtable_insert_function(table, "readInt", &type_int, NULL, 0, true);
    symtable_insert_function(table, "readDouble", &type_double, NULL, 0, true);
    symtable_insert_function(table, "toStr", &type_string, _alloc_params(1, &type_any), 1, true);
    symtable_insert_function(table, "toInt", &type_int, _alloc_params(1, &type_double), 1, true);
    symtable_insert_function(table, "strLength", &type_int, _alloc_params(1, &type_string), 1, true);
    symtable_insert_function(table, "subStr", &type_string, _alloc_params(3, &type_string, &type_int, &type_int), 3, true);
    symtable_insert_function(table, "ord", &type_int, _alloc_params(2, &type_string, &type_int), 2, true);
    symtable_insert_function(table, "chr", &type_string, _alloc_params(1, &type_int), 1, true);
    symtable_insert_function(table, "readBoolean", &type_boolean, NULL, 0, true);
}

/**
 * Calculates total number of parameters across all parameter lists.
 */
static size_t _count_function_params(const AstFunction *func) {
    size_t total = 0;
    for (size_t i = 0; i < func->parameters.count; i++) {
        total += func->parameters.items[i].count;
    }
    return total;
}

/**
 * Extracts parameter types from a function into a flat array.
 */
static AstType **_extract_param_types(const AstFunction *func, size_t *out_count) {
    size_t total = _count_function_params(func);
    *out_count = total;
    if (total == 0) {
        return NULL;
    }
    AstType **types = (AstType **)safe_malloc(sizeof(AstType *) * total);
    size_t idx = 0;
    for (size_t i = 0; i < func->parameters.count; i++) {
        for (size_t j = 0; j < func->parameters.items[i].count; j++) {
            types[idx++] = func->parameters.items[i].items[j].type;
        }
    }
    return types;
}

/**
 * Checks whether any parameter names are duplicated within the function declaration.
 */
static bool _has_duplicate_parameters(const AstFunction *func) {
    size_t total = _count_function_params(func);
    if (total <= 1) {
        return false;
    }

    for (size_t i1 = 0; i1 < func->parameters.count; i1++) {
        for (size_t j1 = 0; j1 < func->parameters.items[i1].count; j1++) {
            const char *name1 = func->parameters.items[i1].items[j1].name;
            for (size_t i2 = i1; i2 < func->parameters.count; i2++) {
                size_t start_j2 = (i2 == i1) ? (j1 + 1) : 0;
                for (size_t j2 = start_j2; j2 < func->parameters.items[i2].count; j2++) {
                    const char *name2 = func->parameters.items[i2].items[j2].name;
                    if (name1 && name2 && strcmp(name1, name2) == 0) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/* ==================== Pass 1 Implementation ==================== */

int semantic_pass1(AstProgram *program, Symtable *table) {
    if (!program || !table) {
        return ERR_INTERNAL;
    }

    _register_builtins(table);

    size_t main_count = 0;

    for (size_t i = 0; i < program->count; i++) {
        AstDefinition *def = &program->items[i];
        if (def->kind != AST_DEF_FUNCTION) {
            continue;
        }

        AstFunction *func = def->data.function;
        if (!func || !func->name) {
            continue;
        }

        // Redefinition check
        if (symtable_lookup(table, func->name) != NULL) {
            error_print(ERR_SEM_REDEFINITION, func->pos, "Redefinition of function '%s'", func->name);
            return ERR_SEM_REDEFINITION;
        }

        // Check for duplicate parameter names
        if (_has_duplicate_parameters(func)) {
            error_print(ERR_SEM_REDEFINITION, func->pos, "Duplicate parameter name in function '%s'", func->name);
            return ERR_SEM_REDEFINITION;
        }

        // Validate @main entry function
        if (func->is_main) {
            size_t param_cnt = _count_function_params(func);
            if (param_cnt > 0) {
                error_print(ERR_SEM_CALL, func->pos,
                            "Entry function '@main' must take 0 parameters, but got %zu", param_cnt);
                return ERR_SEM_CALL;
            }

            if (func->return_type != NULL && func->return_type->kind != AST_TYPE_UNIT) {
                error_print(ERR_SEM_TYPE, func->pos,
                            "Entry function '@main' must have return type Unit, but got '%s'",
                            ast_type_to_string(func->return_type->kind));
                return ERR_SEM_TYPE;
            }

            // If return type is not explicitly written, default it to Unit
            if (func->return_type == NULL) {
                func->return_type = &type_unit;
            }

            main_count++;
            if (main_count > 1) {
                error_print(ERR_SEM_REDEFINITION, func->pos, "Multiple '@main' entry functions defined");
                return ERR_SEM_REDEFINITION;
            }
        }

        // Extract parameter types and register function in global scope
        size_t param_count = 0;
        AstType **param_types = _extract_param_types(func, &param_count);

        if (!symtable_insert_function(table, func->name, func->return_type, param_types, param_count, false)) {
            if (param_types) {
                free(param_types);
            }
            return ERR_INTERNAL;
        }
    }

    // Verify presence of @main entry function
    if (main_count == 0) {
        error_print_no_pos(ERR_SEM_UNDEFINED, "Missing entry function with '@main' annotation");
        return ERR_SEM_UNDEFINED;
    }

    return ERR_OK;
}

int analyze_semantics(AstProgram *program) {
    if (!program) {
        return ERR_INTERNAL;
    }

    Symtable *table = symtable_create();

    int result = semantic_pass1(program, table);
    if (result != ERR_OK) {
        symtable_destroy(table);
        return result;
    }

    // Pass 2 will be called here

    symtable_destroy(table);
    return ERR_OK;
}
