#include <iostream>
#include <string>
#include <set>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <new>
#include <sys/queue.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#define KB  1024

int cnt = 0;
const char *file_path;
const char *proj_path;
int macro_fd = -1;
int idx_fd = -1;
int inc_fd = -1;
int macros_fd = -1;

#define TYPE_SET_UNSIGNED(type) (type + 1)

/*
 * NOTE: new ids are only appended after type_string so that the numeric ids
 * already written to the generated *.inc files keep their meaning.
 *
 * Every type_string* value means "the content of this char* argument is known
 * at compile time and is stored in macro_rec::arg_str[i]".
 * A char* that can not be resolved stays type_pointer_char (copied at run time).
 */
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
    type_pointer_char,             // char * with run-time content
    type_string,                   // printf("abc")   (also LINE_STR, which is one literal)

    type_string_func,              // __func__
    type_string_FUNCTION,          // __FUNCTION__
    type_string_PRETTY_FUNCTION,   // __PRETTY_FUNCTION__

    type_string_const_array,       // const char s[] = "..."; printf(s)
    type_string_mutable_array,     // char s[] = "...";  s is not touched before printf(s)

    type_string_ptr_literal,       // const char *p = "...";           printf(p)
    type_string_ptr_const_array,   // const char s[]="..."; p = s;     printf(p)
    type_string_ptr_mutable_array, // char s[]="..."; p = s; only p uses s; printf(p)
};

#define IS_STRING_TYPE(t)                                                                          \
    ((t) >= type_string && (t) <= type_string_ptr_mutable_array)

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
    [type_string_func] = "string(__func__)",
    [type_string_FUNCTION] = "string(__FUNCTION__)",
    [type_string_PRETTY_FUNCTION] = "string(__PRETTY_FUNCTION__)",
    [type_string_const_array] = "string(const array)",
    [type_string_mutable_array] = "string(mutable array)",
    [type_string_ptr_literal] = "string(ptr to literal)",
    [type_string_ptr_const_array] = "string(ptr to const array)",
    [type_string_ptr_mutable_array] = "string(ptr to mutable array)",
};

int(type_size[]) = {
    [type_none] = 0,         [type_char] = 1,           [type_unsigned_char] = 1,
    [type_short] = 2,        [type_unsigned_short] = 2, [type_int] = 4,
    [type_unsigned_int] = 4, [type_long_long] = 8,      [type_unsigned_long_long] = 8,
    [type_float] = 4,        [type_double] = 8,         [type_pointer] = (int)sizeof(void *),
    [type_pointer_char] = 0, [type_string] = 0,
    [type_string_func] = 0,
    [type_string_FUNCTION] = 0,
    [type_string_PRETTY_FUNCTION] = 0,
    [type_string_const_array] = 0,
    [type_string_mutable_array] = 0,
    [type_string_ptr_literal] = 0,
    [type_string_ptr_const_array] = 0,
    [type_string_ptr_mutable_array] = 0,
};

typedef struct macro_rec {
    STAILQ_ENTRY(macro_rec) list;
    const char *macro_name;
    char argc;
    char argts[16];
    char argss[16];
    const char *arg_str[16];
} macro_rec_t;

STAILQ_HEAD(listhead, macro_rec);
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

// function currently handed to PLUGIN_PRE_GENERICIZE
static tree cur_fndecl;

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
    char buf[16 * KB];
    int pos = 0;
    char str[KB];

    char arg_list[2 * KB];
    pos = 0;
    for (int i = 0; i < argc; i++) {
        sprintf(str, "arg%d,", i);
        put_str(arg_list, &pos, str);
    }
    arg_list[pos - 1] = '\0';

    char len_arg[2 * KB] = {0};
    pos = 0;
    for (int i = 0; i < argc; i++) {
        if (type_pointer_char == np->argts[i]) {
            sprintf(str, "int len_arg%d = strlen(arg%d)+1; ", i, i);
            put_str(len_arg, &pos, str);
        }
    }

    char member[2 * KB];
    pos = 0;
    for (int i = 0; i < argc; i++) {
        if (type_pointer_char == np->argts[i]) {
            sprintf(str, "char member%d[len_arg%d]; ", i, i);
        } else if (IS_STRING_TYPE(np->argts[i])) {
            continue;
        } else {
            sprintf(str, "%s member%d; ", type_to_str(argts[i]), i);
        }
        put_str(member, &pos, str);
    }

    char member_len[2 * KB];
    pos = 0;
    for (int i = 0; i < argc; i++) {
        if (type_pointer_char == np->argts[i]) {
            sprintf(str, "+len_arg%d", i);
        } else if (IS_STRING_TYPE(np->argts[i])) {
            continue;
        } else {
            sprintf(str, "+sizeof(stru_p->member%d) ", i);
        }
        put_str(member_len, &pos, str);
    }

    char member_set[2 * KB];
    pos = 0;
    for (int i = 0; i < argc; i++) {
        if (type_pointer_char == np->argts[i]) {
            sprintf(str, "memcpy(stru_p->member%d, arg%d, len_arg%d); ", i, i, i);
        } else if (IS_STRING_TYPE(np->argts[i])) {
            continue;
        } else {
            sprintf(str, "stru_p->member%d = (arg%d); ", i, i);
        }
        put_str(member_set, &pos, str);
    }

    const char *format = 
    "#define %s(log, %s)                                                                                                \\\n"
    "extern unsigned short %s_log_idx;                                                                                  \\\n"
    "{                                                                                                                  \\\n"
    "    %s                                                                                                             \\\n"
    "    struct                                                                                                         \\\n"
    "        autogen_%s_log_stru {                                                                                      \\\n"
    "        unsigned short len;                                                                                        \\\n"
    "        unsigned int idx;                                                                                          \\\n"
    "        %s                                                                                                         \\\n"
    "    } __attribute__((__packed__)) * stru_p;                                                                        \\\n"
    "    int buf_size = sizeof(unsigned short) + sizeof(unsigned int) %s;                                               \\\n"
    "    char stru_buf[buf_size];                                                                                       \\\n"
    "    stru_p =(struct autogen_%s_log_stru *)stru_buf;                                                                \\\n"
    "    stru_p->len = buf_size;                                                                                        \\\n"
    "    stru_p->idx = %s_log_idx + fastlog_get_idx_base(log);                                                          \\\n"
    "    %s                                                                                                             \\\n"
    "    fastlog_t *_log = log;                                                                                         \\\n"
    "    fastlog_write(_log, (void *)stru_p, buf_size);                                                                 \\\n"
    "};\n\n";
    snprintf(buf, sizeof(buf) - 1, format, func, arg_list, func, len_arg,  func, member, member_len , func, func, member_set);

    write(macro_fd, buf, strlen(buf));

    // print(buf);
}

/* Escape a string so that it can be placed between "" in generated C code. */
static std::string c_escape(const char *s) {
    std::string o;
    if (!s) {
        return o;
    }
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '\n':
            o += "\\n";
            break;
        case '\t':
            o += "\\t";
            break;
        case '\r':
            o += "\\r";
            break;
        case '"':
            o += "\\\"";
            break;
        case '\\':
            o += "\\\\";
            break;
        default:
            if (c < 0x20 || c >= 0x7f) {
                char b[8];
                snprintf(b, sizeof(b), "\\%03o", c);
                o += b;
            } else {
                o += (char)c;
            }
        }
    }
    return o;
}

void make_idx(struct macro_rec *np) {
    std::string out;
    char tmp[256];

    snprintf(tmp, sizeof(tmp), "unsigned short %s_log_idx = __COUNTER__; LOG_IDX_FAKE(\"",
             np->macro_name);
    out += tmp;
    out += c_escape(file_path);
    snprintf(tmp, sizeof(tmp), "\", %d", np->argc);
    out += tmp;

    for (int i = 0; i < 16; i++) {
        snprintf(tmp, sizeof(tmp), ",%d", np->argts[i]);
        out += tmp;
    }
    for (int i = 0; i < 16; i++) {
        snprintf(tmp, sizeof(tmp), ",%d", np->argss[i]);
        out += tmp;
    }
    for (int i = 0; i < 16; i++) {
        out += ",\"";
        out += c_escape(np->arg_str[i]);
        out += "\"";
    }
    out += ");\n";

    write(idx_fd, out.c_str(), out.size());
}

#define _TO_STRING(x) #x
#define TO_STRING(x) _TO_STRING(x)

/* Old helper: only recognises (char *)&"literal". Kept for reference, no longer used. */
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

/* ====================================================================== *
 *  Compile-time resolution of a char * argument
 * ====================================================================== */

struct str_info {
    int type;
    const char *content;
};

static tree strip_nops(tree t) {
    while (t && (CONVERT_EXPR_P(t) || TREE_CODE(t) == NON_LVALUE_EXPR ||
                 TREE_CODE(t) == VIEW_CONVERT_EXPR)) {
        t = TREE_OPERAND(t, 0);
    }
    return t;
}

/* &"abc", &"abc"[0], &arr, &arr[0]  ->  STRING_CST / VAR_DECL */
static tree addr_to_object(tree t) {
    t = strip_nops(t);
    if (!t || TREE_CODE(t) != ADDR_EXPR) {
        return NULL_TREE;
    }
    tree obj = TREE_OPERAND(t, 0);
    while (TREE_CODE(obj) == ARRAY_REF && integer_zerop(TREE_OPERAND(obj, 1))) {
        obj = TREE_OPERAND(obj, 0);
    }
    return obj;
}

/*
 * Scan the body of cur_fndecl and collect, for one local declaration:
 *   refs - number of times it is referenced (declaration itself and its own
 *          initializer statement are not references)
 *   init - initializer expression, when the front end emitted it as a separate
 *          INIT_EXPR instead of DECL_INITIAL
 * Over-counting is harmless (result becomes "dynamic"), under-counting is not,
 * so no visited-set is used when walking.
 */
struct decl_scan {
    tree decl;
    int refs;
    tree init;
    std::set<tree> inited;
};

static tree scan_cb(tree *tp, int *walk_subtrees, void *data);

static void scan_decl_init(tree d, decl_scan *s) {
    if (!d || !VAR_P(d) || !DECL_INITIAL(d)) {
        return;
    }
    if (!s->inited.insert(d).second) {
        return; // already walked through another path (DECL_EXPR / BIND_EXPR_VARS)
    }
    walk_tree(&DECL_INITIAL(d), scan_cb, s, NULL);
}

static tree scan_cb(tree *tp, int *walk_subtrees, void *data) {
    decl_scan *s = (decl_scan *)data;
    tree t = *tp;

    if (t == s->decl) {
        s->refs++;
        return NULL_TREE;
    }

    switch (TREE_CODE(t)) {
    case DECL_EXPR:
        scan_decl_init(DECL_EXPR_DECL(t), s);
        *walk_subtrees = 0;
        break;
    case BIND_EXPR:
        for (tree v = BIND_EXPR_VARS(t); v; v = DECL_CHAIN(v)) {
            scan_decl_init(v, s);
        }
        walk_tree(&BIND_EXPR_BODY(t), scan_cb, s, NULL);
        *walk_subtrees = 0;
        break;
    case INIT_EXPR:
        if (TREE_OPERAND(t, 0) == s->decl && !s->init) {
            s->init = TREE_OPERAND(t, 1);
            walk_tree(&TREE_OPERAND(t, 1), scan_cb, s, NULL);
            *walk_subtrees = 0;
        }
        break;
    default:
        break;
    }
    return NULL_TREE;
}

static void scan_decl(tree decl, decl_scan &s) {
    s.decl = decl;
    s.refs = 0;
    s.init = NULL_TREE;
    if (!cur_fndecl || !DECL_SAVED_TREE(cur_fndecl)) {
        s.refs = 1 << 20; // unknown -> treated as modified
        return;
    }
    walk_tree(&DECL_SAVED_TREE(cur_fndecl), scan_cb, &s, NULL);
}

/* 0: not a function-name decl, otherwise the matching type_string_* id */
static int func_name_type(tree d) {
    if (!DECL_ARTIFICIAL(d) || !DECL_NAME(d)) {
        return 0;
    }
    const char *n = IDENTIFIER_POINTER(DECL_NAME(d));
    if (!strcmp(n, "__func__")) {
        return type_string_func;
    }
    if (!strcmp(n, "__FUNCTION__")) {
        return type_string_FUNCTION;
    }
    if (!strcmp(n, "__PRETTY_FUNCTION__")) {
        return type_string_PRETTY_FUNCTION;
    }
    return 0;
}

static bool array_is_const(tree decl) {
    return TREE_READONLY(decl) || TYPE_READONLY(TREE_TYPE(TREE_TYPE(decl)));
}

static const char *string_cst_dup(tree str) {
    // TREE_STRING_LENGTH includes the terminating NUL (and may include padding)
    const char *p = TREE_STRING_POINTER(str);
    size_t len = strnlen(p, (size_t)TREE_STRING_LENGTH(str));
    char *r = new char[len + 1]; // GCC headers poison malloc/strdup
    memcpy(r, p, len);
    r[len] = '\0';
    return r;
}

static const char *cstr_dup(const char *s) {
    size_t len = strlen(s);
    char *r = new char[len + 1];
    memcpy(r, s, len + 1);
    return r;
}

static bool resolve_object(tree obj, bool via_ptr, str_info &si) {
    if (!obj) {
        return false;
    }

    /* "abc" */
    if (TREE_CODE(obj) == STRING_CST) {
        si.type = via_ptr ? type_string_ptr_literal : type_string;
        si.content = string_cst_dup(obj);
        return true;
    }

    if (!VAR_P(obj) || TREE_CODE(TREE_TYPE(obj)) != ARRAY_TYPE) {
        return false;
    }

    /* __func__ / __FUNCTION__ / __PRETTY_FUNCTION__ */
    int fn_type = func_name_type(obj);
    if (fn_type) {
        tree init = strip_nops(DECL_INITIAL(obj));
        if (init && TREE_CODE(init) == STRING_CST) {
            si.type = fn_type;
            si.content = string_cst_dup(init);
            return true;
        }
        // The initializer may not be available yet; plain function name is safe
        // for __func__ / __FUNCTION__, but not for __PRETTY_FUNCTION__ (C++ signature).
        if (fn_type != type_string_PRETTY_FUNCTION && cur_fndecl && DECL_NAME(cur_fndecl)) {
            si.type = fn_type;
            si.content = cstr_dup(IDENTIFIER_POINTER(DECL_NAME(cur_fndecl)));
            return true;
        }
        return false;
    }

    bool is_const = array_is_const(obj);
    bool is_static = TREE_STATIC(obj) || DECL_EXTERNAL(obj);

    tree init = DECL_INITIAL(obj);
    int refs = 0;
    if (!is_static) {
        decl_scan s;
        scan_decl(obj, s);
        refs = s.refs;
        if (!init) {
            init = s.init;
        }
    }
    init = strip_nops(init);
    if (!init || TREE_CODE(init) != STRING_CST) {
        return false; // brace initializer, extern, uninitialised, ...
    }

    if (is_const) {
        si.type = via_ptr ? type_string_ptr_const_array : type_string_const_array;
    } else {
        // Mutable: only a local that nobody else references can be trusted.
        // The single allowed reference is the format argument itself (direct case)
        // or the initializer of the pointer p (pointer case).
        if (is_static || refs != 1) {
            return false;
        }
        si.type = via_ptr ? type_string_ptr_mutable_array : type_string_mutable_array;
    }
    si.content = string_cst_dup(init);
    return true;
}

static bool resolve_string_arg(tree arg, str_info &si, int depth = 0) {
    if (depth > 4) {
        return false;
    }
    arg = strip_nops(arg);
    if (!arg) {
        return false;
    }

    switch (TREE_CODE(arg)) {
    case STRING_CST:
        return resolve_object(arg, depth > 0, si);

    case ADDR_EXPR: /* &"abc" / &arr / &arr[0] */
        return resolve_object(addr_to_object(arg), depth > 0, si);

    case VAR_DECL:
        if (TREE_CODE(TREE_TYPE(arg)) == ARRAY_TYPE) { /* array not yet decayed */
            return resolve_object(arg, depth > 0, si);
        }
        if (POINTER_TYPE_P(TREE_TYPE(arg))) { /* pointer variable p */
            if (TREE_STATIC(arg) || DECL_EXTERNAL(arg)) {
                // global/static pointer: only a const pointer can not change
                if (!TREE_READONLY(arg) || !DECL_INITIAL(arg)) {
                    return false;
                }
                return resolve_string_arg(DECL_INITIAL(arg), si, depth + 1);
            }
            decl_scan s;
            scan_decl(arg, s);
            tree init = DECL_INITIAL(arg) ? DECL_INITIAL(arg) : s.init;
            if (!init) {
                return false;
            }
            // char *const p is fixed; otherwise p must only be used by this call
            if (!TREE_READONLY(arg) && s.refs != 1) {
                return false;
            }
            return resolve_string_arg(init, si, depth + 1);
        }
        return false;

    default:
        return false;
    }
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
        STAILQ_INSERT_TAIL(&macro_rec_head, ptr, list);
        macro_rec_cnt++;

        for (int i = 0; i < 16; ++i) {
            ptr->arg_str[i] = "Null";
        }
        ptr->macro_name = func_name;
        // print(ptr->fmt);
        const std::size_t argc = call_expr_nargs(node);
        ptr->argc = argc;
        for (int i = 0; i < argc; ++i) {
            print("arg" << i);
            tree arg = CALL_EXPR_ARG(node, i);
            int type = get_type(TREE_TYPE(arg));
            // a char array that has not been decayed yet reports type_char
            bool maybe_str = (type_pointer_char == type) ||
                             (type_char == type && TREE_CODE(TREE_TYPE(arg)) == ARRAY_TYPE);
            if (maybe_str) {
                str_info si;
                if (resolve_string_arg(arg, si)) {
                    ptr->arg_str[i] = si.content;
                    type = si.type;
                    print(si.content);
                } else if (type_char == type) {
                    type = type_pointer_char;
                }
            }
            ptr->argts[i] = type;
            ptr->argss[i] = type_size[type];
            traverse_tree(arg);
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
                while (TREE_CODE(_statement) == CLEANUP_POINT_EXPR ||
                       TREE_CODE(_statement) == EXPR_STMT) {
                    _statement = TREE_OPERAND(_statement, 0);
                }
                if (TREE_CODE(_statement) == INIT_EXPR) {
                    // C++ front end: initializer emitted as a separate statement
                    tree _lhs = TREE_OPERAND(_statement, 0);
                    tree _rhs = strip_nops(TREE_OPERAND(_statement, 1));
                    if (VAR_P(_lhs) && DECL_NAME(_lhs) && _rhs && TREE_CODE(_rhs) == STRING_CST) {
                        const char *_n = IDENTIFIER_POINTER(DECL_NAME(_lhs));
                        if (!strcmp("file_path", _n)) {
                            file_path = TREE_STRING_POINTER(_rhs);
                        } else if (!strcmp("proj_path", _n)) {
                            proj_path = TREE_STRING_POINTER(_rhs);
                        }
                    }
                    continue;
                }
                if (TREE_CODE(_statement) != DECL_EXPR) {
                    continue;
                }
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
    if (TREE_CODE(node) == FUNCTION_DECL) {
        cur_fndecl = node;
    }
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
    if (j < 0) {
        j = 0; // source file directly inside proj_path
    }

    char rela_path[128];
    memcpy(rela_path, &file_path[i], j);
    rela_path[j] = '\0';

    char out_fd_dir[1024];
    snprintf(out_fd_dir, sizeof(out_fd_dir), "%s/fastlog_out/%s", proj_path, rela_path);
    printf("%s\n", out_fd_dir);
    _mkdir(out_fd_dir);

    char macro_fd_path[1024];
    snprintf(macro_fd_path, sizeof(macro_fd_path), "%s/fastlog_out/%s.macro.h", proj_path, &file_path[i]);
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
    // const char *str = "#include \"c.inc.h\"\r\n";
    // write(idx_fd, str, strlen(str));

    snprintf(out_fd_dir, sizeof(out_fd_dir), "%s/fastlog_out/sys", proj_path);
    printf("%s\n", out_fd_dir);
    _mkdir(out_fd_dir);

    char inc_fd_path[1024];
    snprintf(inc_fd_path, sizeof(inc_fd_path), "%s/fastlog_out/sys/log_inc.c", proj_path);
    fd = open(inc_fd_path, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    inc_fd = fd;
    char buf[1024 + 64];
    snprintf(buf, sizeof(buf), "#include \"%s\"\n", src_fd_path);
    write(inc_fd, buf, strlen(buf));

    char macros_fd_path[1024];
    snprintf(macros_fd_path, sizeof(macros_fd_path), "%s/fastlog_out/sys/log_macro.h", proj_path);
    fd = open(macros_fd_path, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    macros_fd = fd;
    snprintf(buf, sizeof(buf), "#include \"%s\"\n", macro_fd_path);
    write(macros_fd, buf, strlen(buf));

    struct macro_rec *np;
    STAILQ_FOREACH(np, &macro_rec_head, list) {
        make_macro(np);
        make_idx(np);
    }
    snprintf(buf, sizeof(buf), "// FASTLOG_MODULE_SIZE_ %d\n", macro_rec_cnt);
    write(idx_fd, buf, strlen(buf));

    /* Close all descriptors we opened */
    if (macro_fd >= 0) {
        close(macro_fd);
        macro_fd = -1;
    }
    if (idx_fd >= 0) {
        close(idx_fd);
        idx_fd = -1;
    }
    if (inc_fd >= 0) {
        close(inc_fd);
        inc_fd = -1;
    }
    if (macros_fd >= 0) {
        close(macros_fd);
        macros_fd = -1;
    }

    /* Free allocated records */
    while (!STAILQ_EMPTY(&macro_rec_head)) {
        np = STAILQ_FIRST(&macro_rec_head);
        STAILQ_REMOVE_HEAD(&macro_rec_head, list);
        delete np;
    }
    macro_rec_cnt = 0;
}

int plugin_init(struct plugin_name_args *plugin_info, struct plugin_gcc_version *version) {
    STAILQ_INIT(&macro_rec_head);
    register_callback(plugin_info->base_name, PLUGIN_PRE_GENERICIZE, pre_genericize_callback, NULL);
    register_callback(plugin_info->base_name, PLUGIN_FINISH, finish_callback, NULL);
    return 0;
}