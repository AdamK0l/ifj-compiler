/**
 * Project: IFJ26 / IAL compiler
 * File: bst.h
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#ifndef BST_H
#define BST_H

#include "ast.h"
#include <stdbool.h>
#include <stddef.h>


typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION
} SymbolKind;

typedef struct SymbolData {
    SymbolKind kind;        // Variable or function
    AstType *type;          // Declared type or return type for functions

    bool is_changeable;     // True for var false for val

    AstType **param_types;  // Array of parameter types
    size_t param_count;     // Total count of all parameters
    bool is_builtin;        // True if function is built-in 
} SymbolData;

typedef struct BstNode {
    char *key;              // Unique identifier name
    SymbolData data;        // Semantic metadata
    struct BstNode *left;   // Pointer to left child
    struct BstNode *right;  // Pointer to right child
    int height;             // Height of the subtree
} BstNode;

/**
 * @brief Searches for a symbol by key in the AVL tree.
 *
 * @param root Pointer to root of the (sub)tree.
 * @param key Identifier name to find.
 * @return Pointer to the found BstNode, or NULL if not found.
 */
BstNode *bst_search(BstNode *root, const char *key);

/**
 * @brief Inserts a new symbol into the AVL tree, performing AVL balancing if needed.
 *
 * @param root Double pointer to the root of the tree (may be updated due to rotations).
 * @param key Unique identifier name to insert.
 * @param data Associated symbol data.
 * @return Pointer to the newly inserted node, or NULL on duplication or failure.
 */
BstNode *bst_insert(BstNode **root, const char *key, SymbolData data);

/**
 * @brief Recursively deallocates the entire subtree (post-order traversal).
 * Frees key strings, dynamic parameter arrays, and node memory.
 *
 * @param root Root of the tree to destroy.
 */
void bst_free(BstNode *root);

#endif // BST_H