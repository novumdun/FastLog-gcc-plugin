#include <iostream>

#include "gcc-plugin.h"
#include "tree.h"
#include "plugin.h"
#include "tree-pass.h"
#include "context.h"
#include "coretypes.h"
#include "stmt.h"
#include "function.h"
#include "tree-pretty-print.h"
#include "tree-iterator.h"

// we have to specify plugin_is_GPL_compatible for every plugin
// if it is not defined a compilation error will follow later.
int plugin_is_GPL_compatible;

static void traverse_tree(tree node);

static void print_qualifiers(tree node) {
    if (TYPE_QUALS(node) & TYPE_QUAL_CONST) {
        std::cout << "const ";
    }
    // other qualifiers ...
}

static void print_integer(tree integer) {
    switch (TYPE_PRECISION(integer)) {
    case 8:
        std::cout << "char";
        break;
    case 16:
        std::cout << "short";
        break;
    case 32:
        std::cout << "int";
        break;
    case 64:
        std::cout << "long long";
        break;
    default:
        std::cout << "unknown integer type with precision: " << TYPE_PRECISION(integer);
    }
}

static void print_floating_point(tree floating_point) {
    if (TYPE_PRECISION(floating_point) == 32) {
        std::cout << "float";
    } else if (TYPE_PRECISION(floating_point) == 64) {
        std::cout << "double";
    } else {
        std::cout << "Floating point type with precision " << TYPE_PRECISION(floating_point);
    }
}

static void print_function_arguments(tree type);
static void print_type(tree type) {
    print_qualifiers(type);

    switch (TREE_CODE(type)) {
    case INTEGER_TYPE: {
        if (TYPE_UNSIGNED(type)) {
            std::cout << "unsigned ";
        }
        print_integer(type);
    } break;
    case REAL_TYPE:
        print_floating_point(type);
        break;
    case POINTER_TYPE:
        print_type(TREE_TYPE(type));
        std::cout << '*';
        break;
    case ARRAY_TYPE:
        print_type(TREE_TYPE(type));
        std::cout << "[]";
        break;
    case RECORD_TYPE: // classes and structs
        if (TYPE_NAME(type) && TREE_CODE(TYPE_NAME(type)) == TYPE_DECL) {
            std::cout << IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(type)));
        } else {
            std::cout << "unnamed struct or class";
        }
        break;
    case UNION_TYPE:
        if (TYPE_NAME(type) && TREE_CODE(TYPE_NAME(type)) == TYPE_DECL) {
            std::cout << IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(type)));
        } else {
            std::cout << "unnamed union";
        }
        break;
    case VOID_TYPE:
        std::cout << "void";
        break;
    case FUNCTION_TYPE:
        print_type(TREE_TYPE(type));
        print_function_arguments(type);
        break;
    case REFERENCE_TYPE:
        print_type(TREE_TYPE(type));
        std::cout << '&';
        break;
    default:
        std::cout << "unknown type: " << get_tree_code_name(TREE_CODE(type));
        break;
    }
    std::cout << ' ';
}

static void print_function_arguments(tree type) {
    if (TREE_CODE(type) == FUNCTION_TYPE) {
        std::cout << " args: (";
        for (tree arg = TYPE_ARG_TYPES(type); arg; arg = TREE_CHAIN(arg)) {
            print_type(TREE_VALUE(arg));
            if (TREE_CHAIN(arg)) {
                std::cout << ", ";
            }
        }
        std::cout << ")";
    }
}

static void print_call_expression(tree node) {
    tree call_expr_fn = CALL_EXPR_FN(node);
    tree func_decl = TREE_OPERAND(call_expr_fn, 0);
    std::cout << "callee: '" << IDENTIFIER_POINTER(DECL_NAME(func_decl)) << "'\n";
    char *func = "funcdddd";
    if (!memcmp(IDENTIFIER_POINTER(DECL_NAME(func_decl)), func, strlen(func))) {
        std::cout << "00000000000000000000";
    }

    const std::size_t argc = call_expr_nargs(node);
    std::cout << "arg count: " << argc << '\n';
    for (int i = 0; i < call_expr_nargs(node); ++i) {
        traverse_tree(CALL_EXPR_ARG(node, i));
    }

    tree return_type = TREE_TYPE(TREE_TYPE(func_decl));
    std::cout << "returns: " << get_tree_code_name(TREE_CODE(return_type)) << '\n';
}

static void statement(tree node) {
    std::cout << "statement " << get_tree_code_name(TREE_CODE(node)) << '\n';
    traverse_tree(DECL_EXPR_DECL(node));
}

static void expression(tree node) {
    switch (TREE_CODE(node)) {
    case EXPR_STMT:
        std::cout << "accessing node in expression statement\n";
        traverse_tree(TREE_OPERAND(node, 0));
        break;
    case CLEANUP_POINT_EXPR:
        std::cout << "accessing node in clean up point\n";
        traverse_tree(TREE_OPERAND(node, 0));
        break;
    case INIT_EXPR: {
        tree lhs = TREE_OPERAND(node, 0);
        tree rhs = TREE_OPERAND(node, 1);

        std::cout << "intializing variable lhs = " << get_tree_code_name(TREE_CODE(lhs))
                  << ", rhs = " << get_tree_code_name(TREE_CODE(rhs)) << '\n';

        traverse_tree(lhs);
        traverse_tree(rhs);
    } break;
    case MODIFY_EXPR:
        std::cout << "MODIFY_EXPR\n";
        traverse_tree(TREE_OPERAND(node, 0));
        traverse_tree(TREE_OPERAND(node, 1));
        break;
    case BIND_EXPR:
        std::cout << "BIND_EXPR_VARS\n";
        traverse_tree(BIND_EXPR_VARS(node));
        std::cout << "BIND_EXPR_BODY\n";
        traverse_tree(BIND_EXPR_BODY(node));
        break;
    case CALL_EXPR:
        print_call_expression(node);
        break;
    default:
        std::cout << "Unhandled expression: '" << get_tree_code_name(TREE_CODE(node)) << "'\n";
        break;
    }
}

static void print_name_with_scope(tree node) {
    std::string str;

    while (node && DECL_NAME(node) && TREE_CODE(node) != TRANSLATION_UNIT_DECL) {
        str = "::" + std::string(IDENTIFIER_POINTER(DECL_NAME(node))) + str;
        node = DECL_CONTEXT(node);
    }
    std::cout << str;
}

static void declaration(tree node) {
    tree del_type;
    switch (TREE_CODE(node)) {
    case FUNCTION_DECL:
        std::cout << "traversing through function tree\n";
        traverse_tree(DECL_SAVED_TREE(node));
        break;
    case VAR_DECL:
        print_name_with_scope(node);
        print_type(TREE_TYPE(node));
        if (DECL_INITIAL(node)) {
            traverse_tree(DECL_INITIAL(node));
        }
        std::cout << '\n';
        break;
    case RESULT_DECL:
        std::cout << "RESULT_DECL";
        print_type(TREE_TYPE(node));
        break;
    default:
        std::cout << "Unhandled declaration: '" << get_tree_code_name(TREE_CODE(node)) << "'\n";
        break;
    }
}

static void exceptional(tree node) {
    switch (TREE_CODE(node)) {
    case STATEMENT_LIST:
        std::cout << "iterating over statement list\n";
        for (tree_stmt_iterator tsi = tsi_start(node); !tsi_end_p(tsi); tsi_next(&tsi)) {
            traverse_tree(tsi_stmt(tsi));
        }
        break;
    default:
        break;
    }
}

static void unary(tree node) {
    switch (TREE_CODE(node)) {
    case CONVERT_EXPR:
        std::cout << "-- accessing tree in convert expression\n";
        traverse_tree(TREE_OPERAND(node, 0));
        break;
    default:
        print_type(TREE_TYPE(node));
        std::cout << "Unhandled unary: '" << get_tree_code_name(TREE_CODE(node)) << "'\n";
        break;
    }
}

static void constant(tree node) {
    switch (TREE_CODE(node)) {
    case VOID_CST:
        std::cout << "-- void constant, we're done now\n";
        break;
    case INTEGER_CST:
        std::cout << "-- integer constant: " << TREE_INT_CST_LOW(node) << '\n';
        break;
    default:
        std::cout << "Unhandled declaration: '" << get_tree_code_name(TREE_CODE(node)) << "'\n";
        break;
    }
}

static void traverse_tree(tree node) {
    switch (TREE_CODE_CLASS(TREE_CODE(node))) {
    case tcc_exceptional:
        exceptional(node);
        break;
    case tcc_constant:
        constant(node);
        break;
    case tcc_type:
        break;
    case tcc_declaration:
        declaration(node);
        break;
    case tcc_comparison:
        break;
    case tcc_unary:
        unary(node);
        break;
    case tcc_binary:
        break;
    case tcc_statement:
        statement(node);
        break;
    case tcc_vl_exp:
        expression(node);
        break;
    case tcc_expression:
        expression(node);
        break;
    default:
        std::cout << "Unhandled tree code class" << TREE_CODE_CLASS(TREE_CODE(node)) << ") '"
                  << get_tree_code_name(TREE_CODE(node)) << "'\n";
        break;
    }
}

static void pre_genericize_callback(void *gcc_data, void *user_data) {
    tree node = (tree)gcc_data;
    std::cout << "pre genericize callback: current node = '" << get_tree_code_name(TREE_CODE(node))
              << "'\n";
    traverse_tree(node);
}

int plugin_init(struct plugin_name_args *plugin_info, struct plugin_gcc_version *version) {
    register_callback(plugin_info->base_name, PLUGIN_PRE_GENERICIZE, pre_genericize_callback, NULL);
    return 0;
}