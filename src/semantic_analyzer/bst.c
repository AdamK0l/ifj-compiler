/**
 * Project: IFJ26 / IAL compiler
 * File: bst.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "bst.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>


static int _node_height(BstNode *node) {
    return (node != NULL) ? node->height : 0;
}

static int _max(int a, int b) {
    return (a > b) ? a : b;
}

static void _update_height(BstNode *node) {
    if (node != NULL) {
        node->height = 1 + _max(_node_height(node->left), _node_height(node->right));
    }
}

static int _balance_factor(BstNode *node) {
    return (node != NULL) ? (_node_height(node->left) - _node_height(node->right)) : 0;
}

static BstNode *_create_node(const char *key, SymbolData data) {
    BstNode *node = (BstNode *)safe_malloc(sizeof(BstNode));
    node->key = safe_strdup(key);
    node->data = data;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    return node;
}

static BstNode *_rotate_right(BstNode *y){
    BstNode *x = y->left;
    BstNode *t2 = x->right;

    x->right = y;
    y->left = t2;

    _update_height(y);
    _update_height(x);

    return x;
}

static BstNode *_rotate_left(BstNode *x) {
    BstNode *y = x->right;
    BstNode *t2 = y->left;

    y->left = x;
    x->right = t2;

    _update_height(x);
    _update_height(y);

    return y;
}

BstNode *bst_search(BstNode *root, const char *key) {
    if (root == NULL) {
        return NULL;
    }

    int cmp = strcmp(key, root->key);

    if (cmp == 0) {
        return root;
    }
    else if (cmp < 0) {
        return bst_search (root->left, key);
    }
    else {
        return bst_search (root->right, key);
    }
}

void bst_free(BstNode *root) {
    if (root == NULL) {
        return;
    }

    bst_free(root->left);
    bst_free(root->right);

    free(root->key);

    if (root->data.kind == SYMBOL_FUNCTION && root->data.param_types != NULL) {
        free(root->data.param_types);
    }

    free(root);
}

BstNode *bst_insert(BstNode **root, const char *key, SymbolData data) {
    if (root == NULL) {
        return NULL;
    }

    if (*root == NULL) {
        *root =_create_node(key, data);
        return *root;
    }

    int cmp = strcmp(key, (*root)->key);
    BstNode *inserted = NULL;

    if (cmp == 0) {
        return NULL;
    }
    else if (cmp < 0) {
        inserted = bst_insert(&((*root)->left), key, data);
    }
    else {
        inserted = bst_insert(&((*root)->right), key, data);
    }

    if (inserted == NULL) {
        return NULL;
    }

    _update_height(*root);

    int balance = _balance_factor(*root);

    if (balance > 1 && strcmp(key, (*root)->left->key) < 0) {
        *root = _rotate_right(*root);
    }
    else if (balance < -1 && strcmp(key, (*root)->right->key) > 0) {
        *root = _rotate_left(*root);
    }
    else if (balance > 1 && strcmp(key, (*root)->left->key) > 0) {
        (*root)->left = _rotate_left((*root)->left);
        *root = _rotate_right(*root);
    }
    else if (balance < -1 && strcmp(key, (*root)->right->key) < 0) {
        (*root)->right = _rotate_right((*root)->right);
        *root = _rotate_left(*root);
    }
    
    return inserted;
}

