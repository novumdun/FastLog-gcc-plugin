#include <stdio.h>
#include "fastlog.h"
#include "fastlog_test.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "log_macro.h"
#endif


int test(void) {

    char list[10];
    memcpy(list, "dddd", 1);
    fastlog("%d %s", 33, list);


    fastlog("%d %d", 99, 100 + 1234);
    fastlog("%d %s", 100, "ssssssssssssss");
    return 0;
}