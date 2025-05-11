#include <stdio.h>
#include <string.h>

#include "fastlog.h"
#include "fastlog_test.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "log_macro.h"
#endif

#define __LINE_STR(x) "Line" #x
#define _LINE_STR(x) __LINE_STR(x)
#define LINE_STR _LINE_STR(__LINE__)

extern int test01(void);

const char *test_func_1(void) {
    return __func__;
}

int main(int argc, char **argv) {
    fastlog_open("/home/dengbo/workspace/cabin/FastLog-gcc-plugin/fastlog_out/flog");
    FASTLOG_MODULE_INIT(test)();

    fastlog(__func__);

    char *fmt = "%d %s";
    fastlog(fmt, 123, LINE_STR);

    char c = 0xaa;
    fastlog("%d %d", 123, c);
    fastlog("%d %f", 123, 3.1415926);

    const char *p = LINE_STR;
    fastlog("%d %s", 12345678, p);

    char str_arr[32];
    char *str = "Hello world, %d %s";
    memcpy(str_arr, str, strlen(str));
    fastlog(str_arr, __LINE__, LINE_STR);

    p = test_func_1();
    fastlog(p);
    fastlog("%d %s", __LINE__, p);

    fastlog("%d %x", __LINE__, __LINE__ + 0xFFFF0000);

    test01();
    return 0;
}