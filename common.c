/* SPDX-License-Identifier: MIT
 * Copyright(c) 2022 Darek Stojaczyk
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>

#include "common.h"

int g_log_level = 99; /* everything but debug by default */

void
slog(int type, const char *filename, unsigned lineno, const char *fnname, const char *fmt, ...)
{
	va_list args;
	const char *type_str;

	if (type > g_log_level) {
		return;
	}

	switch (type) {
		case LOG_ERROR:
			type_str = "ERROR";
			break;
		case LOG_INFO:
			type_str = "INFO";
			break;
		case LOG_DEBUG_1:
			type_str = "DEBUG1";
			break;
		case LOG_DEBUG_2:
			type_str = "DEBUG2";
			break;
		case LOG_DEBUG_3:
			type_str = "DEBUG3";
			break;
			break;
		default:
			return;
	}

	fprintf(stderr, "%s:%u %s(): %s: ", filename, lineno, fnname, type_str);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	putc('\n', stderr);
	fflush(stderr);
}