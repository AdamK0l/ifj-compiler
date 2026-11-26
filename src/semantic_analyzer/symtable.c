/**
 * Project: IFJ26 / IAL compiler
 * File: symtable.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "symtable.h"
#include "memory.h"
#include "bst.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Allocates and initializes the symbol table and pushes the initial global scope.
 *
 * @return Pointer to initialized Symtable.
 */
Symtable *symtable_create(void) {
    Symtable *table = (Symtable *)safe_malloc(sizeof(Symtable));
    table->current_scope = NULL;
    table->global_scope = NULL;
    table->extensions.items = NULL;
    table->extensions.count = 0;
    table->extensions.capacity = 0;
    // Push the initial global scope
    symtable_push_scope(table);
    table->global_scope = table->current_scope;
    
    return table;
}

/**
 * @brief Pushes a new empty scope onto the scope stack when entering a block.
 *
 * @param table Pointer to Symtable.
 */
void symtable_push_scope(Symtable *table) {
    if (!table) {
        return;
    }

    Scope *new_scope = (Scope *)safe_malloc(sizeof(Scope));
    new_scope->tree_root = NULL;
    new_scope->parent = table->current_scope;
    table->current_scope = new_scope;
}

/**
 * @brief Pops and destroys the innermost scope when exiting a block.
 * Reclaims the AVL tree of local symbols and unlinks from the scope stack.
 *
 * @param table Pointer to Symtable.
 */
void symtable_pop_scope(Symtable *table) {
    if (!table || !table->current_scope) {
        return;
    }

    Scope *old_scope = table->current_scope;
    table->current_scope = old_scope->parent;

    bst_free(old_scope->tree_root);
    free(old_scope);
}

/**
 * @brief Completely destroys the symbol table, popping all remaining scopes and freeing memory.
 *
 * @param table Pointer to Symtable.
 */
void symtable_destroy(Symtable *table) {
    if (!table) {
        return;
    }

    while (table->current_scope != NULL) {
        symtable_pop_scope(table);
    }

    if (table->extensions.items != NULL) {
        free(table->extensions.items);
    }
    free(table);
}

/**
 * @brief Looks up a symbol by name across active scopes, starting from current to global.
 * Traverses parent pointers from innermost to outermost scope, implementing variable shadowing.
 *
 * @param table Pointer to Symtable.
 * @param name Identifier name to search for.
 * @return BstNode pointer of first matching symbol found, or NULL if undefined.
 */
BstNode *symtable_lookup(Symtable *table, const char *name) {
    if (!table || !name) {
        return NULL;
    }
    Scope *scope = table->current_scope;
    while (scope != NULL) {
        BstNode *node = bst_search(scope->tree_root, name);
        if (node != NULL) {
            return node;
        }
        scope = scope->parent;
    }
    return NULL;
}

/**
 * @brief Looks up a symbol ONLY in the innermost (current) lexical scope.
 * Used to detect illegal redefinitions within the same block (Error 4).
 *
 * @param table Pointer to Symtable.
 * @param name Identifier name to search for.
 * @return BstNode pointer if defined in current scope, NULL otherwise.
 */
BstNode *symtable_lookup_current_scope(Symtable *table, const char *name) {
    if (!table || !table->current_scope || !name) {
        return NULL;
    }
    return bst_search(table->current_scope->tree_root, name);
}

/**
 * @brief Inserts a variable into the currently active innermost scope.
 *
 * @param table Pointer to Symtable.
 * @param name Variable identifier name.
 * @param type Evaluated or declared type.
 * @param is_changeable True for 'var', false for 'val' and function parameters.
 * @return True on success, false if duplicate in current scope or on failure.
 */
bool symtable_insert_variable(Symtable *table, const char *name, AstType *type, bool is_changeable) {
    if (!table || !table->current_scope || !name) {
        return false;
    }
    SymbolData data;
    data.kind = SYMBOL_VARIABLE;
    data.type = type;
    data.is_changeable = is_changeable;
    data.param_types = NULL;
    data.param_count = 0;
    data.is_builtin = false;
    BstNode *inserted = bst_insert(&(table->current_scope->tree_root), name, data);
    return (inserted != NULL);

}

/**
 * @brief Inserts a function signature into the global scope.
 * Always places functions in global_scope, regardless of currently active scope.
 *
 * @param table Pointer to Symtable.
 * @param name Function identifier name.
 * @param return_type Declared or inferred return type.
 * @param param_types Array of parameter types.
 * @param param_count Number of parameters.
 * @param is_builtin True if standard library built-in function.
 * @return True on success, false if duplicate function name.
 */
bool symtable_insert_function(Symtable *table, const char *name, AstType *return_type, AstType **param_types, size_t param_count, bool is_builtin) {
    if (!table || !table->global_scope || !name) {
        return false;
    }
    SymbolData data;
    data.kind = SYMBOL_FUNCTION;
    data.type = return_type;
    data.is_changeable = false;
    data.param_types = param_types;
    data.param_count = param_count;
    data.is_builtin = is_builtin;
    BstNode *inserted = bst_insert(&(table->global_scope->tree_root), name, data);
    return (inserted != NULL);
}

/**
 * @brief Inserts an extension method into the extension methods table.
 * Enforces redefinition rules: returns false if a method with the same name already exists
 * for the exact same receiver type (Error 4).
 *
 * @param table Pointer to Symtable.
 * @param receiver_type Type of the receiver (e.g. Int, Double).
 * @param receiver_name Identifier name of the receiver.
 * @param function Pointer to the method's AstFunction.
 * @param ext Pointer to enclosing AstExtension.
 * @return True on success, false on redefinition.
 */
bool symtable_insert_extension(Symtable *table, AstType *receiver_type, const char *receiver_name, AstFunction *function, const AstExtension *ext) {
    if (!table || !receiver_type || !function || !function->name) {
        return false;
    }

    // Check if an extension method with the same name already exists for the same receiver type
    for (size_t i = 0; i < table->extensions.count; i++) {
        ExtensionMethod *m = &table->extensions.items[i];
        if (strcmp(m->function->name, function->name) == 0 &&
            ast_type_equals(m->receiver_type, receiver_type)) {
            return false; // Redefinition for the same receiver type!
        }
    }

    if (table->extensions.count >= table->extensions.capacity) {
        size_t new_cap = table->extensions.capacity == 0 ? 8 : table->extensions.capacity * 2;
        table->extensions.items = (ExtensionMethod *)safe_realloc(
            table->extensions.items, new_cap * sizeof(ExtensionMethod)
        );
        table->extensions.capacity = new_cap;
    }

    ExtensionMethod *item = &table->extensions.items[table->extensions.count++];
    item->receiver_type = receiver_type;
    item->receiver_name = receiver_name;
    item->function = function;
    item->extension = ext;
    return true;
}

/**
 * @brief Looks up an extension method by receiver type and method name.
 * Supports receiver polymorphism: matches both receiver_type and method_name.
 *
 * @param table Pointer to Symtable.
 * @param receiver_type Evaluated type of the expression before the dot.
 * @param method_name Name of the method being called.
 * @return Pointer to AstFunction if found, NULL otherwise.
 */
AstFunction *symtable_lookup_extension(Symtable *table, const AstType *receiver_type, const char *method_name) {
    if (!table || !receiver_type || !method_name) {
        return NULL;
    }

    for (size_t i = 0; i < table->extensions.count; i++) {
        ExtensionMethod *m = &table->extensions.items[i];
        if (strcmp(m->function->name, method_name) == 0 &&
            ast_type_equals(m->receiver_type, receiver_type)) {
            return m->function;
        }
    }
    return NULL;
}