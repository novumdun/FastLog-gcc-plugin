#ifndef __PRE_FASTLOG_H__
#define __PRE_FASTLOG_H__

#include "shared_mutex.h"
#include "config.h"

typedef struct fastlog {
    int idx_cnt;
    int idx_base;
    int idx_size;

    char log_file_path[64];
    char *log_file_name;
    int log_fd;
    shared_mutex_t log_mutex;

    shared_mutex_t idx_mutex;
} fastlog_t;

#endif /* __FASTLOG_H__ */
