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

#ifndef container_of
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })
#endif

#ifndef array_size
#define array_size(x)	(sizeof(x) / sizeof((x)[0]))
#endif

#define RPMI_BIT(nr)		(1UL << (nr))
#define RPMI_GENMASK(h, l)	((~0UL << (l)) & \
				 (~0UL >> ((sizeof(unsigned long) * 8) - 1 - (h))))

/** CPPC fast-channel flags */
#define RPMI_CPPC_FST_CHN_MODE_NORMAL		0U
#define RPMI_CPPC_FST_CHN_MODE_AUTONOMOUS	RPMI_BIT(3)
#define RPMI_CPPC_FST_CHN_DB_REG_WIDTH_MASK	RPMI_GENMASK(2, 1)
#define RPMI_CPPC_FST_CHN_DB_REG_08_BITS	0U
#define RPMI_CPPC_FST_CHN_DB_REG_16_BITS	RPMI_BIT(1)
#define RPMI_CPPC_FST_CHN_DB_REG_32_BITS	RPMI_BIT(2)
#define RPMI_CPPC_FST_CHN_DB_NOT_SUPP		0U
#define RPMI_CPPC_FST_CHN_DB_SUPP		RPMI_BIT(0)

#define RPMI_MAX(a, b)			\
({					\
	__typeof__(a) _a = (a);		\
	__typeof__(b) _b = (b);		\
	(_a > _b) ? _a : _b;		\
})

#define RPMI_MIN(a, b)			\
({					\
	__typeof__(a) _a = (a);		\
	__typeof__(b) _b = (b);		\
	(_a < _b) ? _a : _b;		\
})

#define RPMI_CLAMP(a, lo, hi) RPMI_MIN(RPMI_MAX(a, lo), hi)

#define RPMI_STR(x) RPMI_XSTR(x)

#define RPMI_XSTR(x) #x

/**
 * Roundup the x to the next multiple of m
 */
#define RPMI_ROUNDUP(x, m)		\
({					\
	__typeof__(m) _m = m;		\
	((((x - 1) / _m) + 1) * _m)	\
})

/**
 * Rounddown the x to the previous multiple of m
 */
#define RPMI_ROUNDDOWN(x, m)		\
({					\
	__typeof__(m) _m = m;		\
	((x / _m) * _m);		\
})

#endif /* __LIBRPMI_INTERNAL_H__ */
