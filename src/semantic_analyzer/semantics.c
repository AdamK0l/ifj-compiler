/**
 * Project: IFJ26 / IAL compiler
 * File: semantics.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "semantics.h"
#include "errors.h"
#include "memory.h"
#include "type_checker.h"
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
 * @brief Allocates and populates an array of parameter types for built-in functions.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a helper function to allocate and fill an array of AstType pointers"
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
 * @brief Inserts built-in IFJ26 functions into global scope.
 * 
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini 
 * Prompt: "Write a function, that inserts all IFJ26 built-in functions into the symtable"
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
 * @brief Calculates total number of parameters across all parameter lists.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a helper function to count total parameters across nested parameter lists in an AstFunction, so that I can use it when allocating memory"
 */
static size_t _count_function_params(const AstFunction *func) {
    size_t total = 0;
    for (size_t i = 0; i < func->parameters.count; i++) {
        total += func->parameters.items[i].count;
    }
    return total;
}

/**
 * @brief Extracts parameter types from a function into a flat array for symtable registration.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a helper function, that extracts types from AstParameterLists into an array"
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
 * @brief Checks whether any parameter names are duplicated within the function declaration.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a function that checks for duplicate parameter names across all parameter lists of an AstFunction"
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
        if (def->kind == AST_DEF_EXTENSION) {
            AstExtension *ext = def->data.extension;
            if (!ext || !ext->receiver.type || !ext->receiver.name) {
                continue;
            }

            for (size_t j = 0; j < ext->method_count; j++) {
                AstFunction *method = ext->methods[j];
                if (!method || !method->name) {
                    continue;
                }

                // Check duplicate parameters in method
                if (_has_duplicate_parameters(method)) {
                    error_print(ERR_SEM_REDEFINITION, method->pos,
                                "Duplicate parameter name in extension method '%s'", method->name);
                    return ERR_SEM_REDEFINITION;
                }

                // Check if any parameter name collides with receiver name
                for (size_t list_idx = 0; list_idx < method->parameters.count; list_idx++) {
                    AstParameterList *plist = &method->parameters.items[list_idx];
                    for (size_t p = 0; p < plist->count; p++) {
                        if (strcmp(plist->items[p].name, ext->receiver.name) == 0) {
                            error_print(ERR_SEM_REDEFINITION, method->pos,
                                        "Parameter '%s' collides with extension receiver name in method '%s'",
                                        plist->items[p].name, method->name);
                            return ERR_SEM_REDEFINITION;
                        }
                    }
                }

                // Extension method cannot be annotated with @main
                if (method->is_main) {
                    error_print(ERR_SEM_REDEFINITION, method->pos,
                                "Extension method '%s' cannot be annotated with @main", method->name);
                    return ERR_SEM_REDEFINITION;
                }

                // Register extension method (checks redefinition for the same receiver type -> Error 4)
                if (!symtable_insert_extension(table, ext->receiver.type, ext->receiver.name, method, ext)) {
                    error_print(ERR_SEM_REDEFINITION, method->pos,
                                "Redefinition of extension method '%s' for type '%s'",
                                method->name, ast_type_to_string(ext->receiver.type->kind));
                    return ERR_SEM_REDEFINITION;
                }
            }
            continue;
        }

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

/* ==================== Pass 2 Implementation ==================== */

/**
 * @brief Type checks an indented block of statements and evaluates its resulting type from the last statement.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a check_block function that iterates through statements, validates each, and evaluates block return type from the last statement"
 */
int check_block(AstBLOCK *block, Symtable *table, AstType *enclosing_return_type, AstType **out_block_type) {
    if (!block) {
        if (out_block_type) {
            *out_block_type = type_get_unit();
        }
        return ERR_OK;
    }

    AstType *last_type = type_get_unit();

    for (size_t i = 0; i < block->statements.count; i++) {
        AstStatement *stmt = block->statements.items[i];
        if (!stmt) continue;

        AstType *stmt_type = NULL;
        int res = check_statement(stmt, table, enclosing_return_type, &stmt_type);
        if (res != ERR_OK) {
            return res;
        }

        if (stmt_type != NULL) {
            last_type = stmt_type;
        } else {
            last_type = type_get_unit();
        }
    }

    if (out_block_type) {
        *out_block_type = last_type;
    }
    return ERR_OK;
}

int check_statement(AstStatement *stmt, Symtable *table, AstType *enclosing_return_type, AstType **out_type) {
    if (!stmt || !table) {
        return ERR_INTERNAL;
    }

    switch (stmt->kind) {
        case AST_STATEMENT_VARIABLE: {
            // Spec section 5.7: Named functions can only be assigned to variables with an explicitly declared type
            if (stmt->data.variable.declared_type == NULL &&
                stmt->data.variable.initializer &&
                stmt->data.variable.initializer->kind == AST_EXPR_NAME) {
                BstNode *fn_node = symtable_lookup(table, stmt->data.variable.initializer->data.name);
                if (fn_node && fn_node->data.kind == SYMBOL_FUNCTION) {
                    error_print(ERR_SEM_TYPE, stmt->pos,
                                "Assigning function '%s' to variable '%s' requires an explicit variable type",
                                stmt->data.variable.initializer->data.name,
                                stmt->data.variable.name);
                    return ERR_SEM_TYPE;
                }
            }

            AstType *init_type = NULL;
            int res = check_expression(stmt->data.variable.initializer, table, &init_type);
            if (res != ERR_OK) {
                return res;
            }

            AstType *var_type = stmt->data.variable.declared_type;
            if (var_type != NULL) {
                if (!type_is_compatible(var_type, init_type)) {
                    error_print(ERR_SEM_TYPE, stmt->pos,
                                "Type mismatch in variable '%s' definition: declared '%s', but initialized with '%s'",
                                stmt->data.variable.name,
                                ast_type_to_string(var_type->kind),
                                ast_type_to_string(init_type->kind));
                    return ERR_SEM_TYPE;
                }
            } else {
                var_type = init_type;
                stmt->data.variable.declared_type = ast_type_clone(init_type);
            }

            // Check redefinition in the current scope
            if (symtable_lookup_current_scope(table, stmt->data.variable.name) != NULL) {
                error_print(ERR_SEM_REDEFINITION, stmt->pos,
                            "Redefinition of variable '%s' in the same scope",
                            stmt->data.variable.name);
                return ERR_SEM_REDEFINITION;
            }

            if (!symtable_insert_variable(table, stmt->data.variable.name, var_type, stmt->data.variable.is_changeable)) {
                return ERR_INTERNAL;
            }

            if (out_type) *out_type = type_get_unit();
            return ERR_OK;
        }

        case AST_STATEMENT_ASSIGN: {
            BstNode *var_node = symtable_lookup(table, stmt->data.assignment.name);
            if (!var_node || var_node->data.kind != SYMBOL_VARIABLE) {
                error_print(ERR_SEM_UNDEFINED, stmt->pos,
                            "Assignment to undefined variable '%s'",
                            stmt->data.assignment.name);
                return ERR_SEM_UNDEFINED;
            }

            // Check mutability: cannot assign to val or function parameter
            if (!var_node->data.is_changeable) {
                error_print(ERR_SEM_MUTABILITY, stmt->pos,
                            "Cannot assign to immutable variable or parameter '%s'",
                            stmt->data.assignment.name);
                return ERR_SEM_MUTABILITY;
            }

            AstType *val_type = NULL;
            int res = check_expression(stmt->data.assignment.value, table, &val_type);
            if (res != ERR_OK) {
                return res;
            }

            if (!type_is_compatible(var_node->data.type, val_type)) {
                error_print(ERR_SEM_TYPE, stmt->pos,
                            "Type mismatch in assignment to '%s': variable is '%s', assigned value is '%s'",
                            stmt->data.assignment.name,
                            ast_type_to_string(var_node->data.type->kind),
                            ast_type_to_string(val_type->kind));
                return ERR_SEM_TYPE;
            }

            if (out_type) *out_type = type_get_unit();
            return ERR_OK;
        }

        case AST_STATEMENT_EXPRESSION: {
            return check_expression(stmt->data.expression, table, out_type);
        }

        case AST_STATEMENT_WHILE: {
            AstType *cond_type = NULL;
            int res = check_expression(stmt->data.while_loop.condition, table, &cond_type);
            if (res != ERR_OK) {
                return res;
            }

            if (cond_type->kind != AST_TYPE_BOOLEAN) {
                error_print(ERR_SEM_TYPE, stmt->pos,
                            "Condition in while loop must be Boolean, but got '%s'",
                            ast_type_to_string(cond_type->kind));
                return ERR_SEM_TYPE;
            }

            symtable_push_scope(table);
            res = check_block(stmt->data.while_loop.inside, table, enclosing_return_type, NULL);
            symtable_pop_scope(table);
            if (res != ERR_OK) {
                return res;
            }

            if (out_type) *out_type = type_get_unit();
            return ERR_OK;
        }

        case AST_STATEMENT_RETURN: {
            if (!table->in_function || table->in_lambda) {
                error_print(ERR_SEM_OTHER, stmt->pos, "Return statement outside function body");
                return ERR_SEM_OTHER;
            }

            AstType *ret_type = NULL;
            if (stmt->data.return_value != NULL) {
                int res = check_expression(stmt->data.return_value, table, &ret_type);
                if (res != ERR_OK) {
                    return res;
                }
            } else {
                ret_type = type_get_unit();
            }

            AstType *expected_ret = enclosing_return_type ? enclosing_return_type : table->enclosing_return_type;
            if (!expected_ret) {
                error_print(ERR_SEM_TYPE, stmt->pos,
                            "Function containing 'return' must have an explicit return type in its header");
                return ERR_SEM_TYPE;
            }

            // Spec section 2.3 rule 4: In a function with explicit Unit return type,
            // the returned value is evaluated and discarded.
            if (expected_ret->kind != AST_TYPE_UNIT &&
                !type_is_compatible(expected_ret, ret_type)) {
                error_print(ERR_SEM_TYPE, stmt->pos,
                            "Return value type mismatch: function expects '%s', but returned '%s'",
                            ast_type_to_string(expected_ret->kind),
                            ast_type_to_string(ret_type->kind));
                return ERR_SEM_TYPE;
            }

            if (out_type) *out_type = ret_type;
            return ERR_OK;
        }

        case AST_STATEMENT_FOR: {
            // Check zero step constraint (Error 10)
            if (stmt->data.for_loop.step != NULL &&
                stmt->data.for_loop.step->kind == AST_EXPR_INT &&
                stmt->data.for_loop.step->data.int_value == 0) {
                error_print(ERR_SEM_OTHER, stmt->pos, "For loop step cannot be 0");
                return ERR_SEM_OTHER;
            }

            AstType *from_type = NULL;
            int res = check_expression(stmt->data.for_loop.from, table, &from_type);
            if (res != ERR_OK) return res;

            AstType *to_type = NULL;
            res = check_expression(stmt->data.for_loop.to, table, &to_type);
            if (res != ERR_OK) return res;

            if (stmt->data.for_loop.step != NULL) {
                AstType *step_type = NULL;
                res = check_expression(stmt->data.for_loop.step, table, &step_type);
                if (res != ERR_OK) return res;
            }

            // Enter loop scope with induction variable
            symtable_push_scope(table);
            symtable_insert_variable(table, stmt->data.for_loop.variable_name, type_get_int(), false);

            // Check guards
            for (size_t g = 0; g < stmt->data.for_loop.guards.count; g++) {
                AstType *guard_type = NULL;
                res = check_expression(stmt->data.for_loop.guards.items[g], table, &guard_type);
                if (res != ERR_OK) {
                    symtable_pop_scope(table);
                    return res;
                }
                if (guard_type->kind != AST_TYPE_BOOLEAN) {
                    error_print(ERR_SEM_TYPE, stmt->pos, "For loop guard must be Boolean");
                    symtable_pop_scope(table);
                    return ERR_SEM_TYPE;
                }
            }

            res = check_block(stmt->data.for_loop.inside, table, enclosing_return_type, NULL);
            symtable_pop_scope(table);
            if (res != ERR_OK) return res;

            if (out_type) *out_type = type_get_unit();
            return ERR_OK;
        }

        default:
            error_print(ERR_SEM_OTHER, stmt->pos, "Unsupported statement kind %d", stmt->kind);
            return ERR_SEM_OTHER;
    }
}

int semantic_pass2(AstProgram *program, Symtable *table) {
    if (!program || !table) {
        return ERR_INTERNAL;
    }

    // Pass 2a: Check all extension method bodies
    for (size_t i = 0; i < program->count; i++) {
        AstDefinition *def = &program->items[i];
        if (def->kind != AST_DEF_EXTENSION) {
            continue;
        }

        AstExtension *ext = def->data.extension;
        if (!ext) continue;

        for (size_t j = 0; j < ext->method_count; j++) {
            AstFunction *method = ext->methods[j];
            if (!method) continue;

            symtable_push_scope(table);

            // Insert receiver into local scope as immutable (val) variable
            symtable_insert_variable(table, ext->receiver.name, ext->receiver.type, false);

            // Insert method parameters into local scope as immutable (val) variables
            for (size_t list_idx = 0; list_idx < method->parameters.count; list_idx++) {
                AstParameterList *plist = &method->parameters.items[list_idx];
                for (size_t p = 0; p < plist->count; p++) {
                    AstParameter *param = &plist->items[p];
                    symtable_insert_variable(table, param->name, param->type, false);
                }
            }

            // Check method body block
            table->in_function = true;
            table->enclosing_return_type = method->return_type;
            table->in_lambda = false;

            AstType *body_return_type = NULL;
            int res = check_block(method->inside, table, method->return_type, &body_return_type);

            table->in_function = false;
            table->enclosing_return_type = NULL;

            if (res != ERR_OK) {
                symtable_pop_scope(table);
                return res;
            }

            // Validate return type against body
            if (method->return_type != NULL) {
                if (method->return_type->kind != AST_TYPE_UNIT &&
                    !type_is_compatible(method->return_type, body_return_type)) {
                    error_print(ERR_SEM_TYPE, method->pos,
                                "Extension method '%s' declared return type '%s', but body evaluates to '%s'",
                                method->name,
                                ast_type_to_string(method->return_type->kind),
                                ast_type_to_string(body_return_type->kind));
                    symtable_pop_scope(table);
                    return ERR_SEM_TYPE;
                }
            } else {
                // Infer return type from body
                method->return_type = ast_type_clone(body_return_type);
            }

            symtable_pop_scope(table);
        }
    }

    // Pass 2b: Check all top-level user function bodies
    for (size_t i = 0; i < program->count; i++) {
        AstDefinition *def = &program->items[i];
        if (def->kind != AST_DEF_FUNCTION) {
            continue;
        }

        AstFunction *func = def->data.function;
        if (!func) continue;

        // Open local scope for function body and its parameters
        symtable_push_scope(table);

        // Insert parameters into local scope as immutable (val) variables
        for (size_t list_idx = 0; list_idx < func->parameters.count; list_idx++) {
            AstParameterList *plist = &func->parameters.items[list_idx];
            for (size_t p = 0; p < plist->count; p++) {
                AstParameter *param = &plist->items[p];
                symtable_insert_variable(table, param->name, param->type, false);
            }
        }

        // Check function body block
        table->in_function = true;
        table->enclosing_return_type = func->return_type;
        table->in_lambda = false;

        AstType *body_return_type = NULL;
        int res = check_block(func->inside, table, func->return_type, &body_return_type);

        table->in_function = false;
        table->enclosing_return_type = NULL;

        if (res != ERR_OK) {
            symtable_pop_scope(table);
            return res;
        }

        // Validate return type against body
        if (func->return_type != NULL) {
            // Function has explicit return type
            // Spec section 2.3 rule 4: In an explicit Unit function, body value is discarded.
            if (func->return_type->kind != AST_TYPE_UNIT &&
                !type_is_compatible(func->return_type, body_return_type)) {
                error_print(ERR_SEM_TYPE, func->pos,
                            "Function '%s' declared return type '%s', but body evaluates to '%s'",
                            func->name,
                            ast_type_to_string(func->return_type->kind),
                            ast_type_to_string(body_return_type->kind));
                symtable_pop_scope(table);
                return ERR_SEM_TYPE;
            }
        } else {
            // Return type was omitted, infer it from the body!
            func->return_type = ast_type_clone(body_return_type);
            BstNode *fn_node = symtable_lookup(table, func->name);
            if (fn_node) {
                fn_node->data.type = func->return_type;
            }
        }

        symtable_pop_scope(table);
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

    result = semantic_pass2(program, table);
    if (result != ERR_OK) {
        symtable_destroy(table);
        return result;
    }

    symtable_destroy(table);
    return ERR_OK;
}
