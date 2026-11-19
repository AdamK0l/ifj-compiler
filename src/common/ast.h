/**
 * Project: IFJ26 / IAL compiler
 * File: ast.h
 * Authors:
 *   Peter Takáč (xtakacp00)
 *   Adam Kolacek (xkolaca00)
 */

#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Position in source code
typedef struct {
    size_t line;
    size_t column;
} position;

// Strings from source code
typedef struct {
    char *data;
    size_t length;
} AstString;

typedef enum {
    AST_TYPE_INT,
    AST_TYPE_DOUBLE,
    AST_TYPE_STRING,
    AST_TYPE_BOOLEAN,
    AST_TYPE_UNIT,
    AST_TYPE_FUNCTION

} AstTypeKind;

typedef enum {
    AST_ADD,
    AST_SUBTRACT,
    AST_MULTIPLY,
    AST_DIVIDE,

    AST_EQUAL,
    AST_NOT_EQUAL,
    AST_LESS,
    AST_LESS_EQUAL,
    AST_GREATER,
    AST_GREATER_EQUAL,

    AST_AND,
    AST_OR
} AstBinaryOp;

typedef enum { AST_UNARY_PLUS, AST_UNARY_MINUS, AST_UNARY_NOT } AstUnaryOp;

typedef enum {
    AST_EXPR_INT,
    AST_EXPR_DOUBLE,
    AST_EXPR_STRING,
    AST_EXPR_BOOLEAN,
    AST_EXPR_UNIT,
    AST_EXPR_NAME,
    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_CALL,
    AST_EXPR_MEMBER,
    AST_EXPR_IF,
    AST_EXPR_LAMBDA,
    AST_EXPR_INTERPOLATED_STRING
} AstExpressionKind;

typedef enum {
    AST_STATEMENT_VARIABLE,
    AST_STATEMENT_ASSIGN,
    AST_STATEMENT_EXPRESSION,
    AST_STATEMENT_RETURN,
    AST_STATEMENT_WHILE,
    AST_STATEMENT_FOR
} AstStatementKind;

typedef struct AstStatement AstStatement;
typedef struct AstBLOCK AstBLOCK;
typedef struct AstType AstType;

struct AstType {
    AstTypeKind kind;

    struct {
        AstType **parameters;
        size_t parameter_count;
        AstType *result;
    } function;
};

typedef struct {
    char *name;
    AstType *type;
    position pos;
} AstParameter;

typedef struct {
    AstParameter *items;
    size_t count;
    size_t capacity;
} AstParameterList;

typedef struct {
    AstParameterList *items;
    size_t count;
    size_t capacity;
} AstParameterLists;

typedef struct AstExpression AstExpression;

typedef enum { AST_STRING_TEXT, AST_STRING_EXPRESSION } AstStringPartKind;

typedef struct {
    AstStringPartKind kind;
    position pos;

    union {
        AstString text;
        AstExpression *expression;
    } data;

} AstStringPart;

typedef struct {
    AstExpression **items;
    size_t count;
    size_t capacity;
} AstExpressionList;

typedef struct {
    AstExpressionList *items;
    size_t count;
    size_t capacity;
} AstArgumentList;

struct AstExpression {
    AstExpressionKind kind;
    position pos;
    AstType *evaluated_type;

    union {
        int32_t int_value;
        double double_value;
        AstString string_value;
        bool boolean_value;
        char *name;

        struct {
            AstBinaryOp op;
            AstExpression *left;
            AstExpression *right;
        } binary;

        struct {
            AstUnaryOp op;
            AstExpression *operand;
        } unary;

        struct {
            AstExpression *callee;
            AstArgumentList arguments;
        } call;

        struct {
            AstExpression *receiver;
            char *name;
        } member;

        struct {
            AstExpression *condition;
            AstBLOCK *then_branch;
            AstBLOCK *else_branch;
        } conditional;

        struct {
            AstParameterList parameters;
            AstBLOCK *inside;
        } lambda;

        struct {
            AstStringPart *items;
            size_t count;
        } interpolated_string;
    } data;
};

typedef struct {
    AstStatement **items;
    size_t count;
    size_t capacity;
} AstStatementList;

struct AstBLOCK {
    position pos;
    AstStatementList statements;
};

struct AstStatement {
    AstStatementKind kind;
    position pos;

    union {
        struct {
            char *name;
            bool is_changeable;
            AstType *declared_type;
            AstExpression *initializer;
        } variable;

        struct {
            char *name;
            AstExpression *value;
        } assignment;

        AstExpression *expression;
        AstExpression *return_value;

        struct {
            AstExpression *condition;
            AstBLOCK *inside;
        } while_loop;

        struct {
            char *variable_name;
            position variable_pos;

            AstExpression *from;
            AstExpression *to;
            bool inclusive;

            AstExpression *step;
            AstBLOCK *inside;
            AstExpressionList guards;
        } for_loop;
    } data;
};

typedef struct {
    position pos;
    char *name;
    bool is_main;

    AstParameterLists parameters;
    AstType *return_type; // NULL if no explicit type
    AstBLOCK *inside;
} AstFunction;

typedef struct {
    position pos;
    AstParameter receiver;

    AstFunction **methods;
    size_t method_count;
} AstExtension;

typedef enum { AST_DEF_FUNCTION, AST_DEF_EXTENSION } AstDefinitionKind;

typedef struct {
    AstDefinitionKind kind;

    union {
        AstFunction *function;
        AstExtension *extension;
    } data;
} AstDefinition;

typedef struct {
    AstDefinition *items;
    size_t count;
    size_t capacity;
} AstProgram;

/* ==================== Type Helper Functions ==================== */

/**
 * Creates a simple atomic type (Int, Double, String, Boolean, Unit).
 */
AstType *ast_type_create(AstTypeKind kind);

/**
 * Creates a function type (for LAMBDAS extension).
 */
AstType *ast_type_create_function(AstType **parameters, size_t parameter_count, AstType *result);

/**
 * Deep-copies an AstType.
 */
AstType *ast_type_clone(const AstType *type);

/**
 * Compares two types for exact structural equality.
 */
bool ast_type_equals(const AstType *a, const AstType *b);

/**
 * Returns human-readable string representation of a type kind.
 */
const char *ast_type_to_string(AstTypeKind kind);

/**
 * Frees a single AstType structure and its nested function parameters/return types (if any).
 */
void ast_type_free(AstType *type);

/* ==================== Memory Deallocation (Destructors) ==================== */

/**
 * Frees an AstParameterList and its elements.
 */
void ast_parameter_list_free(AstParameterList *list);

/**
 * Frees an AstParameterLists and its parameter lists.
 */
void ast_parameter_lists_free(AstParameterLists *lists);

/**
 * Recursively frees an AstExpression and all its subexpressions, arguments, and evaluated_type.
 */
void ast_free_expression(AstExpression *expr);

/**
 * Recursively frees an AstStatement and its contained expressions and blocks.
 */
void ast_free_statement(AstStatement *stmt);

/**
 * Recursively frees an AstBLOCK and all statements inside it.
 */
void ast_free_block(AstBLOCK *block);

/**
 * Recursively frees an AstFunction, its parameters, return type, and body block.
 */
void ast_free_function(AstFunction *func);

/**
 * Recursively frees an AstExtension and its receiver and methods.
 */
void ast_free_extension(AstExtension *ext);

/**
 * Recursively frees the entire AstProgram and all its definitions.
 */
void ast_free_program(AstProgram *program);

// Allocates memory 
void *ast_alloc(size_t size);

// Expression constructors
AstExpression *ast_expr_create(AstExpressionKind kind, position pos);
AstExpression *ast_expr_int(int32_t value, position pos);
AstExpression *ast_expr_double(double value, position pos);
AstExpression *ast_expr_unit(position pos);
AstExpression *ast_expr_name(char *name, position pos);
AstExpression *ast_expr_bool(bool value, position pos);
AstExpression *ast_expr_string(char *data, size_t length, position pos);
AstExpression *ast_expr_unary(AstUnaryOp op, AstExpression *operand, position pos);
AstExpression *ast_expr_binary(AstBinaryOp op, AstExpression *left, AstExpression *right, position pos);

/* AI-generated (Claude). Prompt: "Generate the declarations for the remaining constructors and list functions in the provided ast.c" */

AstExpression *ast_expr_member(AstExpression *receiver, char *name, position pos);
AstExpression *ast_expr_conditional(AstExpression *condition, AstBLOCK *then_branch,
                                    AstBLOCK *else_branch, position pos);
AstExpression *ast_expr_call(AstExpression *callee, position pos);
AstExpression *ast_expr_lambda(AstBLOCK *inside, position pos);

// Statement constructors
AstStatement *ast_stmt_create(AstStatementKind kind, position pos);
AstStatement *ast_stmt_variable(char *name, bool is_changeable, AstType *declared_type,
                                AstExpression *initializer, position pos);
AstStatement *ast_stmt_assign(char *name, AstExpression *value, position pos);
AstStatement *ast_stmt_expression(AstExpression *expression, position pos);
AstStatement *ast_stmt_return(AstExpression *return_value, position pos);
AstStatement *ast_stmt_while(AstExpression *condition, AstBLOCK *inside, position pos);
AstStatement *ast_stmt_for(char *variable_name, position variable_pos, AstExpression *from,
                           AstExpression *to, bool inclusive, AstExpression *step,
                           AstBLOCK *inside, position pos);

// Block and function constructors
AstBLOCK *ast_block_create(position pos);
AstFunction *ast_function_create(bool is_main, char *name, AstType *return_type,
                                 AstBLOCK *inside, position pos);

// Adding to lists
void ast_stmt_list_push(AstStatementList *list, AstStatement *stmt);
void ast_expr_list_push(AstExpressionList *list, AstExpression *expr);
void ast_parameter_list_push(AstParameterList *list, AstParameter param);
AstParameterList *ast_parameter_lists_push(AstParameterLists *lists);
AstExpressionList *ast_argument_lists_push(AstArgumentList *lists);
void ast_definition_push(AstProgram *program, AstDefinition definition);


/* ==================== Debug dump ==================== */
/* AI-generated (Claude). Prompt: "declarations for ast_dump" */
#include <stdio.h>

void ast_dump_type(FILE *out, const AstType *type);
void ast_dump_expression(FILE *out, const AstExpression *e, int depth);
void ast_dump_statement(FILE *out, const AstStatement *s, int depth);
void ast_dump_block(FILE *out, const AstBLOCK *block, int depth);
void ast_dump_function(FILE *out, const AstFunction *f, int depth);
void ast_dump_program(FILE *out, const AstProgram *program);

#endif
