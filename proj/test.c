#include <stdio.h>
#include "fastlog.h"
#include "fastlog_test.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "log_macro.h"
#endif

char *mmmm(void) {
    fastlog("%d %s", 123, "ssssssssssssss");
    return "dddddddddddddd";
}

int main(int argc, char **argv) {
    fastlog_open("/home/dengbo/workspace/cabin/FastLog-gcc-plugin/fastlog_out/flog");

    FASTLOG_MODULE_INIT(test)();

    char *p = mmmm();
    fastlog("%d %s", 100, p);

    char list[10];
    memcpy(list, "dddd", 1);
    fastlog("%d %s %d", 33, list, 123);


    fastlog("%d %d", 99, 100 + 1234);
    fastlog("%d %s", 100, "ssssssssssssss");

    test();
    return 0;
}