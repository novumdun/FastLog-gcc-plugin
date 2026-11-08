/*
 * Copyright (c) 2014-2026, dengbo19920415@hotmail.com
 *
 * Runtime support for FastLog.
 * Index ranges are allocated from a shared JSON file protected by flock(),
 * using the cJSON library for parsing/serialization.
 */
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <signal.h>

#include "cJSON.h"
#include "fastlog.h"

static pthread_mutex_t fd_lock = PTHREAD_MUTEX_INITIALIZER;
static int fd_lock_initialized = 0;

void fastlog_init_idx_base(fastlog_t *log) {
    if (log->idx_base >= 0) {
        return;
    }
    log->idx_base = fastlog_get_idx_base(log->idx_cnt);
}

int fastlog_open(fastlog_t *log, const char *str, int size) {
    log->idx_cnt = size;
    fastlog_init_idx_base(log);

    if (!str)
        return -1;
    int fd = open(str, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        fprintf(stderr, "%s:%d open(\"%s\") failed: %s\n", __FILE__, __LINE__, str, strerror(errno));
        return -1;
    }
    log->log_fd = fd;

    if (!fd_lock_initialized) {
        pthread_mutex_init(&fd_lock, NULL);
        fd_lock_initialized = 1;
    }
    return fd;
}

int fastlog_write(fastlog_t *log, struct autogen_log_stru *buff, int len) {
    if (log->log_fd < 0 || !buff || len <= 0)
        return -1;

    int written = 0;
    const char *p = (const char *)buff;

    while (written < len) {
        pthread_mutex_lock(&fd_lock);
        ssize_t n = write(log->log_fd, p + written, (size_t)(len - written));
        pthread_mutex_unlock(&fd_lock);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "%s:%d write failed: %s\n", __FILE__, __LINE__, strerror(errno));
            return -1;
        }
        if (n == 0)
            return -1;
        written += (int)n;
    }
    return 0;
}

void fastlog_close(fastlog_t *log) {
    if (log->log_fd >= 0) {
        close(log->log_fd);
        log->log_fd = -1;
    }
}
