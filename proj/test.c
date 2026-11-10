#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "fastlog.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "log_macro.h"
#endif

#define __LINE_STR(x) "Line" #x
#define _LINE_STR(x)  __LINE_STR(x)
#define LINE_STR      _LINE_STR(__LINE__)

#define _STR(x)       #x
#define STR(x)        _STR(x)

extern int FASTLOG_MODULE_SIZE(__FASTLOG_MOD_NAME__); // You need to add this to every module using 'fastlog'
const char project_name[] = STR(__FASTLOG_MOD_NAME__);
extern int file_a_test(fastlog_t *log);

const char *test_func_1(void) {
    return __func__;
}

int main(int argc, char **argv) {
    fastlog_t log;
    memset(&log, 0, sizeof(log));

    if (argc != 2) {
        return -1;
    }

    char *log_path = argv[1];
    int size = FASTLOG_MODULE_SIZE(__FASTLOG_MOD_NAME__);
    fastlog_open(&log, project_name, log_path, size);

    fastlog(&log, __func__);
    fastlog(&log, __FUNCTION__ );
    fastlog(&log, __FILE__);

    fastlog(&log, "Hello world");

    char *fmt = "%d %s";
    fastlog(&log, fmt, 123, LINE_STR);

    char c = 0xaa;
    fastlog(&log, "%d %d", 123, c);
    fastlog(&log, "%d %f", 123, 3.1415926);

    const char *p = LINE_STR;
    fastlog(&log, "%d %s", 12345678, p);

    char str_arr[32];
    char *str = "Hello world, %d %s";
    memcpy(str_arr, str, strlen(str));
    fastlog(&log, str_arr, __LINE__, LINE_STR);

    p = test_func_1();
    fastlog(&log, p);
    fastlog(&log, "%d %s", __LINE__, p);

    fastlog(&log, "%d %x", __LINE__, __LINE__ + 0xFFFF0000);

    char mut[] = "hello";
    mut[0] = 'o';
    fastlog(&log, mut);

    file_a_test(&log);
    return 0;
}