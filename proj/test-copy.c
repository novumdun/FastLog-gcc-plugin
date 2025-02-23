#include <stdio.h>
#include "fastlog.h"
#include "fastlog_test.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "log_macro.h"
#endif


int test(void) {
    fastlog("%d %d", 99, 100 + 1234);
    return 0;
}