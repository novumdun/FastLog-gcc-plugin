#include <stdio.h>
#include <string.h>

#include "fastlog.h"
#include "fastlog_test.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "log_macro.h"
#endif

extern int test(void);

const char *test_func_1(void) {
    return __func__;
}

int main(int argc, char **argv) {
    fastlog_open("/home/dengbo/workspace/cabin/FastLog-gcc-plugin/fastlog_out/flog");
    FASTLOG_MODULE_INIT(test)();

    char *p = "%d %s";
    fastlog(p, 123, "ssssssssssssss");

    char c = 0xaa;
    fastlog("%d %d", 123, c);
    fastlog("%d %s", 12345678, p);

    char str[12];
    memcpy(str, "hello", strlen("hello"));
    fastlog(str);

    const char *p_1 = test_func_1();
    fastlog("%d %s", 100, p_1);
    fastlog(p_1);

    fastlog("%d %d", 99, 100 + 1234);

    test();
    return 0;
}