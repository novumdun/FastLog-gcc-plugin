#include <stdio.h>
#include "Fastlog.h"
#include "fastlog_test.h"
#if COMPILE_PASS != COMPILE_PASS_GEN_MACRO
#include "test.macro.h"
#endif

char *mmmm(void) {
    fastlog("%d %s", 123, "ssssssssssssss");
    return "dddddddddddddd";
}

int main(int argc, char **argv) {

    char *p = mmmm();
    fastlog("%d %s", 100, p);

    char list[10];
    memcpy(list, "dddd", 1);
    fastlog("%d %s", 33, list);


    fastlog("%d %d", 99, 100 + 1234);
    fastlog("%d %s", 100, "ssssssssssssss");
    return 0;
}