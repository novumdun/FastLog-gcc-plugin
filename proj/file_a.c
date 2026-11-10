#include <stdio.h>
#include "fastlog.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "log_macro.h"
#endif


int file_a_test(fastlog_t *log) {
    fastlog(log, "%d %d", 99, 100 + 1234);
    return 0;
}