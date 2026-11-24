/**
 * Project: IFJ26 / IAL compiler
 * File: type_checker.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "type_checker.h"
#include "semantics.h"
#include "errors.h"
#include <string.h>

/* ==================== Static Built-in Atomic Types ==================== */

static AstType _type_int = { AST_TYPE_INT, { NULL, 0, NULL } };
static AstType _type_double = { AST_TYPE_DOUBLE, { NULL, 0, NULL } };
static AstType _type_string = { AST_TYPE_STRING, { NULL, 0, NULL } };
static AstType _type_boolean = { AST_TYPE_BOOLEAN, { NULL, 0, NULL } };
static AstType _type_unit = { AST_TYPE_UNIT, { NULL, 0, NULL } };
static AstType _type_any = { AST_TYPE_ANY, { NULL, 0, NULL } };

AstType *type_get_int(void) { return &_type_int; }
AstType *type_get_double(void) { return &_type_double; }
AstType *type_get_string(void) { return &_type_string; }
AstType *type_get_boolean(void) { return &_type_boolean; }
AstType *type_get_unit(void) { return &_type_unit; }
AstType *type_get_any(void) { return &_type_any; }

/* ==================== Type Compatibility Helpers ==================== */

bool type_is_numeric(const AstType *type) {
    if (!type) {
        return false;
    }
    return (type->kind == AST_TYPE_INT || type->kind == AST_TYPE_DOUBLE);
}

bool type_is_compatible(const AstType *expected, const AstType *actual) {
    if (!expected || !actual) {
        return false;
    }
    // Any wildcard accepts any valid type
    if (expected->kind == AST_TYPE_ANY) {
        return true;
    }
    // Exact structural equality
    if (ast_type_equals(expected, actual)) {
        return true;
    }
    // Implicit promotion: Int -> Double (expected Double, given Int)
    if (expected->kind == AST_TYPE_DOUBLE && actual->kind == AST_TYPE_INT) {
        return true;
    }
    return false;
}

/* ==================== Operator Type Evaluation ==================== */

static int _check_binary_op(AstExpression *expr, AstType *left, AstType *right, AstType **out_type) {
    AstBinaryOp op = expr->data.binary.op;

    switch (op) {
        case AST_ADD:
            if (left->kind == AST_TYPE_STRING && right->kind == AST_TYPE_STRING) {
                *out_type = type_get_string();
                return ERR_OK;
            }
            if (left->kind == AST_TYPE_INT && right->kind == AST_TYPE_INT) {
                *out_type = type_get_int();
                return ERR_OK;
            }
            if (type_is_numeric(left) && type_is_numeric(right)) {
                *out_type = type_get_double();
                return ERR_OK;
            }
            error_print(ERR_SEM_TYPE, expr->pos,
                        "Incompatible types for operator '+': '%s' and '%s'",
                        ast_type_to_string(left->kind), ast_type_to_string(right->kind));
            return ERR_SEM_TYPE;

        case AST_SUBTRACT:
        case AST_MULTIPLY:
            if (left->kind == AST_TYPE_INT && right->kind == AST_TYPE_INT) {
                *out_type = type_get_int();
                return ERR_OK;
            }
            if (type_is_numeric(left) && type_is_numeric(right)) {
                *out_type = type_get_double();
                return ERR_OK;
            }
            error_print(ERR_SEM_TYPE, expr->pos,
                        "Arithmetic operator requires numeric operands, but got '%s' and '%s'",
                        ast_type_to_string(left->kind), ast_type_to_string(right->kind));
            return ERR_SEM_TYPE;

        case AST_DIVIDE:
            if (left->kind == AST_TYPE_INT && right->kind == AST_TYPE_INT) {
                *out_type = type_get_int(); // Integer division
                return ERR_OK;
            }
            if (type_is_numeric(left) && type_is_numeric(right)) {
                *out_type = type_get_double(); // Floating-point division
                return ERR_OK;
            }
            error_print(ERR_SEM_TYPE, expr->pos,
                        "Division operator '/' requires numeric operands, but got '%s' and '%s'",
                        ast_type_to_string(left->kind), ast_type_to_string(right->kind));
            return ERR_SEM_TYPE;

        case AST_LESS:
        case AST_LESS_EQUAL:
        case AST_GREATER:
        case AST_GREATER_EQUAL:
            if (type_is_numeric(left) && type_is_numeric(right)) {
                *out_type = type_get_boolean();
                return ERR_OK;
            }
            if (left->kind == AST_TYPE_STRING && right->kind == AST_TYPE_STRING) {
                *out_type = type_get_boolean();
                return ERR_OK;
            }
            error_print(ERR_SEM_TYPE, expr->pos,
                        "Relational operator requires compatible numbers or strings, but got '%s' and '%s'",
                        ast_type_to_string(left->kind), ast_type_to_string(right->kind));
            return ERR_SEM_TYPE;

        case AST_EQUAL:
        case AST_NOT_EQUAL:
            if (type_is_numeric(left) && type_is_numeric(right)) {
                *out_type = type_get_boolean();
                return ERR_OK;
            }
            if (ast_type_equals(left, right)) {
                *out_type = type_get_boolean();
                return ERR_OK;
            }
            error_print(ERR_SEM_TYPE, expr->pos,
                        "Equality operator requires operands of matching types, but got '%s' and '%s'",
                        ast_type_to_string(left->kind), ast_type_to_string(right->kind));
            return ERR_SEM_TYPE;

        case AST_AND:
        case AST_OR:
            if (left->kind == AST_TYPE_BOOLEAN && right->kind == AST_TYPE_BOOLEAN) {
                *out_type = type_get_boolean();
                return ERR_OK;
            }
            error_print(ERR_SEM_TYPE, expr->pos,
                        "Logical operator requires Boolean operands, but got '%s' and '%s'",
                        ast_type_to_string(left->kind), ast_type_to_string(right->kind));
            return ERR_SEM_TYPE;

        default:
            error_print(ERR_SEM_OTHER, expr->pos, "Unknown binary operator");
            return ERR_SEM_OTHER;
    }
}

static int _check_unary_op(AstExpression *expr, AstType *operand, AstType **out_type) {
    AstUnaryOp op = expr->data.unary.op;

    switch (op) {
        case AST_UNARY_PLUS:
        case AST_UNARY_MINUS:
            if (type_is_numeric(operand)) {
                *out_type = operand;
                return ERR_OK;
            }
            error_print(ERR_SEM_TYPE, expr->pos,
                        "Unary +/- requires numeric operand, but got '%s'",
                        ast_type_to_string(operand->kind));
            return ERR_SEM_TYPE;

        case AST_UNARY_NOT:
            if (operand->kind == AST_TYPE_BOOLEAN) {
                *out_type = type_get_boolean();
                return ERR_OK;
            }
            error_print(ERR_SEM_TYPE, expr->pos,
                        "Unary '!' operator requires Boolean operand, but got '%s'",
                        ast_type_to_string(operand->kind));
            return ERR_SEM_TYPE;

        default:
            error_print(ERR_SEM_OTHER, expr->pos, "Unknown unary operator");
            return ERR_SEM_OTHER;
    }
}

/* ==================== Function & Member Call Evaluation ==================== */

static int _check_member_call(AstExpression *expr, Symtable *table, AstType **out_type) {
    AstExpression *callee = expr->data.call.callee;
    AstExpression *recv = callee->data.member.receiver;
    const char *name = callee->data.member.name;

    if (!recv) {
        error_print(ERR_SEM_UNDEFINED, expr->pos, "Member call receiver is missing");
        return ERR_SEM_UNDEFINED;
    }
    if (!name) {
        error_print(ERR_SEM_UNDEFINED, expr->pos, "Member name is missing");
        return ERR_SEM_UNDEFINED;
    }

    AstType *rec_type = NULL;
    int res = check_expression(recv, table, &rec_type);
    if (res != ERR_OK) return res;

    // Check if user is attempting to call a VALMEM built-in with parentheses
    if (strcmp(name, "toString") == 0 || strcmp(name, "toInt") == 0 ||
        strcmp(name, "floor") == 0 || strcmp(name, "length") == 0) {
        error_print(ERR_SEM_CALL, expr->pos,
                    "Built-in member '%s' cannot be called with arguments or parentheses", name);
        return ERR_SEM_CALL;
    }

    // Look up extension method for this receiver type
    AstFunction *ext_fn = symtable_lookup_extension(table, rec_type, name);
    if (!ext_fn) {
        error_print(ERR_SEM_UNDEFINED, expr->pos,
                    "Unknown extension method '%s' for type '%s'",
                    name, ast_type_to_string(rec_type->kind));
        return ERR_SEM_UNDEFINED;
    }

    // Check parameterless method without parens called with parens:
    // Spec: "Povoleny jsou i deklarace bezparametrických metod bez závorek (v příkladu níže např. twice), které se pak volají bez závorek."
    // "pozor, nelze `x.foo` ani `x.twice()`"
    if (ext_fn->parameters.count == 0) {
        error_print(ERR_SEM_CALL, expr->pos,
                    "Parameterless extension method '%s' must be called without parentheses", name);
        return ERR_SEM_CALL;
    }

    // Match parameter lists with argument lists
    if (expr->data.call.arguments.count != ext_fn->parameters.count) {
        error_print(ERR_SEM_CALL, expr->pos,
                    "Extension method '%s' expects %zu parameter list(s), but got %zu",
                    name, ext_fn->parameters.count, expr->data.call.arguments.count);
        return ERR_SEM_CALL;
    }

    for (size_t list_idx = 0; list_idx < ext_fn->parameters.count; list_idx++) {
        AstParameterList *pl = &ext_fn->parameters.items[list_idx];
        AstExpressionList *al = &expr->data.call.arguments.items[list_idx];

        if (pl->count != al->count) {
            error_print(ERR_SEM_CALL, expr->pos,
                        "Argument count mismatch in list %zu of extension method '%s': expected %zu, got %zu",
                        list_idx + 1, name, pl->count, al->count);
            return ERR_SEM_CALL;
        }

        for (size_t p = 0; p < pl->count; p++) {
            AstExpression *arg_expr = al->items[p];
            AstType *arg_type = NULL;
            res = check_expression(arg_expr, table, &arg_type);
            if (res != ERR_OK) return res;

            AstType *expected_type = pl->items[p].type;
            if (!type_is_compatible(expected_type, arg_type)) {
                error_print(ERR_SEM_CALL, arg_expr->pos,
                            "Argument %zu in list %zu of extension method '%s' expects type '%s', but got '%s'",
                            p + 1, list_idx + 1, name,
                            ast_type_to_string(expected_type->kind),
                            ast_type_to_string(arg_type->kind));
                return ERR_SEM_CALL;
            }
        }
    }

    AstType *ret = ext_fn->return_type ? ext_fn->return_type : type_get_unit();
    if (callee->evaluated_type) {
        ast_type_free(callee->evaluated_type);
    }
    callee->evaluated_type = ast_type_clone(ret);

    *out_type = ret;
    return ERR_OK;
}

static int _check_function_call(AstExpression *expr, Symtable *table, AstType **out_type) {
    if (!expr->data.call.callee) {
        error_print(ERR_SEM_OTHER, expr->pos, "Callee expression is missing");
        return ERR_SEM_OTHER;
    }

    if (expr->data.call.callee->kind == AST_EXPR_MEMBER) {
        return _check_member_call(expr, table, out_type);
    }

    if (expr->data.call.callee->kind != AST_EXPR_NAME) {
        error_print(ERR_SEM_OTHER, expr->pos, "Complex callee expressions are not supported");
        return ERR_SEM_OTHER;
    }

    const char *func_name = expr->data.call.callee->data.name;
    BstNode *func_node = symtable_lookup(table, func_name);

    if (!func_node || func_node->data.kind != SYMBOL_FUNCTION) {
        error_print(ERR_SEM_UNDEFINED, expr->pos, "Call to undefined function '%s'", func_name);
        return ERR_SEM_UNDEFINED;
    }

    // Count total arguments across all argument lists
    size_t arg_count = 0;
    for (size_t i = 0; i < expr->data.call.arguments.count; i++) {
        arg_count += expr->data.call.arguments.items[i].count;
    }

    if (arg_count != func_node->data.param_count) {
        error_print(ERR_SEM_CALL, expr->pos,
                    "Function '%s' expects %zu argument(s), but got %zu",
                    func_name, func_node->data.param_count, arg_count);
        return ERR_SEM_CALL;
    }

    // Type check each argument
    size_t arg_idx = 0;
    for (size_t i = 0; i < expr->data.call.arguments.count; i++) {
        for (size_t j = 0; j < expr->data.call.arguments.items[i].count; j++) {
            AstExpression *arg_expr = expr->data.call.arguments.items[i].items[j];
            AstType *arg_type = NULL;

            int res = check_expression(arg_expr, table, &arg_type);
            if (res != ERR_OK) {
                return res;
            }

            AstType *expected_type = func_node->data.param_types[arg_idx];
            if (!type_is_compatible(expected_type, arg_type)) {
                error_print(ERR_SEM_CALL, arg_expr->pos,
                            "Argument %zu of function '%s' expects type '%s', but got '%s'",
                            arg_idx + 1, func_name,
                            ast_type_to_string(expected_type->kind),
                            ast_type_to_string(arg_type->kind));
                return ERR_SEM_CALL;
            }

            arg_idx++;
        }
    }

    *out_type = func_node->data.type ? func_node->data.type : type_get_unit();
    return ERR_OK;
}

/* ==================== Expression Type Checking Entry ==================== */

int check_expression(AstExpression *expr, Symtable *table, AstType **out_type) {
    if (!expr || !table || !out_type) {
        return ERR_INTERNAL;
    }

    int res = ERR_OK;

    switch (expr->kind) {
        case AST_EXPR_INT:
            *out_type = type_get_int();
            break;

        case AST_EXPR_DOUBLE:
            *out_type = type_get_double();
            break;

        case AST_EXPR_STRING:
            *out_type = type_get_string();
            break;

        case AST_EXPR_BOOLEAN:
            *out_type = type_get_boolean();
            break;

        case AST_EXPR_UNIT:
            *out_type = type_get_unit();
            break;

        case AST_EXPR_NAME: {
            BstNode *node = symtable_lookup(table, expr->data.name);
            if (!node) {
                error_print(ERR_SEM_UNDEFINED, expr->pos, "Undefined identifier '%s'", expr->data.name);
                return ERR_SEM_UNDEFINED;
            }
            if (node->data.kind != SYMBOL_VARIABLE) {
                error_print(ERR_SEM_TYPE, expr->pos,
                            "Identifier '%s' refers to a function, not a variable", expr->data.name);
                return ERR_SEM_TYPE;
            }
            *out_type = node->data.type;
            break;
        }

        case AST_EXPR_BINARY: {
            AstType *left_type = NULL;
            AstType *right_type = NULL;

            res = check_expression(expr->data.binary.left, table, &left_type);
            if (res != ERR_OK) return res;

            res = check_expression(expr->data.binary.right, table, &right_type);
            if (res != ERR_OK) return res;

            res = _check_binary_op(expr, left_type, right_type, out_type);
            if (res != ERR_OK) return res;
            break;
        }

        case AST_EXPR_UNARY: {
            AstType *operand_type = NULL;
            res = check_expression(expr->data.unary.operand, table, &operand_type);
            if (res != ERR_OK) return res;

            res = _check_unary_op(expr, operand_type, out_type);
            if (res != ERR_OK) return res;
            break;
        }

        case AST_EXPR_CALL:
            res = _check_function_call(expr, table, out_type);
            if (res != ERR_OK) return res;
            break;

        case AST_EXPR_MEMBER: {
            if (!expr->data.member.receiver) {
                error_print(ERR_SEM_UNDEFINED, expr->pos, "Member receiver expression is missing");
                return ERR_SEM_UNDEFINED;
            }

            AstType *rec_type = NULL;
            res = check_expression(expr->data.member.receiver, table, &rec_type);
            if (res != ERR_OK) return res;

            const char *name = expr->data.member.name;
            if (!name) {
                error_print(ERR_SEM_UNDEFINED, expr->pos, "Member name is missing");
                return ERR_SEM_UNDEFINED;
            }

            if (strcmp(name, "toString") == 0) {
                *out_type = type_get_string();
            } else if (strcmp(name, "toInt") == 0) {
                if (rec_type->kind != AST_TYPE_DOUBLE) {
                    error_print(ERR_SEM_TYPE, expr->pos,
                                "Member 'toInt' requires receiver of type Double, but got '%s'",
                                ast_type_to_string(rec_type->kind));
                    return ERR_SEM_TYPE;
                }
                *out_type = type_get_int();
            } else if (strcmp(name, "floor") == 0) {
                if (rec_type->kind != AST_TYPE_DOUBLE) {
                    error_print(ERR_SEM_TYPE, expr->pos,
                                "Member 'floor' requires receiver of type Double, but got '%s'",
                                ast_type_to_string(rec_type->kind));
                    return ERR_SEM_TYPE;
                }
                *out_type = type_get_double();
            } else if (strcmp(name, "length") == 0) {
                if (rec_type->kind != AST_TYPE_STRING) {
                    error_print(ERR_SEM_TYPE, expr->pos,
                                "Member 'length' requires receiver of type String, but got '%s'",
                                ast_type_to_string(rec_type->kind));
                    return ERR_SEM_TYPE;
                }
                *out_type = type_get_int();
            } else {
                // Check if this is a user extension method for rec_type
                AstFunction *ext_fn = symtable_lookup_extension(table, rec_type, name);
                if (ext_fn != NULL) {
                    // Must be a parameterless method defined without parentheses
                    if (ext_fn->parameters.count > 0) {
                        error_print(ERR_SEM_CALL, expr->pos,
                                    "Extension method '%s' defined with parameter list(s) must be called with parentheses",
                                    name);
                        return ERR_SEM_CALL;
                    }
                    *out_type = ext_fn->return_type ? ext_fn->return_type : type_get_unit();
                } else {
                    error_print(ERR_SEM_UNDEFINED, expr->pos,
                                "Unknown member '%s' for type '%s'",
                                name, ast_type_to_string(rec_type->kind));
                    return ERR_SEM_UNDEFINED;
                }
            }
            break;
        }

        case AST_EXPR_INTERPOLATED_STRING: {
            for (size_t i = 0; i < expr->data.interpolated_string.count; i++) {
                AstStringPart *part = &expr->data.interpolated_string.items[i];
                if (part->kind == AST_STRING_EXPRESSION) {
                    AstType *part_type = NULL;
                    res = check_expression(part->data.expression, table, &part_type);
                    if (res != ERR_OK) return res;
                }
            }
            *out_type = type_get_string();
            break;
        }

        case AST_EXPR_IF: {
            AstType *cond_type = NULL;
            res = check_expression(expr->data.conditional.condition, table, &cond_type);
            if (res != ERR_OK) return res;

            if (cond_type->kind != AST_TYPE_BOOLEAN) {
                error_print(ERR_SEM_TYPE, expr->pos,
                            "Condition in 'if' expression must be Boolean, but got '%s'",
                            ast_type_to_string(cond_type->kind));
                return ERR_SEM_TYPE;
            }

            symtable_push_scope(table);
            AstType *then_type = NULL;
            res = check_block(expr->data.conditional.then_branch, table, NULL, &then_type);
            symtable_pop_scope(table);
            if (res != ERR_OK) return res;

            if (expr->data.conditional.else_branch != NULL) {
                symtable_push_scope(table);
                AstType *else_type = NULL;
                res = check_block(expr->data.conditional.else_branch, table, NULL, &else_type);
                symtable_pop_scope(table);
                if (res != ERR_OK) return res;

                if (ast_type_equals(then_type, else_type)) {
                    *out_type = then_type;
                } else if (type_is_numeric(then_type) && type_is_numeric(else_type)) {
                    *out_type = type_get_double();
                } else {
                    error_print(ERR_SEM_TYPE, expr->pos,
                                "Incompatible branch types in 'if' expression: then '%s', else '%s'",
                                ast_type_to_string(then_type->kind),
                                ast_type_to_string(else_type->kind));
                    return ERR_SEM_TYPE;
                }
            } else {
                // BOOLTHEN: Missing else branch defaults to Unit ()
                *out_type = type_get_unit();
            }
            break;
        }

        default:
            error_print(ERR_SEM_OTHER, expr->pos, "Unsupported expression kind %d", expr->kind);
            return ERR_SEM_OTHER;
    }

    // Write evaluated type into AST node for Code Generator
    if (expr->evaluated_type != NULL) {
        ast_type_free(expr->evaluated_type);
    }
    expr->evaluated_type = ast_type_clone(*out_type);
    return ERR_OK;
}
