 /*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems
 */

#ifndef __LIBRPMI_INTERNAL_H__
#define __LIBRPMI_INTERNAL_H__

#include <librpmi_env.h>

#undef __packed
#define __packed		__attribute__((packed))

#undef __noreturn
#define __noreturn		__attribute__((noreturn))

#undef __aligned
#define __aligned(x)		__attribute__((aligned(x)))

#undef __always_inline
#define __always_inline	inline __attribute__((always_inline))

#ifndef __has_builtin
#define __has_builtin(...) 0
#endif

#undef offsetof
#if __has_builtin(__builtin_offsetof)
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE,MEMBER)
#elif defined(__compiler_offsetof)
#define offsetof(TYPE, MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((rpmi_size_t) &((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })

#define array_size(x) 	(sizeof(x) / sizeof((x)[0]))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(a, lo, hi) MIN(MAX(a, lo), hi)

#define STR(x) XSTR(x)
#define XSTR(x) #x

#define ROUNDUP(a, b) ((((a)-1) / (b) + 1) * (b))
#define ROUNDDOWN(a, b) ((a) / (b) * (b))

#endif


