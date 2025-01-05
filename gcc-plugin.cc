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

int cnt = 0;
int macro_fd = -1;

#define TYPE_SET_UNSIGNED(type) (type | (0x1 << 4))

enum {
    type_none = 0,
    type_char,
    type_short,
    type_int,
    type_long_long,
    type_unsigned_char = TYPE_SET_UNSIGNED(type_char),
    type_unsigned_short = TYPE_SET_UNSIGNED(type_short),
    type_unsigned_int = TYPE_SET_UNSIGNED(type_int),
    type_unsigned_long_long = TYPE_SET_UNSIGNED(type_long_long),
    type_float,
    type_double,
    type_pointer,
    type_pointer_const_str,
};

#define FUNCSTART                                                                                  \
    do {                                                                                           \
        cnt++;                                                                                     \
    } while (0);

#define FUNCEND                                                                                    \
    do {                                                                                           \
        cnt--;                                                                                     \
    } while (0);

#define print(str)                                                                                 \
    do {                                                                                           \
        int ___i = 0;                                                                              \
        for (; ___i < cnt; ___i++) {                                                               \
            std::cout << "  ";                                                                     \
        }                                                                                          \
        std::cout << str << "\n";                                                                  \
    } while (0)

// we have to specify plugin_is_GPL_compatible for every plugin
// if it is not defined a compilation error will follow later.
int plugin_is_GPL_compatible;

static void traverse_tree(tree node);

static void print_qualifiers(tree node) {
    if (TYPE_QUALS(node) & TYPE_QUAL_CONST) {
        print("const ");
    }
    // other qualifiers ...
}

static void print_integer(tree integer) {
    switch (TYPE_PRECISION(integer)) {
    case 8:
        print("char");
        break;
    case 16:
        print("short");
        break;
    case 32:
        print("int");
        break;
    case 64:
        print("long long");
        break;
    default:
        print("unknown integer type with precision: " << TYPE_PRECISION(integer));
    }
}

static void print_floating_point(tree floating_point) {
    if (TYPE_PRECISION(floating_point) == 32) {
        print("float");
    } else if (TYPE_PRECISION(floating_point) == 64) {
        print("double");
    } else {
        print("Floating point type with precision " << TYPE_PRECISION(floating_point));
    }
}

static void print_function_arguments(tree type);
static void print_type(tree type) {
    FUNCSTART
    print("type: " << get_tree_code_name(TREE_CODE(type)));
    print_qualifiers(type);

    switch (TREE_CODE(type)) {
    case INTEGER_TYPE: {
        if (TYPE_UNSIGNED(type)) {
            print("unsigned ");
        }
        print_integer(type);
    } break;
    case REAL_TYPE:
        print_floating_point(type);
        break;
    case POINTER_TYPE:
        print_type(TREE_TYPE(type));
        print('*');
        break;
    case ARRAY_TYPE:
        print_type(TREE_TYPE(type));
        print("[]");
        break;
    case RECORD_TYPE: // classes and structs
        if (TYPE_NAME(type) && TREE_CODE(TYPE_NAME(type)) == TYPE_DECL) {
            print(IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(type))));
        } else {
            print("unnamed struct or class");
        }
        break;
    case UNION_TYPE:
        if (TYPE_NAME(type) && TREE_CODE(TYPE_NAME(type)) == TYPE_DECL) {
            print(IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(type))));
        } else {
            print("unnamed union");
        }
        break;
    case VOID_TYPE:
        print("void");
        break;
    case FUNCTION_TYPE:
        print_type(TREE_TYPE(type));
        print_function_arguments(type);
        break;
    case REFERENCE_TYPE:
        print_type(TREE_TYPE(type));
        print('&');
        break;

    default:
        print("unknown type: " << get_tree_code_name(TREE_CODE(type)));
        break;
    }
    FUNCEND
}

static bool get_qualifiers(tree node) {
    if (TYPE_QUALS(node) & TYPE_QUAL_CONST) {
        return true;
    }
    // other qualifiers ...
    return false;
}

static int get_integer(tree integer) {
    switch (TYPE_PRECISION(integer)) {
    case 8:
        return type_char;
        break;
    case 16:
        return type_short;
        break;
    case 32:
        return type_int;
        break;
    case 64:
        return type_long_long;
        break;
    default:
        print("unknown integer type with precision: " << TYPE_PRECISION(integer));
        return type_none;
    }
}

static int get_floating_point(tree floating_point) {
    if (TYPE_PRECISION(floating_point) == 32) {
        return type_float;
    } else if (TYPE_PRECISION(floating_point) == 64) {
        return type_double;
    } else {
        print("Floating point type with precision " << TYPE_PRECISION(floating_point));
        return type_none;
    }
}

static int get_type(tree node) {
    // print(" type: " << get_tree_code_name(TREE_CODE(node)));
    int _const = get_qualifiers(node);
    int type = type_none;

    switch (TREE_CODE(node)) {
    case INTEGER_TYPE:
        type = get_integer(node);
        if (TYPE_UNSIGNED(node)) {
            TYPE_SET_UNSIGNED(type);
        }
        break;
    case REAL_TYPE:
        type = get_floating_point(node);
        break;
    case POINTER_TYPE:
        type = get_type(TREE_TYPE(node));
        if (type_char == type) {
            type = type_pointer_const_str;
        }
        break;
    case ARRAY_TYPE:
        type = get_type(TREE_TYPE(node));
        break;
    case RECORD_TYPE: // classes and structs
        if (TYPE_NAME(node) && TREE_CODE(TYPE_NAME(node)) == TYPE_DECL) {
            print(IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(node))));
        } else {
            print("unnamed struct or class");
        }
        break;
    case UNION_TYPE:
        if (TYPE_NAME(node) && TREE_CODE(TYPE_NAME(node)) == TYPE_DECL) {
            print(IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(node))));
        } else {
            print("unnamed union");
        }
        break;
    case VOID_TYPE:
        print("void");
        break;
    case FUNCTION_TYPE:
        get_type(TREE_TYPE(node));
        print_function_arguments(node);
        break;
    case REFERENCE_TYPE:
        print_type(TREE_TYPE(node));
        print('&');
        break;

    default:
        print("unknown type: " << get_tree_code_name(TREE_CODE(node)));
        break;
    }
    print(' ');
}

static void print_function_arguments(tree node) {
    if (TREE_CODE(node) == FUNCTION_TYPE) {
        print(" args: (");
        for (tree arg = TYPE_ARG_TYPES(node); arg; arg = TREE_CHAIN(arg)) {
            print_type(TREE_VALUE(arg));
            if (TREE_CHAIN(arg)) {
                print(", ");
            }
        }
        print(")");
    }
}

static void print_call_expression(tree node) {
    FUNCSTART
    tree call_expr_fn = CALL_EXPR_FN(node);
    tree func_decl = TREE_OPERAND(call_expr_fn, 0);
    char func[] = "funcdddd";
    if (!memcmp(IDENTIFIER_POINTER(DECL_NAME(func_decl)), func, strlen(func))) {
        print(IDENTIFIER_POINTER(DECL_NAME(func_decl)));
    }

    const std::size_t argc = call_expr_nargs(node);
    for (int i = 0; i < call_expr_nargs(node); ++i) {
        print("arg" << i);
        print_type(TREE_TYPE(CALL_EXPR_ARG(node, i)));
        traverse_tree(CALL_EXPR_ARG(node, i));
    }

    tree return_type = TREE_TYPE(TREE_TYPE(func_decl));
    FUNCEND
}

static void get_call_expression(tree node) {
    FUNCSTART
    tree call_expr_fn = CALL_EXPR_FN(node);
    tree func_decl = TREE_OPERAND(call_expr_fn, 0);
    char func[] = "FASTLOG_MACRO_";
    if (!memcmp(IDENTIFIER_POINTER(DECL_NAME(func_decl)), func, strlen(func))) {
        print(IDENTIFIER_POINTER(DECL_NAME(func_decl)));

        const std::size_t argc = call_expr_nargs(node);
        for (int i = 0; i < call_expr_nargs(node); ++i) {
            print("arg" << i);
            get_type(TREE_TYPE(CALL_EXPR_ARG(node, i)));
            // traverse_tree(CALL_EXPR_ARG(node, i));
        }
    }

    tree return_type = TREE_TYPE(TREE_TYPE(func_decl));
    FUNCEND
}

static void statement(tree node) {
    FUNCSTART
    traverse_tree(DECL_EXPR_DECL(node));
    FUNCEND
}

static void expression(tree node) {
    FUNCSTART
    print("code " << get_tree_code_name(TREE_CODE(node)));
    switch (TREE_CODE(node)) {
    case EXPR_STMT:
        print("accessing node in expression statement");
        traverse_tree(TREE_OPERAND(node, 0));
        break;
    case CLEANUP_POINT_EXPR:
        print("accessing node in clean up point");
        traverse_tree(TREE_OPERAND(node, 0));
        break;
    case INIT_EXPR: {
        tree lhs = TREE_OPERAND(node, 0);
        tree rhs = TREE_OPERAND(node, 1);

        std::cout << "intializing variable lhs = " << get_tree_code_name(TREE_CODE(lhs))
                  << ", rhs = " << get_tree_code_name(TREE_CODE(rhs));

        traverse_tree(lhs);
        traverse_tree(rhs);
    } break;
    case MODIFY_EXPR:
        print("MODIFY_EXPR");
        traverse_tree(TREE_OPERAND(node, 0));
        traverse_tree(TREE_OPERAND(node, 1));
        break;
    case BIND_EXPR:
        print("BIND_EXPR_VARS");
        traverse_tree(BIND_EXPR_VARS(node));
        print("BIND_EXPR_BODY");
        traverse_tree(BIND_EXPR_BODY(node));
        break;
    case CALL_EXPR:
        print_call_expression(node);
        break;
    case PLUS_EXPR:
        print_type(TREE_TYPE(node));
        break;
    case ADDR_EXPR:
        print_type(TREE_TYPE(node));
        // print_type((node));
        // traverse_tree((node));
        break;
    default:
        print("Unhandled expression: '" << get_tree_code_name(TREE_CODE(node)));
        break;
    }
    FUNCEND
}

static void declaration(tree node) {
    FUNCSTART
    tree del_type;
    switch (TREE_CODE(node)) {
    case FUNCTION_DECL:
        traverse_tree(DECL_SAVED_TREE(node));
        break;
    case VAR_DECL:
        print(IDENTIFIER_POINTER(DECL_NAME(node)));
        if (DECL_INITIAL(node)) {
            traverse_tree(DECL_INITIAL(node));
        }
        break;
    case RESULT_DECL:
        break;
    default:
        print("Unhandled declaration: '" << get_tree_code_name(TREE_CODE(node)));
        break;
    }
    FUNCEND
}

static void exceptional(tree node) {
    FUNCSTART
    switch (TREE_CODE(node)) {
    case STATEMENT_LIST:
        print("iterating over statement list");
        for (tree_stmt_iterator tsi = tsi_start(node); !tsi_end_p(tsi); tsi_next(&tsi)) {
            traverse_tree(tsi_stmt(tsi));
        }
        break;
    default:
        break;
    }
    FUNCEND
}

static void unary(tree node) {
    switch (TREE_CODE(node)) {
    case CONVERT_EXPR:
        print("-- accessing tree in convert expression");
        traverse_tree(TREE_OPERAND(node, 0));
        break;
    case NOP_EXPR:
        traverse_tree(TREE_OPERAND(node, 0));
        break;
    default:
        print_type(TREE_TYPE(node));
        print("Unhandled unary: '" << get_tree_code_name(TREE_CODE(node)));
        break;
    }
}

static void constant(tree node) {
    switch (TREE_CODE(node)) {
    case VOID_CST:
        print("-- void constant, we're done now");
        break;
    case INTEGER_CST:
        print("-- integer constant: " << TREE_INT_CST_LOW(node));
        break;
    case REAL_CST:
        TREE_REAL_CST(node);
        break;
    default:
        print("Unhandled declaration: '" << get_tree_code_name(TREE_CODE(node)));
        break;
    }
}

static void traverse_tree(tree node) {
    if (NULL == node) {
        return;
    }
    FUNCSTART
    print("class " << TREE_CODE_CLASS_STRING(TREE_CODE_CLASS(TREE_CODE(node))));
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
        expression(node);
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
        print("Unhandled tree code class:" << TREE_CODE_CLASS(TREE_CODE(node))
                                           << " type:" << get_tree_code_name(TREE_CODE(node)));
        break;
    }
    FUNCEND
}

static void pre_genericize_callback(void *gcc_data, void *user_data) {
    tree node = (tree)gcc_data;
    traverse_tree(node);
}

int plugin_init(struct plugin_name_args *plugin_info, struct plugin_gcc_version *version) {

    int fd = open("/home/dengbo/workspace/cabin/FastLog-gcc-plugin/test.macro.h", O_RDWR | O_CREAT,
                  0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    macro_fd = fd;

    register_callback(plugin_info->base_name, PLUGIN_PRE_GENERICIZE, pre_genericize_callback, NULL);
    return 0;
}