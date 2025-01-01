#include <stdio.h>


int main(int argc, char **argv) {
    float v1 = 1;
    float v2 = v1;
    extern int func();
    int v3 = func();
    extern int funcdddd(int arg1, ...);
    int v4 = funcdddd(99, 100 + 1234);
    int v6 = funcdddd(99, v4 + 1234);
    float v5 = 1;
    funcdddd(99, 100 + 1234);
    return 0;
}