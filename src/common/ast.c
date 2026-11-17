/**
 * Project: IFJ26 / IAL compiler
 * File: ast.c
 * Authors:
 *   Peter Takáč (xtakacp00)
 *   Adam Kolacek (xkolaca00)
 */

#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== Type Helper Functions ==================== */

AstType *ast_type_create(AstTypeKind kind) {
    AstType *type = (AstType *)malloc(sizeof(AstType));
    if (!type) {
        return NULL;
    }
    type->kind = kind;
    type->function.parameters = NULL;
    type->function.parameter_count = 0;
    type->function.result = NULL;
    return type;
}

AstType *ast_type_create_function(AstType **parameters, size_t parameter_count, AstType *result) {
    AstType *type = (AstType *)malloc(sizeof(AstType));
    if (!type) {
        return NULL;
    }
    type->kind = AST_TYPE_FUNCTION;
    type->function.parameters = parameters;
    type->function.parameter_count = parameter_count;
    type->function.result = result;
    return type;
}

AstType *ast_type_clone(const AstType *type) {
    if (!type) {
        return NULL;
    }

    AstType *clone = (AstType *)malloc(sizeof(AstType));
    if (!clone) {
        return NULL;
    }
    clone->kind = type->kind;
    clone->function.parameters = NULL;
    clone->function.parameter_count = 0;
    clone->function.result = NULL;

    if (type->kind == AST_TYPE_FUNCTION) {
        clone->function.parameter_count = type->function.parameter_count;
        if (type->function.parameter_count > 0 && type->function.parameters != NULL) {
            clone->function.parameters =
                (AstType **)malloc(sizeof(AstType *) * type->function.parameter_count);
            if (!clone->function.parameters) {
                free(clone);
                return NULL;
            }
            for (size_t i = 0; i < type->function.parameter_count; i++) {
                clone->function.parameters[i] = ast_type_clone(type->function.parameters[i]);
            }
        }
        clone->function.result = ast_type_clone(type->function.result);
    }

    return clone;
}

bool ast_type_equals(const AstType *a, const AstType *b) {
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return false;
    }
    if (a->kind != b->kind) {
        return false;
    }

    if (a->kind == AST_TYPE_FUNCTION) {
        if (a->function.parameter_count != b->function.parameter_count) {
            return false;
        }
        for (size_t i = 0; i < a->function.parameter_count; i++) {
            if (!ast_type_equals(a->function.parameters[i], b->function.parameters[i])) {
                return false;
            }
        }
        return ast_type_equals(a->function.result, b->function.result);
    }

    return true;
}

const char *ast_type_to_string(AstTypeKind kind) {
    switch (kind) {
    case AST_TYPE_INT:
        return "Int";
    case AST_TYPE_DOUBLE:
        return "Double";
    case AST_TYPE_STRING:
        return "String";
    case AST_TYPE_BOOLEAN:
        return "Boolean";
    case AST_TYPE_UNIT:
        return "Unit";
    case AST_TYPE_FUNCTION:
        return "Function";
    default:
        return "Unknown";
    }
}

void ast_type_free(AstType *type) {
    if (!type) {
        return;
    }

    if (type->kind == AST_TYPE_FUNCTION) {
        if (type->function.parameters != NULL) {
            for (size_t i = 0; i < type->function.parameter_count; i++) {
                ast_type_free(type->function.parameters[i]);
            }
            free(type->function.parameters);
        }
        ast_type_free(type->function.result);
    }

    free(type);
}

void *ast_alloc(size_t size) {
    void *p = calloc(1, size);
    if (p == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        exit(99);
    }
    return p;
}

/*==================== Memory Allocation (Constructors) ====================*/

AstExpression *ast_expr_create(AstExpressionKind kind, position pos) {
    AstExpression *e = ast_alloc(sizeof *e);
    e->kind = kind;
    e->pos = pos;
    return e;
}

AstExpression *ast_expr_int(int32_t value, position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_INT, pos);
    e->data.int_value = value;
    return e;
}

AstExpression *ast_expr_double(double value, position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_DOUBLE, pos);
    e->data.double_value = value;
    return e;
}

AstExpression *ast_expr_unit(position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_UNIT, pos);
    return e;
}

AstExpression *ast_expr_name(char *name, position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_NAME, pos);
    e->data.name = name;
    return e;
}

AstExpression *ast_expr_bool(bool value, position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_BOOLEAN, pos);
    e->data.boolean_value = value;
    return e;
}

AstExpression *ast_expr_string(char *data, size_t length, position pos) {
    AstExpression *s = ast_expr_create(AST_EXPR_STRING, pos);
    s->data.string_value.data = data;
    s->data.string_value.length = length;
    return s;
}

AstExpression *ast_expr_unary(AstUnaryOp op, AstExpression *operand, position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_UNARY, pos);
    e->data.unary.op = op;
    e->data.unary.operand = operand;
    return e;
}

AstExpression *ast_expr_binary(AstBinaryOp op, AstExpression *left, AstExpression *right, position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_BINARY, pos);
    e->data.binary.op = op;
    e->data.binary.left = left;
    e->data.binary.right = right;
    return e;
}

AstExpression *ast_expr_member(AstExpression *receiver, char *name, position pos){
    AstExpression *e = ast_expr_create(AST_EXPR_MEMBER, pos);
    e->data.member.receiver = receiver;
    e->data.member.name = name;
    return e;
}

AstExpression *ast_expr_conditional(AstExpression *condition, AstBLOCK *then_branch, AstBLOCK *else_branch, position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_IF, pos);
    e->data.conditional.condition = condition;
    e->data.conditional.then_branch = then_branch;
    e->data.conditional.else_branch = else_branch;
    return e;
}



AstStatement *ast_stmt_create(AstStatementKind kind, position pos) {
    AstStatement *s = ast_alloc(sizeof *s);
    s->kind = kind;
    s->pos = pos;
    return s;
}

AstStatement *ast_stmt_variable(char *name, bool is_changeable, AstType *declared_type, AstExpression *initializer, position pos) {
    AstStatement *s = ast_stmt_create(AST_STATEMENT_VARIABLE, pos);
    s->data.variable.name = name;
    s->data.variable.is_changeable = is_changeable;
    s->data.variable.declared_type = declared_type;
    s->data.variable.initializer = initializer;
    return s;
}

AstStatement *ast_stmt_assign(char *name, AstExpression *value, position pos) {
    AstStatement *s = ast_stmt_create(AST_STATEMENT_ASSIGN, pos);
    s->data.assignment.name = name;
    s->data.assignment.value = value;
    return s;
}

AstStatement *ast_stmt_expression(AstExpression *expression, position pos) {
    AstStatement *s = ast_stmt_create(AST_STATEMENT_EXPRESSION, pos);
    s->data.expression = expression;
    return s;
}

AstStatement *ast_stmt_return(AstExpression *return_value, position pos) {
    AstStatement *s = ast_stmt_create(AST_STATEMENT_RETURN, pos);
    s->data.return_value = return_value;
    return s;
}

AstStatement *ast_stmt_while(AstExpression *condition, AstBLOCK *inside, position pos){
    AstStatement *s = ast_stmt_create(AST_STATEMENT_WHILE, pos);
    s->data.while_loop.condition = condition;
    s->data.while_loop.inside = inside;
    return s;
}

AstStatement *ast_stmt_for(char *variable_name, position variable_pos, AstExpression *from, 
    AstExpression *to, bool inclusive, AstExpression *step, AstBLOCK *inside, position pos) {
        AstStatement *s = ast_stmt_create(AST_STATEMENT_FOR, pos);
        s->data.for_loop.variable_name = variable_name;
        s->data.for_loop.variable_pos = variable_pos;
        s->data.for_loop.from = from;
        s->data.for_loop.to = to;
        s->data.for_loop.inclusive = inclusive;
        s->data.for_loop.step = step;
        s->data.for_loop.inside = inside;
        return s;
}

/* ==================== Memory Deallocation (Destructors) ==================== */

void ast_parameter_list_free(AstParameterList *list) {
    if (!list || !list->items) {
        return;
    }

    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].name);
        ast_type_free(list->items[i].type);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void ast_parameter_lists_free(AstParameterLists *lists) {
    if (!lists || !lists->items) {
        return;
    }

    for (size_t i = 0; i < lists->count; i++) {
        ast_parameter_list_free(&lists->items[i]);
    }
    free(lists->items);
    lists->items = NULL;
    lists->count = 0;
    lists->capacity = 0;
}

void ast_free_expression(AstExpression *expr) {
    if (!expr) {
        return;
    }

    ast_type_free(expr->evaluated_type);

    switch (expr->kind) {
    case AST_EXPR_INT:
    case AST_EXPR_DOUBLE:
    case AST_EXPR_BOOLEAN:
    case AST_EXPR_UNIT:
        // Primitive literal data requires no extra heap free
        break;

    case AST_EXPR_STRING:
        free(expr->data.string_value.data);
        break;

    case AST_EXPR_NAME:
        free(expr->data.name);
        break;

    case AST_EXPR_BINARY:
        ast_free_expression(expr->data.binary.left);
        ast_free_expression(expr->data.binary.right);
        break;

    case AST_EXPR_UNARY:
        ast_free_expression(expr->data.unary.operand);
        break;

    case AST_EXPR_CALL:
        ast_free_expression(expr->data.call.callee);
        if (expr->data.call.arguments.items != NULL) {
            for (size_t i = 0; i < expr->data.call.arguments.count; i++) {
                AstExpressionList *list = &expr->data.call.arguments.items[i];
                if (list->items != NULL) {
                    for (size_t j = 0; j < list->count; j++) {
                        ast_free_expression(list->items[j]);
                    }
                    free(list->items);
                }
            }
            free(expr->data.call.arguments.items);
        }
        break;

    case AST_EXPR_MEMBER:
        ast_free_expression(expr->data.member.receiver);
        free(expr->data.member.name);
        break;

    case AST_EXPR_IF:
        ast_free_expression(expr->data.conditional.condition);
        ast_free_block(expr->data.conditional.then_branch);
        ast_free_block(expr->data.conditional.else_branch);
        break;

    case AST_EXPR_LAMBDA:
        ast_parameter_list_free(&expr->data.lambda.parameters);
        ast_free_block(expr->data.lambda.inside);
        break;

    case AST_EXPR_INTERPOLATED_STRING:
        if (expr->data.interpolated_string.items != NULL) {
            for (size_t i = 0; i < expr->data.interpolated_string.count; i++) {
                AstStringPart *part = &expr->data.interpolated_string.items[i];
                if (part->kind == AST_STRING_TEXT) {
                    free(part->data.text.data);
                } else if (part->kind == AST_STRING_EXPRESSION) {
                    ast_free_expression(part->data.expression);
                }
            }
            free(expr->data.interpolated_string.items);
        }
        break;
    }

    free(expr);
}

void ast_free_statement(AstStatement *stmt) {
    if (!stmt) {
        return;
    }

    switch (stmt->kind) {
    case AST_STATEMENT_VARIABLE:
        free(stmt->data.variable.name);
        ast_type_free(stmt->data.variable.declared_type);
        ast_free_expression(stmt->data.variable.initializer);
        break;

    case AST_STATEMENT_ASSIGN:
        free(stmt->data.assignment.name);
        ast_free_expression(stmt->data.assignment.value);
        break;

    case AST_STATEMENT_EXPRESSION:
        ast_free_expression(stmt->data.expression);
        break;

    case AST_STATEMENT_RETURN:
        ast_free_expression(stmt->data.return_value);
        break;

    case AST_STATEMENT_WHILE:
        ast_free_expression(stmt->data.while_loop.condition);
        ast_free_block(stmt->data.while_loop.inside);
        break;

    case AST_STATEMENT_FOR:
        free(stmt->data.for_loop.variable_name);
        ast_free_expression(stmt->data.for_loop.from);
        ast_free_expression(stmt->data.for_loop.to);
        ast_free_expression(stmt->data.for_loop.step);
        ast_free_block(stmt->data.for_loop.inside);
        if (stmt->data.for_loop.guards.items != NULL) {
            for (size_t i = 0; i < stmt->data.for_loop.guards.count; i++) {
                ast_free_expression(stmt->data.for_loop.guards.items[i]);
            }
            free(stmt->data.for_loop.guards.items);
        }
        break;
    }

    free(stmt);
}

void ast_free_block(AstBLOCK *block) {
    if (!block) {
        return;
    }

    if (block->statements.items != NULL) {
        for (size_t i = 0; i < block->statements.count; i++) {
            ast_free_statement(block->statements.items[i]);
        }
        free(block->statements.items);
        block->statements.items = NULL;
    }

    free(block);
}

void ast_free_function(AstFunction *func) {
    if (!func) {
        return;
    }

    free(func->name);
    ast_parameter_lists_free(&func->parameters);
    ast_type_free(func->return_type);
    ast_free_block(func->inside);

    free(func);
}

void ast_free_extension(AstExtension *ext) {
    if (!ext) {
        return;
    }

    free(ext->receiver.name);
    ast_type_free(ext->receiver.type);

    if (ext->methods != NULL) {
        for (size_t i = 0; i < ext->method_count; i++) {
            ast_free_function(ext->methods[i]);
        }
        free(ext->methods);
        ext->methods = NULL;
    }

    free(ext);
}

void ast_free_program(AstProgram *program) {
    if (!program) {
        return;
    }

    if (program->items != NULL) {
        for (size_t i = 0; i < program->count; i++) {
            if (program->items[i].kind == AST_DEF_FUNCTION) {
                ast_free_function(program->items[i].data.function);
            } else if (program->items[i].kind == AST_DEF_EXTENSION) {
                ast_free_extension(program->items[i].data.extension);
            }
        }
        free(program->items);
        program->items = NULL;
    }
    program->count = 0;
    program->capacity = 0;
}
