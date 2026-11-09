#include <stdio.h>
#include <string.h>

#include "fastlog.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "log_macro.h"
#endif

#define __LINE_STR(x) "Line" #x
#define _LINE_STR(x)  __LINE_STR(x)
#define LINE_STR      _LINE_STR(__LINE__)

extern int FASTLOG_MODULE_SIZE(test);
extern int test01(fastlog_t *log);

const char *test_func_1(void) {
    return __func__;
}

int main(int argc, char **argv) {
    fastlog_t log;

    int size = FASTLOG_MODULE_SIZE(test);
    fastlog_open(&log, "fastlog_out/flog", size);

    fastlog(&log, __func__);

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

    test01(&log);
    return 0;
}