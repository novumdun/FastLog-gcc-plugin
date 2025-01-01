#include <stdio.h>
#include <errno.h>

int main(int argc, char **argv) {
    if (2 != argc) {
        return EINVAL;
    }
    char *p = argv[1];
    char buf[2048];
    int cnt = 0;
    while (*p) {
        char ch = *p;
        switch (ch) {
        case '/':
            buf[cnt++] = '_';
            break;
        case '-':
            buf[cnt++] = '_';
            buf[cnt++] = '_';
            break;
        case '.':
            buf[cnt++] = '_';
            buf[cnt++] = '_';
            buf[cnt++] = '_';
            break;
        default:
            buf[cnt++] = ch;
            break;
        }
        p++;
    }
    buf[cnt] = '\0';
    printf("%s\n", buf);

    return 0;
}