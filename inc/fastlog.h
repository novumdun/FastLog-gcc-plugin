#ifndef __FASTLOG_H__
#define __FASTLOG_H__

#define _MACRO_CAT(a, b)         a##b
#define MACRO_CAT(a, b)          _MACRO_CAT(a, b)

#define _MACRO_CAT_3(a, b, c)    a##b##c
#define MACRO_CAT_3(a, b, c)     _MACRO_CAT_3(a, b, c)

#define _MACRO_CAT_4(a, b, c, d) a##b##c##d
#define MACRO_CAT_4(a, b, c, d)  _MACRO_CAT_4(a, b, c, d)

#define _MACRO_CAT_5(a, b, c, d, e) a##b##c##d##e
#define MACRO_CAT_5(a, b, c, d, e)  _MACRO_CAT_5(a, b, c, d, e)

#define COMPILE_PASS_GEN_MACRO      0

typedef struct idx_range {
    unsigned int start;
    unsigned int end;
} idx_range_t;

typedef struct fastlog {
    int idx_cnt;
    int idx_base;
    int log_fd;
} fastlog_t;

struct autogen_log_stru {
    unsigned short len;
    unsigned short idx;
} __attribute__((__packed__));

#define FASTLOG_MODULE_SIZE(name) MACRO_CAT(FASTLOG_MODULE_SIZE_, name)

static inline int fastlog_get_idx_base(fastlog_t *log) {
    return log->idx_base;
}

extern int fastlog_open(fastlog_t *log, const char *str, int size);
extern int fastlog_write(fastlog_t *log, struct autogen_log_stru *buff, int len);
extern void fastlog_close(fastlog_t *log);

#if COMPILE_PASS == COMPILE_PASS_GEN_MACRO

#define fastlog(log, fmt, ...)                                                                                         \
    {                                                                                                                  \
        extern int MACRO_CAT_4(FASTLOG_MACRO_, __FILE_ID__, _, __LINE__)(char *format, ...);                      \
        MACRO_CAT_4(FASTLOG_MACRO_, __FILE_ID__, _, __LINE__)(fmt __VA_OPT__(, ) __VA_ARGS__);                         \
    }

static void fastlog_tmp_func(void) {
    char file_path[] = __FILE_PATH__;
    char proj_path[] = __PROJ_PATH__;
    return;
}

#else

/*
 * Shared index allocation (JSON file + flock).
 *
 * fastlog_get_idx_base(size) – allocates a contiguous range of `size` indices,
 *   records it under (process_name, pid) in the JSON file, prunes dead PIDs
 *   that share the same process name, and returns the start index.
 */
extern int fastlog_get_idx_base_from_config(int mode_size, char *index_file_name);

#define fastlog(log, fmt, ...)                                                                                         \
    {                                                                                                                  \
        MACRO_CAT_5(FASTLOG_MACRO_, __FILE_ID__, _, __LINE__, _log_idx)(log, fmt __VA_OPT__(, ) __VA_ARGS__);          \
    }
#endif

#endif /* __FASTLOG_H__ */
