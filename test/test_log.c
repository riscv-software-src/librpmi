/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include "test_log.h"

int __printf(1, 2) rpmi_env_printf(const char *format, ...)
{
	int res;
	va_list args;
	va_start(args, format);
	res = vprintf(format, args);
	va_end(args);
	return res;
}
