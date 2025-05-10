#include <iostream>
#include <string.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>

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
const char *file_path;
const char *proj_path;
int macro_fd = -1;
int idx_fd = -1;
int inc_fd = -1;
int macros_fd = -1;
int rec_fd = -1;

#define TYPE_SET_UNSIGNED(type) (type + 1)

enum {
    type_none = 0,
    type_char,
    type_unsigned_char,
    type_short,
    type_unsigned_short,
    type_int,
    type_unsigned_int,
    type_long_long,
    type_unsigned_long_long,
    type_float,
    type_double,
    type_pointer,
    type_pointer_char,
    type_string,
};

const char *(type_str[]) = {
    [type_none] = "none",
    [type_char] = "char",
    [type_unsigned_char] = "unsigned char",
    [type_short] = "short",
    [type_unsigned_short] = "unsigned short",
    [type_int] = "int",
    [type_unsigned_int] = "unsigned int",
    [type_long_long] = "long long",
    [type_unsigned_long_long] = "unsigned long long",
    [type_float] = "float",
    [type_double] = "double",
    [type_pointer] = "void *",
    [type_pointer_char] = "char *",
    [type_string] = "string ",
};

int(type_size[]) = {
    [type_none] = 0,         [type_char] = 1,           [type_unsigned_char] = 1,
    [type_short] = 2,        [type_unsigned_short] = 2, [type_int] = 4,
    [type_unsigned_int] = 4, [type_long_long] = 8,      [type_unsigned_long_long] = 8,
    [type_float] = 4,        [type_double] = 8,         [type_pointer] = 8,
    [type_pointer_char] = 8, [type_string] = 0,
};

typedef struct macro_rec {
    LIST_ENTRY(macro_rec) list;
    const char *macro_name;
    const char *fmt;
    char argc;
    char argts[16];
    char argss[16];
    const char *arg_str[16];
} macro_rec_t;

LIST_HEAD(listhead, macro_rec);
struct listhead macro_rec_head;
int macro_rec_cnt;

const char *type_to_str(int type) {
    return type_str[type];
}

#define FUNCSTART                                                                                  \
    do {                                                                                           \
        cnt++;                                                                                     \
    } while (0);

#define FUNCEND                                                                                    \
    do {                                                                                           \
        cnt--;                                                                                     \
    } while (0);

#ifdef DEBUG
#define print(str)                                                                                 \
    do {                                                                                           \
        int ___i = 0;                                                                              \
        for (; ___i < cnt; ___i++) {                                                               \
            std::cout << "  ";                                                                     \
        }                                                                                          \
        std::cout << str << "\n";                                                                  \
    } while (0)
#else
#define print(str)
#endif

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
    FUNCSTART
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
    FUNCEND
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
    print(__func__ << " type " << get_tree_code_name(TREE_CODE(type)));
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
    FUNCSTART
        print("type: " << get_tree_code_name(TREE_CODE(node)));
    // int _const = get_qualifiers(node);
    int type = type_none;

    switch (TREE_CODE(node)) {
    case INTEGER_TYPE:
        type = get_integer(node);
        if (TYPE_UNSIGNED(node)) {
            type = TYPE_SET_UNSIGNED(type);
        }
        break;
    case REAL_TYPE:
        type = get_floating_point(node);
        break;
    case POINTER_TYPE:
        type = get_type(TREE_TYPE(node));
        if (type_char == type) {
            type = type_pointer_char;
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

    FUNCEND
    return type;
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

void put_str(char *buf, int *pos, const char *str) {
    strcpy(&buf[*pos], str);
    *pos += strlen(str);
    return;
}

void make_macro(struct macro_rec *np) {
    const char *func = np->macro_name;
    int argc = np->argc;
    char *argts = np->argts;
    char buf[8192];
    int pos = 0;
    char str[2048];

    sprintf(str, "#define %s(", func);
    put_str(buf, &pos, str);

    for (int i = 0; i < argc; i++) {
        sprintf(str, "arg%d,", i);
        put_str(buf, &pos, str);
    }
    buf[pos - 1] = ')';

    sprintf(str, "extern unsigned short %s_log_idx; {", func);
    put_str(buf, &pos, str);

    if (NULL == np->fmt) {
        sprintf(str, "int len_arg0= strlen(arg0)+1; ");
    } else {
        sprintf(str, "int len_arg0= 0; ");
    }
    put_str(buf, &pos, str);

    sprintf(str,
            "struct autogen_%s_log_stru { "
            "unsigned short len; "
            "unsigned short idx; ",
            func);
    put_str(buf, &pos, str);

    if (NULL == np->fmt) {
        sprintf(str, "char member0[len_arg0]; ");
        put_str(buf, &pos, str);
    }

    for (int i = 1; i < argc; i++) {
        sprintf(str, "%s member%d; ", type_to_str(argts[i]), i);
        put_str(buf, &pos, str);
    }

    sprintf(str, " } __attribute__((__packed__)) *stru_p;");
    put_str(buf, &pos, str);

    sprintf(str, "int buf_size = len_arg0 + 2*sizeof(unsigned short)", func);
    put_str(buf, &pos, str);
    for (int i = 1; i < argc; i++) {
        sprintf(str, "+sizeof(stru_p->member%d) ", i);
        put_str(buf, &pos, str);
    }
    sprintf(str, ";");
    put_str(buf, &pos, str);

    sprintf(str, "char stru_buf[buf_size]; stru_p = (struct autogen_%s_log_stru *)stru_buf; ",
            func);
    put_str(buf, &pos, str);

    sprintf(str,
            "stru_p->len = buf_size;"
            "stru_p->idx = %s_log_idx + FASTLOG_MODULE_BASE_IDX;",
            func);
    put_str(buf, &pos, str);

    for (int i = 1; i < argc; i++) {
        sprintf(str, "stru_p->member%d = (arg%d); ", i, i);
        put_str(buf, &pos, str);
    }

    if (NULL == np->fmt) {
        sprintf(str, "memcpy(stru_p->member0, arg0, len_arg0); ");
        put_str(buf, &pos, str);
    }

    sprintf(str, "fastlog_write(fastlog_fd, (void *)stru_p, buf_size);};\n\n");

    put_str(buf, &pos, str);

    write(macro_fd, buf, pos);

    // print(buf);
}

void make_idx(struct macro_rec *np) {
    char str[2048];

    if (np->fmt) {
        sprintf(str,
                "unsigned short %s_log_idx = __COUNTER__; "
                "LOG_IDX_FAKE(\"%s\",\"%s\",%d",
                np->macro_name, file_path, np->fmt, np->argc);
    } else {
        sprintf(str,
                "unsigned short %s_log_idx = __COUNTER__; "
                "LOG_IDX_FAKE(\"%s\",%s,%d",
                np->macro_name, file_path, "-1", np->argc);
    }

    sprintf(str + strlen(str),
            ",%d,%d,%d,%d"
            ",%d,%d,%d,%d"
            ",%d,%d,%d,%d"
            ",%d,%d,%d,%d"
            ",%d,%d,%d,%d"
            ",%d,%d,%d,%d);\n",

            np->argts[0], np->argts[1], np->argts[2], np->argts[3], np->argts[4], np->argts[5],
            np->argts[6], np->argts[7], np->argts[8], np->argts[9], np->argts[10], np->argts[11],

            np->argss[0], np->argss[1], np->argss[2], np->argss[3], np->argss[4], np->argss[5],
            np->argss[6], np->argss[7], np->argss[8], np->argss[9], np->argss[10], np->argss[11]);

    write(idx_fd, str, strlen(str));
}

#define _TO_STRING(x) #x
#define TO_STRING(x) _TO_STRING(x)
const char *get_call_exp_fmt(tree node) {
    if (NOP_EXPR == TREE_CODE(node)) {
        tree nop_expr = node;
        if (ADDR_EXPR == TREE_CODE(TREE_OPERAND(nop_expr, 0))) {
            tree addr_expr = TREE_OPERAND(nop_expr, 0);
            if (STRING_CST == TREE_CODE(TREE_OPERAND(addr_expr, 0))) {
                tree string_cst = TREE_OPERAND(addr_expr, 0);
                if (NULL != TREE_STRING_POINTER(string_cst)) {
                    return TREE_STRING_POINTER(string_cst);
                }
            }
        }
    }
    return NULL;
}

static void get_call_expression(tree node) {
    FUNCSTART
    tree call_expr_fn = CALL_EXPR_FN(node);
    tree func_decl = TREE_OPERAND(call_expr_fn, 0);
    char func[] = "FASTLOG_MACRO_";

    const char *func_name = IDENTIFIER_POINTER(DECL_NAME(func_decl));

    if (!memcmp(func_name, func, strlen(func))) {
        print(func_name);

        struct macro_rec *ptr = new (struct macro_rec);
        memset(ptr, 0, sizeof(struct macro_rec));
        LIST_INSERT_HEAD(&macro_rec_head, ptr, list);
        macro_rec_cnt++;

        ptr->macro_name = func_name;
        ptr->fmt = get_call_exp_fmt(CALL_EXPR_ARG(node, 0));
        // print(ptr->fmt);
        const std::size_t argc = call_expr_nargs(node);
        ptr->argc = argc;
        for (int i = 0; i < call_expr_nargs(node); ++i) {
            print("arg" << i);
            int type = get_type(TREE_TYPE(CALL_EXPR_ARG(node, i)));
            // if (type_pointer_char == type) {
            //     const char *str = get_call_exp_fmt(CALL_EXPR_ARG(node, i));
            //     if (NULL != str) {
            //         ptr->arg_str[i] = str;
            //         type = type_string;
            //     }
            // }
            ptr->argts[i] = type;
            ptr->argss[i] = type_size[type];
            traverse_tree(CALL_EXPR_ARG(node, i));
        }
    }

    tree return_type = TREE_TYPE(TREE_TYPE(func_decl));
    FUNCEND
}

static void statement(tree node) {
    FUNCSTART
    print(__func__ << " type " << get_tree_code_name(TREE_CODE(node)));
    traverse_tree(DECL_EXPR_DECL(node));
    FUNCEND
}

#if 0
static void expression(tree node) {
    FUNCSTART
    print(__func__ << " type " << get_tree_code_name(TREE_CODE(node)));
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
#else
static void expression(tree node) {
    FUNCSTART
    print(__func__ << " type " << get_tree_code_name(TREE_CODE(node)));
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
        get_call_expression(node);
        break;
    case PLUS_EXPR:
        get_type(TREE_TYPE(node));
        break;
    case ADDR_EXPR:
        get_type(TREE_TYPE(node));
        break;
    default:
        print("Unhandled expression: '" << get_tree_code_name(TREE_CODE(node)));
        break;
    }
    FUNCEND
}
#endif

static void declaration(tree node) {
    FUNCSTART
    print(__func__ << " type " << get_tree_code_name(TREE_CODE(node)));
    switch (TREE_CODE(node)) {
    case FUNCTION_DECL:
        // print(IDENTIFIER_POINTER(DECL_NAME(node)));
        if (!strcmp("fastlog_tmp_func", IDENTIFIER_POINTER(DECL_NAME(node)))) {
            tree bind_expr = DECL_SAVED_TREE(node);
            tree bind_expr_body = BIND_EXPR_BODY(bind_expr);
            tree statement_list = bind_expr_body;

            for (tree_stmt_iterator tsi = tsi_start(statement_list); !tsi_end_p(tsi);
                 tsi_next(&tsi)) {
                tree _statement = tsi_stmt(tsi);
                tree _var_decl = DECL_EXPR_DECL(_statement);
                if (_var_decl && DECL_INITIAL(_var_decl)) {
                    const char *_var_name = IDENTIFIER_POINTER(DECL_NAME(_var_decl));
                    tree _string_cst = DECL_INITIAL(_var_decl);
                    if (!strcmp("file_path", _var_name)) {
                        file_path = TREE_STRING_POINTER(_string_cst);
                    } else if (!strcmp("proj_path", _var_name)) {
                        proj_path = TREE_STRING_POINTER(_string_cst);
                    }
                }
            }
            break;
        }
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
    print(__func__ << " type " << get_tree_code_name(TREE_CODE(node)));
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
    FUNCSTART
    print(__func__ << " type " << get_tree_code_name(TREE_CODE(node)));
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
    FUNCEND
}

static void constant(tree node) {
    FUNCSTART
    print(__func__ << " type " << get_tree_code_name(TREE_CODE(node)));
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
    case STRING_CST:
        print(TREE_STRING_POINTER(node));
        break;
    default:
        print("Unhandled declaration: '" << get_tree_code_name(TREE_CODE(node)));
        break;
    }
    FUNCEND
}

static void traverse_tree(tree node) {
    FUNCSTART
    if (NULL == node) {
        print("null node ");
        FUNCEND
        return;
    }

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

static void _mkdir(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    mkdir(tmp, S_IRWXU);
}

static void finish_callback(void *gcc_data, void *user_data) {
    int i;
    for (i = 0; i < strlen(proj_path); i++) {
        if (proj_path[i] != file_path[i]) {
            break;
        }
    }
    i++;

    int j;
    for (j = strlen(&file_path[i]); j > 0; j--) {
        if (file_path[i + j - 1] == '/') {
            break;
        }
    }
    j--;

    char rela_path[128];
    memcpy(rela_path, &file_path[i], j);
    rela_path[j] = '\0';

    char out_fd_dir[1024];
    snprintf(out_fd_dir, sizeof(out_fd_dir), "%s/fastlog_out/%s", proj_path, rela_path);
    printf(out_fd_dir);
    _mkdir(out_fd_dir);

    char macro_fd_path[1024];
    snprintf(macro_fd_path, sizeof(macro_fd_path), "%s/fastlog_out/%s.macro.h", proj_path,
             &file_path[i]);
    int fd = open(macro_fd_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    macro_fd = fd;

    char src_fd_path[1024];
    snprintf(src_fd_path, sizeof(src_fd_path), "%s/fastlog_out/%s.inc", proj_path, &file_path[i]);
    // printf(src_fd_path);
    fd = open(src_fd_path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    idx_fd = fd;
    const char *str = "#include \"c.inc.h\"\r\n";
    write(idx_fd, str, strlen(str));

    char inc_fd_path[1024];
    snprintf(inc_fd_path, sizeof(inc_fd_path), "%s/%s/log_inc.c", proj_path, rela_path);
    fd = open(inc_fd_path, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    inc_fd = fd;
    char buf[1024];
    snprintf(buf, sizeof(buf), "#include \"%s\"\n", src_fd_path);
    write(inc_fd, buf, strlen(buf));

    char macros_fd_path[1024];
    snprintf(macros_fd_path, sizeof(macros_fd_path), "%s/%s/log_macro.h", proj_path, rela_path);
    fd = open(macros_fd_path, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    macros_fd = fd;
    snprintf(buf, sizeof(buf), "#include \"%s\"\n", macro_fd_path);
    write(macros_fd, buf, strlen(buf));

    char rec_fd_path[1024];
    snprintf(rec_fd_path, sizeof(rec_fd_path), "%s/%s/log_rec", proj_path, rela_path);
    fd = open(rec_fd_path, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    rec_fd = fd;

    struct macro_rec *np;
    LIST_FOREACH(np, &macro_rec_head, list) {
        make_macro(np);
        make_idx(np);
    }
    snprintf(buf, sizeof(buf), "// FASTLOG_MODULE_SIZE_ %d\n", macro_rec_cnt);
    write(idx_fd, buf, strlen(buf));
}

int plugin_init(struct plugin_name_args *plugin_info, struct plugin_gcc_version *version) {
    LIST_INIT(&macro_rec_head);
    register_callback(plugin_info->base_name, PLUGIN_PRE_GENERICIZE, pre_genericize_callback, NULL);
    register_callback(plugin_info->base_name, PLUGIN_FINISH, finish_callback, NULL);
    return 0;
}