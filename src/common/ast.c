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
        case AST_TYPE_ANY:
            return "Any";
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

AstExpression *ast_expr_call(AstExpression *callee, position pos) {
    AstExpression *e = ast_expr_create(AST_EXPR_CALL, pos);
    e->data.call.callee = callee;
    return e;
}

AstExpression *ast_expr_lambda(AstBLOCK *inside, position pos){
    AstExpression *e = ast_expr_create(AST_EXPR_LAMBDA, pos);
    e->data.lambda.inside = inside;
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

AstBLOCK *ast_block_create(position pos) {
    AstBLOCK *b = ast_alloc(sizeof *b);
    b->pos = pos;
    return b;

}

AstFunction *ast_function_create(bool is_main, char *name, AstType *return_type, 
    AstBLOCK *inside, position pos){
        AstFunction *f = ast_alloc(sizeof *f);
        f->is_main = is_main;
        f->name = name;
        f->return_type = return_type;
        f->inside = inside;
        f->pos = pos;
        return f;
}

void ast_stmt_list_push(AstStatementList *list, AstStatement *stmt) {
    if(list->count == list->capacity) {
        size_t new_capacity;
        if(list->capacity == 0) new_capacity = 4;
        else new_capacity = list->capacity * 2;

        AstStatement **new_items = realloc(list->items, new_capacity * sizeof *new_items);

        if(new_items == NULL){
            fprintf(stderr, "Memory allocation error\n");
            exit(99);
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count] = stmt;
    list->count++;

}

void ast_expr_list_push(AstExpressionList *list, AstExpression *expr) {
    if(list->count == list->capacity) {
        size_t new_capacity;
        if(list->capacity == 0) new_capacity = 4;
        else new_capacity = list->capacity * 2;

        AstExpression **new_items = realloc(list->items, new_capacity * sizeof *new_items);

        if(new_items == NULL){
            fprintf(stderr, "Memory allocation error\n");
            exit(99);
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count] = expr;
    list->count++;

}

void ast_parameter_list_push(AstParameterList *list, AstParameter param){
    if(list->count == list->capacity) {
        size_t new_capacity;
        if(list->capacity == 0) new_capacity = 4;
        else new_capacity = list->capacity * 2;

        AstParameter *new_items = realloc(list->items, new_capacity * sizeof *new_items);

        if(new_items == NULL){
            fprintf(stderr, "Memory allocation error\n");
            exit(99);
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count] = param;
    list->count++;
}

AstParameterList *ast_parameter_lists_push(AstParameterLists *lists) {
    if(lists->count == lists->capacity) {
        size_t new_capacity;
        if(lists->capacity == 0) new_capacity = 4;
        else new_capacity = lists->capacity *2;

        AstParameterList *new_items = realloc(lists->items, new_capacity * sizeof *new_items);
        if (new_items == NULL) {
            fprintf(stderr, "Memory allocation error\n");
            exit(99);
        }
        lists->items = new_items;
        lists->capacity = new_capacity;
    }

    AstParameterList *added = &lists->items[lists->count];
    added->items = NULL;
    added->count = 0;
    added->capacity = 0;
    lists->count++;
    return added;
}

AstExpressionList *ast_argument_lists_push(AstArgumentList *lists) {
    if(lists->count == lists->capacity) {
        size_t new_capacity;
        if(lists->capacity == 0) new_capacity = 4;
        else new_capacity = lists->capacity *2;

        AstExpressionList *new_items = realloc(lists->items, new_capacity * sizeof *new_items);
        if (new_items == NULL) {
            fprintf(stderr, "Memory allocation error\n");
            exit(99);
        }
        lists->items = new_items;
        lists->capacity = new_capacity;
    }

    AstExpressionList *added = &lists->items[lists->count];
    added->items = NULL;
    added->count = 0;
    added->capacity = 0;
    lists->count++;
    return added;
}

void ast_definition_push(AstProgram *program, AstDefinition definition) {
    if(program->count == program->capacity) {
        size_t new_capacity;
        if(program->capacity == 0) new_capacity = 4;
        else new_capacity = program->capacity * 2;

        AstDefinition *new_items = realloc(program->items, new_capacity * sizeof *new_items);

        if(new_items == NULL){
            fprintf(stderr, "Memory allocation error\n");
            exit(99);
        }
        program->items = new_items;
        program->capacity = new_capacity;
    }
    program->items[program->count] = definition;
    program->count++;
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

/* ==================== Debug dump ==================== */

/* AI-generated (Claude). Prompt: "ast_dump for our ast.h: prints the tree with
 * two-space indentation per level, one node per line, recursive like the free
 * functions; for debugging and parser tests" */

/* Indents the line by two spaces per tree level. */
static void dump_indent(FILE *out, int depth) {
    for (int i = 0; i < depth; i++) {
        fputs("  ", out);
    }
}

/* Operator symbol for printing. */
static const char *dump_binary_op(AstBinaryOp op) {
    switch (op) {
    case AST_ADD: return "+";
    case AST_SUBTRACT: return "-";
    case AST_MULTIPLY: return "*";
    case AST_DIVIDE: return "/";
    case AST_EQUAL: return "==";
    case AST_NOT_EQUAL: return "!=";
    case AST_LESS: return "<";
    case AST_LESS_EQUAL: return "<=";
    case AST_GREATER: return ">";
    case AST_GREATER_EQUAL: return ">=";
    case AST_AND: return "&&";
    case AST_OR: return "||";
    }
    return "?";
}

static const char *dump_unary_op(AstUnaryOp op) {
    switch (op) {
    case AST_UNARY_PLUS: return "+";
    case AST_UNARY_MINUS: return "-";
    case AST_UNARY_NOT: return "!";
    }
    return "?";
}

/* Prints a type on the current line, e.g. "Int" or "(Int, Double) => String". */
void ast_dump_type(FILE *out, const AstType *type) {
    if (type == NULL) {
        fputs("?", out);
        return;
    }
    if (type->kind != AST_TYPE_FUNCTION) {
        fputs(ast_type_to_string(type->kind), out);
        return;
    }
    fputc('(', out);
    for (size_t i = 0; i < type->function.parameter_count; i++) {
        if (i > 0) {
            fputs(", ", out);
        }
        ast_dump_type(out, type->function.parameters[i]);
    }
    fputs(") => ", out);
    ast_dump_type(out, type->function.result);
}

/* Prints one parameter list on the current line: "(a: Int, b: Double)". */
static void dump_parameter_list(FILE *out, const AstParameterList *list) {
    fputc('(', out);
    for (size_t i = 0; i < list->count; i++) {
        if (i > 0) {
            fputs(", ", out);
        }
        fprintf(out, "%s: ", list->items[i].name);
        ast_dump_type(out, list->items[i].type);
    }
    fputc(')', out);
}

void ast_dump_expression(FILE *out, const AstExpression *e, int depth) {
    dump_indent(out, depth);
    if (e == NULL) {
        fputs("(null)\n", out);
        return;
    }

    switch (e->kind) {
    case AST_EXPR_INT:
        fprintf(out, "INT %d\n", (int)e->data.int_value);
        break;
    case AST_EXPR_DOUBLE:
        fprintf(out, "DOUBLE %g\n", e->data.double_value);
        break;
    case AST_EXPR_STRING:
        fprintf(out, "STRING \"%.*s\"\n", (int)e->data.string_value.length,
                e->data.string_value.data ? e->data.string_value.data : "");
        break;
    case AST_EXPR_BOOLEAN:
        fprintf(out, "BOOL %s\n", e->data.boolean_value ? "true" : "false");
        break;
    case AST_EXPR_UNIT:
        fputs("UNIT ()\n", out);
        break;
    case AST_EXPR_NAME:
        fprintf(out, "NAME %s\n", e->data.name);
        break;

    case AST_EXPR_BINARY:
        fprintf(out, "BINARY %s\n", dump_binary_op(e->data.binary.op));
        ast_dump_expression(out, e->data.binary.left, depth + 1);
        ast_dump_expression(out, e->data.binary.right, depth + 1);
        break;

    case AST_EXPR_UNARY:
        fprintf(out, "UNARY %s\n", dump_unary_op(e->data.unary.op));
        ast_dump_expression(out, e->data.unary.operand, depth + 1);
        break;

    case AST_EXPR_CALL:
        fputs("CALL\n", out);
        ast_dump_expression(out, e->data.call.callee, depth + 1);
        for (size_t i = 0; i < e->data.call.arguments.count; i++) {
            const AstExpressionList *args = &e->data.call.arguments.items[i];
            dump_indent(out, depth + 1);
            fprintf(out, "ARGS (%zu)\n", args->count);
            for (size_t j = 0; j < args->count; j++) {
                ast_dump_expression(out, args->items[j], depth + 2);
            }
        }
        break;

    case AST_EXPR_MEMBER:
        fprintf(out, "MEMBER .%s\n", e->data.member.name);
        ast_dump_expression(out, e->data.member.receiver, depth + 1);
        break;

    case AST_EXPR_IF:
        fputs("IF\n", out);
        ast_dump_expression(out, e->data.conditional.condition, depth + 1);
        dump_indent(out, depth + 1);
        fputs("THEN\n", out);
        ast_dump_block(out, e->data.conditional.then_branch, depth + 2);
        if (e->data.conditional.else_branch != NULL) {
            dump_indent(out, depth + 1);
            fputs("ELSE\n", out);
            ast_dump_block(out, e->data.conditional.else_branch, depth + 2);
        }
        break;

    case AST_EXPR_LAMBDA:
        fputs("LAMBDA ", out);
        dump_parameter_list(out, &e->data.lambda.parameters);
        fputc('\n', out);
        ast_dump_block(out, e->data.lambda.inside, depth + 1);
        break;

    case AST_EXPR_INTERPOLATED_STRING:
        fputs("INTERPOLATED\n", out);
        for (size_t i = 0; i < e->data.interpolated_string.count; i++) {
            const AstStringPart *part = &e->data.interpolated_string.items[i];
            if (part->kind == AST_STRING_TEXT) {
                dump_indent(out, depth + 1);
                fprintf(out, "TEXT \"%.*s\"\n", (int)part->data.text.length,
                        part->data.text.data ? part->data.text.data : "");
            } else {
                ast_dump_expression(out, part->data.expression, depth + 1);
            }
        }
        break;
    }
}

void ast_dump_statement(FILE *out, const AstStatement *s, int depth) {
    dump_indent(out, depth);
    if (s == NULL) {
        fputs("(null)\n", out);
        return;
    }

    switch (s->kind) {
    case AST_STATEMENT_VARIABLE:
        fprintf(out, "%s %s", s->data.variable.is_changeable ? "VAR" : "VAL",
                s->data.variable.name);
        if (s->data.variable.declared_type != NULL) {
            fputs(" : ", out);
            ast_dump_type(out, s->data.variable.declared_type);
        }
        fputc('\n', out);
        ast_dump_expression(out, s->data.variable.initializer, depth + 1);
        break;

    case AST_STATEMENT_ASSIGN:
        fprintf(out, "ASSIGN %s\n", s->data.assignment.name);
        ast_dump_expression(out, s->data.assignment.value, depth + 1);
        break;

    case AST_STATEMENT_EXPRESSION:
        fputs("EXPR\n", out);
        ast_dump_expression(out, s->data.expression, depth + 1);
        break;

    case AST_STATEMENT_RETURN:
        fputs("RETURN\n", out);
        if (s->data.return_value != NULL) {
            ast_dump_expression(out, s->data.return_value, depth + 1);
        }
        break;

    case AST_STATEMENT_WHILE:
        fputs("WHILE\n", out);
        ast_dump_expression(out, s->data.while_loop.condition, depth + 1);
        ast_dump_block(out, s->data.while_loop.inside, depth + 1);
        break;

    case AST_STATEMENT_FOR:
        fprintf(out, "FOR %s (%s)\n", s->data.for_loop.variable_name,
                s->data.for_loop.inclusive ? "to" : "until");
        ast_dump_expression(out, s->data.for_loop.from, depth + 1);
        ast_dump_expression(out, s->data.for_loop.to, depth + 1);
        if (s->data.for_loop.step != NULL) {
            dump_indent(out, depth + 1);
            fputs("BY\n", out);
            ast_dump_expression(out, s->data.for_loop.step, depth + 2);
        }
        for (size_t i = 0; i < s->data.for_loop.guards.count; i++) {
            dump_indent(out, depth + 1);
            fputs("GUARD\n", out);
            ast_dump_expression(out, s->data.for_loop.guards.items[i], depth + 2);
        }
        ast_dump_block(out, s->data.for_loop.inside, depth + 1);
        break;
    }
}

void ast_dump_block(FILE *out, const AstBLOCK *block, int depth) {
    dump_indent(out, depth);
    if (block == NULL) {
        fputs("(null block)\n", out);
        return;
    }
    fputs("BLOCK\n", out);
    for (size_t i = 0; i < block->statements.count; i++) {
        ast_dump_statement(out, block->statements.items[i], depth + 1);
    }
}

void ast_dump_function(FILE *out, const AstFunction *f, int depth) {
    dump_indent(out, depth);
    fprintf(out, "%sDEF %s", f->is_main ? "@main " : "", f->name);
    for (size_t i = 0; i < f->parameters.count; i++) {
        dump_parameter_list(out, &f->parameters.items[i]);
    }
    if (f->return_type != NULL) {
        fputs(" : ", out);
        ast_dump_type(out, f->return_type);
    }
    fputc('\n', out);
    ast_dump_block(out, f->inside, depth + 1);
}

void ast_dump_program(FILE *out, const AstProgram *program) {
    fputs("PROGRAM\n", out);
    for (size_t i = 0; i < program->count; i++) {
        const AstDefinition *def = &program->items[i];
        if (def->kind == AST_DEF_FUNCTION) {
            ast_dump_function(out, def->data.function, 1);
        } else {
            const AstExtension *ext = def->data.extension;
            dump_indent(out, 1);
            fprintf(out, "EXTENSION (%s: ", ext->receiver.name);
            ast_dump_type(out, ext->receiver.type);
            fputs(")\n", out);
            for (size_t j = 0; j < ext->method_count; j++) {
                ast_dump_function(out, ext->methods[j], 2);
            }
        }
    }
}
