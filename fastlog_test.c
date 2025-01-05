/*
 * Copyright (c) 2024, dengbo19920415@hotmail.com
 */
#include "fastlog_test.h"

FASTLOG_MODULE_DECLEARE(test);

int FASTLOG_MODULE_BASE_GET(test)(void) {
    return FASTLOG_MODULE_BASE(test);
}

void FASTLOG_MODULE_INIT(test)(void) {
    if (FASTLOG_MODULE_BASE(test) > 0) {
        return;
    }
    int size = FASTLOG_MODULE_SIZE(test);
    FASTLOG_MODULE_BASE(test) = fastlog_get_mode_base(size);
    return;
}