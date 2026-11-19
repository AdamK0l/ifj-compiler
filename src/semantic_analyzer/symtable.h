/**
 * Project: IFJ26 / IAL compiler
 * File: symtable.h
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "bst.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * Scope structure representing a single lexical block.
 * Forms a singly linked list (stack) from innermost to outermost scope.
 */
typedef struct Scope {
    BstNode *tree_root;       // Root of the BST containing symbols in this scope
    struct Scope *parent;     // Pointer to the enclosing (parent) scope, or NULL for global
} Scope;

/**
 * Symbol table structure managing the hierarchy of active scopes.
 */
typedef struct Symtable {
    Scope *current_scope;     // Pointer to the currently active innermost scope
    Scope *global_scope;
} Symtable;

/**
 * Creates and initializes a new symbol table with an initial global scope.
 *
 * @return Pointer to allocated Symtable, or terminates via safe_malloc on failure.
 */
Symtable *symtable_create(void);

/**
 * Enters a new block by pushing a new empty scope onto the scope stack.
 *
 * @param table Pointer to Symtable.
 */
void symtable_push_scope(Symtable *table);

/**
 * Exits the current block by popping the innermost scope from the scope stack.
 *
 * @param table Pointer to Symtable.
 */
void symtable_pop_scope(Symtable *table);

/**
 * Destroys the symbol table, popping and freeing all remaining scopes.
 *
 * @param table Pointer to Symtable.
 */
void symtable_destroy(Symtable *table);

BstNode *symtable_lookup(Symtable *table, const char *name);

BstNode *symtable_lookup_current_scope(Symtable *table, const char *name);

bool symtable_insert_variable(Symtable *table, const char *name, AstType *type, bool is_changeable);

bool symtable_insert_function(Symtable *table, const char *name, AstType *return_type, AstType **param_types, size_t param_count, bool is_builtin);
#endif // SYMTABLE_H
