#ifndef __FASTLOG_CONFIG__
#define __FASTLOG_CONFIG__

#include "fastlog.h"

#define FASTLOG_MODULE_BASE_IDX FASTLOG_MODULE_BASE_GET(test)()

extern int FASTLOG_MODULE_BASE_GET(test)(void);
extern void FASTLOG_MODULE_INIT(test)(void);

#endif
