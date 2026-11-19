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

Symtable *symtable_create(void) {
    Symtable *table = (Symtable *)safe_malloc(sizeof(Symtable));
    table->current_scope = NULL;
    table->global_scope = NULL;
    // Push the initial global scope
    symtable_push_scope(table);
    table->global_scope = table->current_scope;
    
    return table;
}

void symtable_push_scope(Symtable *table) {
    if (!table) {
        return;
    }

    Scope *new_scope = (Scope *)safe_malloc(sizeof(Scope));
    new_scope->tree_root = NULL;
    new_scope->parent = table->current_scope;
    table->current_scope = new_scope;
}

void symtable_pop_scope(Symtable *table) {
    if (!table || !table->current_scope) {
        return;
    }

    Scope *old_scope = table->current_scope;
    table->current_scope = old_scope->parent;

    bst_free(old_scope->tree_root);
    free(old_scope);
}

void symtable_destroy(Symtable *table) {
    if (!table) {
        return;
    }

    while (table->current_scope != NULL) {
        symtable_pop_scope(table);
    }

    free(table);
}

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

BstNode *symtable_lookup_current_scope(Symtable *table, const char *name) {
    if (!table || !table->current_scope || !name) {
        return NULL;
    }
    return bst_search(table->current_scope->tree_root, name);
}

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