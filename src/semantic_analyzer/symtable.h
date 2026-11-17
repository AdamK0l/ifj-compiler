/**
 * Project: IFJ26 / IAL compiler
 * File: symtable.h
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Forward declaration of the BST node structure (implemented as part of IAL).
 */
typedef struct BstNode BstNode;

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

#endif // SYMTABLE_H
