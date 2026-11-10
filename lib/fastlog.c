/*
 * Copyright (c) 2014-2026, dengbo19920415@hotmail.com
 *
 * Runtime support for FastLog.
 * Index ranges are allocated from a shared JSON file protected by flock(),
 * using the cJSON library for parsing/serialization.
 */

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
#include "shared_mutex.h"
#include "fastlog.h"

int fastlog_init_idx_base(fastlog_t *log) {
    if (log->idx_base > 0) {
        return 0;
    }

    int ret = fastlog_get_idx_base_from_config(log);
    return ret;
}

char *get_base_name(char *path) {
    char *p = path;
    while (*path != '\0') {
        if ((*path == '/') && (*(path + 1) != '\0')) {
            p = path + 1;
        }
        path++;
    }
    return p;
}

int fastlog_open(fastlog_t *log, const char *log_file_path, int size) {
    if (log == NULL) {
        return -1;
    }

    if (log_file_path == NULL) {
        return -1;
    }

    if (size <= 0) {
        return -1;
    }

    log->idx_cnt = size;
    strncpy(log->log_file_path, log_file_path, sizeof(log->log_file_path));
    log->log_file_name = get_base_name(log->log_file_path);

    int ret = fastlog_init_idx_base(log);
    if (ret != 0) {
        return -1;
    }

    int fd = open(log_file_path, O_RDWR | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        fprintf(stderr, "%s:%d open(\"%s\") failed: %s\n", __FILE__, __LINE__, log_file_path, strerror(errno));
        return -1;
    }
    log->log_fd = fd;

    char log_file_mutex_name[64];
    snprintf(log_file_mutex_name, sizeof(log_file_mutex_name) - 1, "/%s_mutex", log->log_file_name);
    ret = shared_mutex_init(&log->log_mutex, log_file_mutex_name);
    if (ret != 0) {
        return -1;
    }

    return fd;
}

int fastlog_write(fastlog_t *log, struct autogen_log_stru *buff, int len) {
    if (log->log_fd < 0 || !buff || len <= 0)
        return -1;

    int written = 0;
    const char *p = (const char *)buff;

    while (written < len) {
        shared_mutex_lock(&log->log_mutex);
        ssize_t n = write(log->log_fd, p + written, (size_t)(len - written));
        shared_mutex_unlock(&log->log_mutex);

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
    return;
}
