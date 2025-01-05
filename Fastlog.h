#ifndef __FASTLOG_H__
#define __FASTLOG_H__

#define _MACRO_CAT(a, b) a##b
#define MACRO_CAT(a, b) _MACRO_CAT(a, b)

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

#define fastlog(fmt, ...)                                                                          \
    { MACRO_CAT_4(FASTLOG_MACRO_, __FILE_ID__, _, __LINE__)(fmt, __VA_ARGS__); }
#endif

#endif
