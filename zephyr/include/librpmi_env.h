/*
 * Copyright (c) 2025 Tenstorrent AI ULC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LIBRPMI_ENV_H__
#define __LIBRPMI_ENV_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/sem.h>
#include <zephyr/sys/byteorder.h>

#ifdef __cplusplus
extern "C" {
#endif

#define rpmi_int8_t int8_t
#define rpmi_uint8_t uint8_t

#define rpmi_int16_t int16_t
#define rpmi_uint16_t uint16_t

#define rpmi_int32_t int32_t
#define rpmi_uint32_t uint32_t

#define rpmi_int64_t int64_t
#define rpmi_uint64_t uint64_t

#define rpmi_bool_t bool

#define rpmi_uintptr_t uintptr_t
#define rpmi_size_t size_t
#define rpmi_ssize_t int

#define rpmi_env_memcmp(s1, s2, n) memcmp((s1), (s2), (n))
#define rpmi_env_memcpy(dest, src, n) memcpy((dest), (src), (n))
#define rpmi_env_memset(dest, c, n) memset((dest), (c), (n))

/* note: strnlen() is posix. no need to cross the streams here */
#define rpmi_env_strnlen(str, count) strlen(str)
#define rpmi_env_strncpy(dest, src, count) strncpy((dest), (src), (count))

/* todo: hook into Zephyr's caching subsystem */
#define rpmi_env_cache_invalidate(base, len)
#define rpmi_env_cache_clean(base, len)

#define rpmi_env_zalloc(size) calloc(1, (size))
#define rpmi_env_free(ptr) free(ptr)

static inline void *rpmi_env_alloc_lock(void)
{
	void *ret = malloc(sizeof(struct sys_sem));
	if (ret != NULL) {
		sys_sem_init((struct sys_sem *)ret, 1, 1);
	}

	return ret;
}

static inline void rpmi_env_free_lock(void *lptr)
{
	/* TODO: assert semaphore is not locked */
	free(lptr);
}

static inline void rpmi_env_lock(void *lptr)
{
	sys_sem_take((struct sys_sem *)lptr, K_FOREVER);
}

static inline void rpmi_env_unlock(void *lptr)
{
	sys_sem_give((struct sys_sem *)lptr);
}

#define rpmi_env_div32(dividend, divisor) ((dividend) / (divisor))
#define rpmi_env_mod32(dividend, divisor) ((dividend) % (divisor))


#define BSWAP16(x) BSWAP_16(x)
#define BSWAP32(x) BSWAP_32(x)

#define rpmi_to_le16(val) sys_cpu_to_le16(val)
#define rpmi_to_le32(val) sys_cpu_to_le32(val)

#define rpmi_to_be16(val) sys_cpu_to_be16(val)
#define rpmi_to_be32(val) sys_cpu_to_be32(val)

#define rpmi_to_xe16(is_be, val) ((is_be) ? rpmi_to_be16(val) : rpmi_to_le16(val))
#define rpmi_to_xe32(is_be, val) ((is_be) ? rpmi_to_be32(val) : rpmi_to_le32(val))

#define rpmi_env_writel(addr, val) sys_write32((val), (addr))

#define rpmi_env_printf printk

#ifdef __cplusplus
}
#endif

#endif
