/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#ifndef SYNERGY_SERIAL_COMMON
#define SYNERGY_SERIAL_COMMON

enum {
    LOG_ERROR  = 0,
    LOG_INFO = 1,
    LOG_DEBUG_1 = 101,
    LOG_DEBUG_2 = 102,
    LOG_DEBUG_3 = 103,
};

extern int g_log_level;

void slog(int type, const char *filename, unsigned lineno, const char *fnname, const char *fmt, ...);
#define LOG(type, ...) slog((type), __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif /* SYNERGY_SERIAL_COMMON */