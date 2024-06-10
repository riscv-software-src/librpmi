/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct rpmi_shmem_simple {
	rpmi_uint64_t base;
	struct rpmi_shmem shmem;
};

static enum rpmi_error rpmi_shmem_simple_read(struct rpmi_shmem *shmem,
					      rpmi_uint32_t offset,
					      void *in, rpmi_uint32_t len)
{
	struct rpmi_shmem_simple *sshmem = shmem->priv;
	const void *src = (void *)(unsigned long)(sshmem->base + offset);

	rpmi_env_memcpy(in, src, len);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_shmem_simple_write(struct rpmi_shmem *shmem,
					       rpmi_uint32_t offset,
					       const void *out, rpmi_uint32_t len)
{
	struct rpmi_shmem_simple *sshmem = shmem->priv;
	void *dest = (void *)(unsigned long)(sshmem->base + offset);

	rpmi_env_memcpy(dest, out, len);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_shmem_simple_fill(struct rpmi_shmem *shmem,
					      rpmi_uint32_t offset,
					      char ch, rpmi_uint32_t len)
{
	struct rpmi_shmem_simple *sshmem = shmem->priv;
	void *dest = (void *)(unsigned long)(sshmem->base + offset);

	rpmi_env_memset(dest, ch, len);
	return RPMI_SUCCESS;
}

struct rpmi_shmem *rpmi_shmem_simple_create(const char *name,
					    rpmi_uint64_t base,
					    rpmi_uint32_t size)
{
	struct rpmi_shmem_simple *sshmem;
	struct rpmi_shmem *shmem;

	/* All parameters should be non-zero */
	if (!name || !size)
		return NULL;

	/* Allocate shared memory */
	sshmem = rpmi_env_zalloc(sizeof(*sshmem));
	if (!sshmem)
		return NULL;

	sshmem->base = base;

	shmem = &sshmem->shmem;
	shmem->name = name;
	shmem->size = size;
	shmem->read = rpmi_shmem_simple_read;
	shmem->write = rpmi_shmem_simple_write;
	shmem->fill = rpmi_shmem_simple_fill;
	shmem->priv = sshmem;

	return shmem;
}

void rpmi_shmem_simple_destroy(struct rpmi_shmem *shmem)
{
	if (!shmem)
		return;

	rpmi_env_free(shmem->priv);
}
