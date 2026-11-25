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
 * Extension method entry storing metadata for user extension methods.
 */
typedef struct ExtensionMethod {
    AstType *receiver_type;
    const char *receiver_name;
    AstFunction *function;
    const AstExtension *extension;
} ExtensionMethod;

/**
 * Dynamic table of extension methods.
 */
typedef struct ExtensionTable {
    ExtensionMethod *items;
    size_t count;
    size_t capacity;
} ExtensionTable;

/**
 * Symbol table structure managing the hierarchy of active scopes and registered extensions.
 */
typedef struct Symtable {
    Scope *current_scope;     // Pointer to the currently active innermost scope
    Scope *global_scope;
    ExtensionTable extensions; // Registered extension methods
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

/**
 * Inserts an extension method into the symbol table.
 * Returns false if an extension method with the same name already exists for the same receiver type.
 */
bool symtable_insert_extension(Symtable *table, AstType *receiver_type, const char *receiver_name, AstFunction *function, const AstExtension *ext);

/**
 * Looks up an extension method by receiver type and method name.
 */
AstFunction *symtable_lookup_extension(Symtable *table, const AstType *receiver_type, const char *method_name);
#endif // SYMTABLE_H
