/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>

#ifdef DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/** RPMI shared memory instance */
struct rpmi_shmem {
	/** Name of the shared memory */
	const char *name;

	/** Base address of the shared memory */
	rpmi_uint64_t base;

	/** Size of the shared memory */
	rpmi_uint32_t size;

	/** Pointer to platform specific operations */
	const struct rpmi_shmem_platform_ops *ops;

	/** Private data of the platform specific oprations */
	void *ops_priv;
};

static enum rpmi_error shmem_env_memcpy_read(void *priv, rpmi_uint64_t addr,
					     void *in, rpmi_uint32_t len)
{
	rpmi_env_memcpy(in, (const void *)(unsigned long)addr, len);
	return RPMI_SUCCESS;
}

static enum rpmi_error shmem_env_memcpy_write(void *priv, rpmi_uint64_t addr,
					      const void *out, rpmi_uint32_t len)
{
	rpmi_env_memcpy((void *)(unsigned long)addr, out, len);
	return RPMI_SUCCESS;
}

static enum rpmi_error shmem_env_memset_fill(void *priv, rpmi_uint64_t addr,
					     char ch, rpmi_uint32_t len)
{
	rpmi_env_memset((void *)(unsigned long)addr, ch, len);
	return RPMI_SUCCESS;
}

struct rpmi_shmem_platform_ops rpmi_shmem_simple_ops = {
	.read = shmem_env_memcpy_read,
	.write = shmem_env_memcpy_write,
	.fill = shmem_env_memset_fill,
};

static enum rpmi_error shmem_env_memcpy_invalidate_read(void *priv, rpmi_uint64_t addr,
						        void *in, rpmi_uint32_t len)
{
	rpmi_env_cache_clean((void *)(unsigned long)addr, len);
	rpmi_env_memcpy(in, (const void *)(unsigned long)addr, len);
	return RPMI_SUCCESS;
}

static enum rpmi_error shmem_env_memcpy_write_clean(void *priv, rpmi_uint64_t addr,
						    const void *out, rpmi_uint32_t len)
{
	rpmi_env_memcpy((void *)(unsigned long)addr, out, len);
	rpmi_env_cache_clean((void *)(unsigned long)addr, len);
	return RPMI_SUCCESS;
}

static enum rpmi_error shmem_env_memset_fill_clean(void *priv, rpmi_uint64_t addr,
						   char ch, rpmi_uint32_t len)
{
	rpmi_env_memset((void *)(unsigned long)addr, ch, len);
	rpmi_env_cache_clean((void *)(unsigned long)addr, len);
	return RPMI_SUCCESS;
}

struct rpmi_shmem_platform_ops rpmi_shmem_simple_noncoherent_ops = {
	.read = shmem_env_memcpy_invalidate_read,
	.write = shmem_env_memcpy_write_clean,
	.fill = shmem_env_memset_fill_clean,
};

rpmi_uint64_t rpmi_shmem_base(struct rpmi_shmem *shmem)
{
	return (shmem) ? shmem->base : 0;
}

rpmi_uint32_t rpmi_shmem_size(struct rpmi_shmem *shmem)
{
	return (shmem) ? shmem->size : 0;
}

enum rpmi_error rpmi_shmem_read(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				void *in, rpmi_uint32_t len)
{
	if (!shmem || !in) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}
	if ((offset + len) > shmem->size) {
		DPRINTF("%s: %s: invalid offset 0x%x or len 0x%x\n",
			__func__, shmem->name, offset, len);
		return RPMI_ERR_BAD_RANGE;
	}
	return shmem->ops->read(shmem->ops_priv, shmem->base + offset, in, len);
}

enum rpmi_error rpmi_shmem_write(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				 const void *out, rpmi_uint32_t len)
{
	if (!shmem || !out) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}
	if ((offset + len) > shmem->size) {
		DPRINTF("%s: %s: invalid offset 0x%x or len 0x%x\n",
			__func__, shmem->name, offset, len);
		return RPMI_ERR_BAD_RANGE;
	}
	return shmem->ops->write(shmem->ops_priv, shmem->base + offset, out, len);
}

enum rpmi_error rpmi_shmem_fill(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				char ch, rpmi_uint32_t len)
{
	if (!shmem) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}
	if ((offset + len) > shmem->size) {
		DPRINTF("%s: %s: invalid offset 0x%x or len 0x%x\n",
			__func__, shmem->name, offset, len);
		return RPMI_ERR_BAD_RANGE;
	}
	return shmem->ops->fill(shmem->ops_priv, shmem->base + offset, ch, len);
}

struct rpmi_shmem *rpmi_shmem_create(const char *name,
				     rpmi_uint64_t base,
				     rpmi_uint32_t size,
				     const struct rpmi_shmem_platform_ops *ops,
				     void *ops_priv)
{
	struct rpmi_shmem *shmem;

	/* All parameters should be non-zero */
	if (!name || !size || !ops || !ops->read || !ops->write || !ops->fill) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate shared memory */
	shmem = rpmi_env_zalloc(sizeof(*shmem));
	if (!shmem) {
		DPRINTF("%s: failed to allocate shared memory instance\n", __func__);
		return NULL;
	}

	shmem->name = name;
	shmem->base = base;
	shmem->size = size;
	shmem->ops = ops;
	shmem->ops_priv = ops_priv;

	return shmem;
}

void rpmi_shmem_destroy(struct rpmi_shmem *shmem)
{
	if (!shmem) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_free(shmem);
}
