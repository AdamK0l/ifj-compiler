/**
 * Project: IFJ26 / IAL compiler
 * File: symtable.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "symtable.h"
#include "memory.h"
#include <stdlib.h>

Symtable *symtable_create(void) {
    Symtable *table = (Symtable *)safe_malloc(sizeof(Symtable));
    table->current_scope = NULL;
    // Push the initial global scope
    symtable_push_scope(table);
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

    // Note: When BST is implemented, old_scope->tree_root will be disposed here via bst_free().
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
