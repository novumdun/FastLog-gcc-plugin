#ifndef __FASTLOG_H__
#define __FASTLOG_H__

#define _MACRO_CAT(a, b) a##b
#define MACRO_CAT(a, b) _MACRO_CAT(a, b)

#define _MACRO_CAT_3(a, b, c) a##b##c
#define MACRO_CAT_3(a, b, c) _MACRO_CAT_3(a, b, c)

#define _MACRO_CAT_4(a, b, c, d) a##b##c##d
#define MACRO_CAT_4(a, b, c, d) _MACRO_CAT_4(a, b, c, d)

#define COMPILE_PASS_GEN_MACRO 0

#if COMPILE_PASS == COMPILE_PASS_GEN_MACRO
#define fastlog(fmt, ...)                                                                          \
    {                                                                                              \
        extern int MACRO_CAT_4(FASTLOG_MACRO_, __FILE_ID__, _, __LINE__)(char *format, ...);       \
        MACRO_CAT_4(FASTLOG_MACRO_, __FILE_ID__, _, __LINE__)(fmt, __VA_ARGS__);                   \
    }

#else
extern int fastlog_fd;
extern int fastlog_open(char *str);
extern int fastlog_get_mode_base(int mode_size);
extern int fastlog_write(int fd, char *buff, int len);

#define FASTLOG_MODULE_BASE(name) MACRO_CAT(FASTLOG_MODULE_BASE_IDX_, name)
#define FASTLOG_MODULE_DECLEARE(name) int FASTLOG_MODULE_BASE(name) = -1
#define FASTLOG_MODULE_INIT(name) MACRO_CAT(FASTLOG_MODULE_BASE_IDX_init_, name)
#define FASTLOG_MODULE_BASE_GET(name) MACRO_CAT(FASTLOG_MODULE_BASE_IDX_get_, name)
#define FASTLOG_MODULE_SIZE(name) MACRO_CAT(FASTLOG_MODULE_SIZE_, name)

#define fastlog(fmt, ...)                                                                          \
    { MACRO_CAT_4(FASTLOG_MACRO_, __FILE_ID__, _, __LINE__)(fmt, __VA_ARGS__); }
#endif

#endif
