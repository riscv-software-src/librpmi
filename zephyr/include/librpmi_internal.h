 /*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 Tenstorrent AI ULC
 */

#ifndef __LIBRPMI_INTERNAL_H__
#define __LIBRPMI_INTERNAL_H__

#include <zephyr/toolchain.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define container_of(ptr, type, member) CONTAINER_OF((ptr), type, member)
#define array_size(x) ARRAY_SIZE(x)

#define RPMI_MAX(a, b) MAX((a), (b))
#define RPMI_MIN(a, b) MIN((a), (b))

#define RPMI_CLAMP(a, lo, hi) CLAMP((a), (lo), (hi))

#define RPMI_STR(x) STRINGIFY(x)
#define RPMI_XSTR(x) Z_STRINGIFY(x)

#define RPMI_ROUNDUP(x, m) ROUND_UP((x), (m))
#define RPMI_ROUNDDOWN(x, m) ROUND_DOWN((x), (m))

#ifdef __cplusplus
}
#endif

#endif
