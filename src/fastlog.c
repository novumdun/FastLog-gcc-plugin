/*
 * Copyright (c) 2014, dengbo19920415@hotmail.com
 */
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <pthread.h>

#include "fastlog.h"

static atomic_int fastlog_cnt = 0;
int fastlog_fd = -1;
static pthread_mutex_t fd_lock = PTHREAD_MUTEX_INITIALIZER;

int fastlog_open(char *str) {
    int fd = open(str, O_RDWR | O_CREAT, 0666);
    if (0 > fd) {
        printf("%s.%d open file fail. Err %d.", __FILE__, __LINE__, errno);
    }
    fastlog_fd = fd;
    pthread_mutex_init(&fd_lock, NULL);
    return fd;
}

int fastlog_get_mode_base(int mode_size) {
    int base = atomic_fetch_add(&fastlog_cnt, mode_size);
    return base;
}

int fastlog_write(int fd, char *buff, int len) {
    int count = 0;
    int size = len;

    if (fd < 0) {
        return -1;
    }
    while (count < size) {
        pthread_mutex_lock(&fd_lock);
        count = write(fd, buff + count, size - count);
        pthread_mutex_unlock(&fd_lock);
        size = size - count;
        if (count < 0) {
            return -1;
        }
    }
    return 0;
}
