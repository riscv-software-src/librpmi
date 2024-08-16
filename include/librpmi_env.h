/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#ifndef __LIBRPMI_ENV_H__
#define __LIBRPMI_ENV_H__

/******************************************************************************/

/**
 * \defgroup BASIC_TYPES_DEFINES Basic Data Types & Defines
 * @brief Basic data types and defines.
 * @{
 */
typedef char			rpmi_int8_t;
typedef unsigned char		rpmi_uint8_t;

typedef short			rpmi_int16_t;
typedef unsigned short		rpmi_uint16_t;

typedef int			rpmi_int32_t;
typedef unsigned int		rpmi_uint32_t;

#if __SIZEOF_LONG__ == 8
typedef long			rpmi_int64_t;
typedef unsigned long		rpmi_uint64_t;
#elif __SIZEOF_LONG__ == 4
typedef long long		rpmi_int64_t;
typedef unsigned long long	rpmi_uint64_t;
#else
#error "Unexpected __SIZEOF_LONG__"
#endif

typedef int			rpmi_bool_t;
typedef unsigned long		rpmi_uintptr_t;
typedef unsigned long		rpmi_size_t;
typedef long			rpmi_ssize_t;

#ifndef true
#define true			1
#endif
#ifndef false
#define false			0
#endif
#ifndef NULL
#define NULL			((void *)0)
#endif

/** @} */

/******************************************************************************/

/**
 * \defgroup STRING_ENV String Environment Functions
 * @brief String functions used by library which must be provided by the
 * platform firmware.
 * @{
 */

/**
 * @brief compare memory from one place to another
 *
 * @param[in] s1	pointer to src1
 * @param[in] s2	pointer to src2
 * @param[in] n		number of bytes to compare
 * @return void *	Pointer to destination
 */
static inline int rpmi_env_memcmp(void *s1, void *s2, rpmi_size_t n)
{
	char *temp1 = s1;
	char *temp2 = s2;

	while (n > 0) {
		if (*temp1 < *temp2)
			return -1;
		if (*temp1 > *temp2)
			return 1;
		temp1++;
		temp2++;
		n--;
	}

	return 0;
}

/**
 * @brief Copy memory from one place to another
 *
 * @param[in] dest 	pointer to destination
 * @param[in] src 	pointer to destination
 * @param[in] n 	number of bytes to copy
 * @return void *	Pointer to destination
 */
static inline void *rpmi_env_memcpy(void *dest, const void *src, rpmi_size_t n)
{
	char *temp1 = dest;
	const char *temp2 = src;

	while (n > 0) {
		*temp1++ = *temp2++;
		n--;
	}

	return dest;
}

/**
 * @brief Write or fill a memory range with a character
 *
 * @param[in] dest 	pointer to destination
 * @param[in] c 	character to write of fill
 * @param[in] n 	number of bytes to write
 * @return void *	Pointer to destination
 */
static inline void *rpmi_env_memset(void *dest, int c, rpmi_size_t n)
{
	char *temp = dest;

	while (n > 0) {
		n--;
		*temp++ = c;
	}

	return dest;
}

/**
 * @brief Get length of a fixed size string
 *
 * @param[in] dest	pointer to string buffer
 * @param[in] count	number of max chars to count
 * @return rpmi_size_t  number of chars in string limited by count
 */
static inline rpmi_size_t rpmi_env_strnlen(const char *str, rpmi_size_t count)
{
	unsigned long ret = 0;

	while (*str != '\0' && ret < count) {
		ret++;
		str++;
	}

	return ret;
}

/**
 * @brief Copy string limited by count
 *
 * @param[in] dest	pointer to string destination buffer
 * @param[in] src	pointer to string source buffer
 * @param[in] count	number of max chars to count
 * @return char *	pointer to string destination buffer
 */
static inline char *rpmi_env_strncpy(char *dest, const char *src, rpmi_size_t count)
{
	char *ret = dest;

	while (count && *src != '\0') {
		*dest++ = *src++;
		count -= 1;
	}

	rpmi_env_memset(dest, '\0', count);
	return ret;
}

/** @} */

/******************************************************************************/

/**
 * \defgroup CACHE_ENV Cache Maintenance Functions
 * @brief Cache maintenance functions used by library which must be provided by the
 * platform firmware.
 * @{
 */

/**
 * @brief Invalidate cache lines based on virtual address range
 *
 * @param[in] base	pointer to base virtual address
 * @param[in] len	number of bytes to invalidate
 */
static inline void rpmi_env_cache_invalidate(void *base, rpmi_size_t len)
{
}

/**
 * @brief Clean cache lines based on virtual address range
 *
 * @param[in] base	pointer to base virtual address
 * @param[in] len	number of bytes to clean
 */
static inline void rpmi_env_cache_clean(void *base, rpmi_size_t len)
{
}

/** @} */

/******************************************************************************/

/**
 * \defgroup HEAP_ENV Heap Environment Functions
 * @brief Memory allocation functions used by library which must be provided by the
 * platform firmware.
 * @{
 */

/**
 * @brief Dynamically allocate zero initialized memory of specified size
 *
 * @param[in] size 	Size(bytes) of the memory block to be allocated
 * @return void *	Pointer to the allocated memory block
 */
void *rpmi_env_zalloc(rpmi_size_t size);

/**
 * @brief Dynamically free the memory pointer to by ptr
 *
 * @param[in] ptr 	Pointer to memory
 */
void rpmi_env_free(void *ptr);

/** @} */

/******************************************************************************/

/**
 * \defgroup LOCKING_ENV Locking Environment Functions
 * @brief Locking functions used by library which must be provided by the platform firmware.
 * @{
 */

/**
 * @brief Dynamically allocate a lock for synchronization
 *
 * Note: If locking is not supported then this function will always return NULL.
 *
 * @param[in] size 	Size(bytes) of the memory block to be allocated
 * @return void *	Pointer to the allocated lock
 */
static inline void *rpmi_env_alloc_lock(void)
{
	return NULL;
}

/**
 * @brief Dynamically free a lock
 *
 * Note: This function does nothing if lock pointer is NULL.
 *
 * @param[in] lptr 	Pointer to lock
 */
static inline void rpmi_env_free_lock(void *lptr)
{
}

/**
 * @brief Acquire a lock
 *
 * Note: This function does nothing if lock pointer is NULL.
 *
 * @param[in] lptr 	Pointer to lock
 */
static inline void rpmi_env_lock(void *lptr)
{
	/* Do the actual locking if available */
}

/**
 * @brief Release a lock
 *
 * Note: This function does nothing if lock pointer is NULL.
 *
 * @param[in] lptr 	Pointer to lock
 */
static inline void rpmi_env_unlock(void *lptr)
{
	/* Do the actual unlocking if available */
}

/** @} */

/******************************************************************************/

/**
 * \defgroup MATH_ENV Integer Math Environment Functions
 * @brief Basic math functions for 32/64 bit integers to be implemented by the
 * platform firmware.
 * @{
 */

/**
 * @brief 32-bit Division Operation
 *
 * @param[in] dividend
 * @param[in] divisor
 * @return rpmi_uint32_t
 */
static inline rpmi_uint32_t rpmi_env_div32(rpmi_uint32_t dividend,
					   rpmi_uint32_t divisor)
{
	return dividend / divisor;
}

/**
 * @brief 32-bit Modulo Operation
 *
 * @param[in] dividend
 * @param[in] divisor
 * @return rpmi_uint32_t
 */
static inline rpmi_uint32_t rpmi_env_mod32(rpmi_uint32_t dividend,
					   rpmi_uint32_t divisor)
{
	return dividend % divisor;
}

/** @} */

/******************************************************************************/

/**
 * \defgroup ENDIAN_ENV Endian Conversion Environment Functions
 * @brief Basic math functions for 32/64 bit integers to be implemented by the
 * platform firmware.
 * @{
 */

#define BSWAP16(x)	((((x) & 0x00ff) << 8) | \
			 (((x) & 0xff00) >> 8))
#define BSWAP32(x)	((((x) & 0x000000ff) << 24) | \
			 (((x) & 0x0000ff00) << 8) | \
			 (((x) & 0x00ff0000) >> 8) | \
			 (((x) & 0xff000000) >> 24))

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

static inline rpmi_uint16_t rpmi_to_le16(rpmi_uint16_t val)
{
	return val;
}

static inline rpmi_uint32_t rpmi_to_le32(rpmi_uint32_t val)
{
	return val;
}

static inline rpmi_uint16_t rpmi_to_be16(rpmi_uint16_t val)
{
	return BSWAP16(val);
}

static inline rpmi_uint32_t rpmi_to_be32(rpmi_uint32_t val)
{
	return BSWAP32(val);
}

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

static inline rpmi_uint16_t rpmi_to_le16(rpmi_uint16_t val)
{
	return BSWAP16(val);
}

static inline rpmi_uint32_t rpmi_to_le32(rpmi_uint32_t val)
{
	return BSWAP32(val);
}

static inline rpmi_uint16_t rpmi_to_be16(rpmi_uint16_t val)
{
	return val;
}

static inline rpmi_uint32_t rpmi_to_be32(rpmi_uint32_t val)
{
	return val;
}

#else
#error "Unexpected __BYTE_ORDER__"
#endif

/**
 * @brief Convert endianness of 16-bit integer based on parameter
 *
 * @param[in] is_be	Target endianness (true: Big-endian, false: Little-endian)
 * @param[in] val	16-bit integer value
 * @return rpmi_uint16_t Endian converted value
 */
static inline rpmi_uint16_t rpmi_to_xe16(rpmi_bool_t is_be, rpmi_uint16_t val)
{
	return is_be ? rpmi_to_be16(val) : rpmi_to_le16(val);
}

/**
 * @brief Convert endianness of 32-bit integer based on parameter
 *
 * @param[in] is_be	Target endianness (true: Big-endian, false: Little-endian)
 * @param[in] val	32-bit integer value
 * @return rpmi_uint32_t Endian converted value
 */
static inline rpmi_uint32_t rpmi_to_xe32(rpmi_bool_t is_be, rpmi_uint32_t val)
{
	return is_be ? rpmi_to_be32(val) : rpmi_to_le32(val);
}

/** @} */

/******************************************************************************/

/**
 * \defgroup MMIO_ENV MMIO Read/Write Environment Functions
 * @brief System-wide memory mapped input/output (MMIO) read/write functions
 * to be implemented by the platform firmware.
 * @{
 */

/**
 * @brief Write 4-bytes to address in little-endian byte-order
 *
 * @param[in] addr	64-bit address
 * @param[in] val	4-byte data to write
 */
void rpmi_env_writel(rpmi_uint64_t addr, rpmi_uint32_t val);

/** @} */

/******************************************************************************/

/**
 * \defgroup CONSOLE_ENV Console Environment Functions
 * @brief Console functions for logging purpose to be implemented by the platform firmware.
 * @{
 */

#define __printf(a, b) __attribute__((format(printf, a, b)))

/**
 * @brief Print a formatted string on console
 *
 * @param[in] format	Formatted stdio string
 * @return 0 upon success and negative error (< 0) upon failure
 */
int __printf(1, 2) rpmi_env_printf(const char *format, ...);

/** @} */

#endif  /* __LIBRPMI_ENV_H__ */
