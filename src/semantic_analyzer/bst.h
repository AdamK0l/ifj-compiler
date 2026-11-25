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
    SymbolKind kind;
    AstType *type;

    bool is_changeable;

    AstType **param_types;
    size_t param_count;
    bool is_builtin;
} SymbolData;

typedef struct BstNode {
    char *key;
    SymbolData data;
    struct BstNode *left;
    struct BstNode *right;
    int height;
} BstNode;

BstNode *bst_search(BstNode *root, const char *key);

BstNode *bst_insert(BstNode **root, const char *key, SymbolData data);

void bst_free(BstNode *root);

#endif // BST_H