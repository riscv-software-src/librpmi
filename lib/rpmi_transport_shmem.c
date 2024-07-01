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

struct rpmi_transport_shmem {
	struct rpmi_shmem *shmem;
	rpmi_uint32_t queue_size;
	rpmi_uint32_t data_slots;
	struct rpmi_transport trans;
};

static rpmi_bool_t shmem_is_empty(struct rpmi_transport *trans,
				  enum rpmi_queue_type qtype)
{
	struct rpmi_transport_shmem *shtrans = trans->priv;
	struct rpmi_shmem *shmem = shtrans->shmem;
	rpmi_uint32_t queue_base = shtrans->queue_size * qtype;
	rpmi_uint32_t headidx, tailidx;
	int rc;

	rc = rpmi_shmem_read(shmem, queue_base, &headidx, sizeof(headidx));
	if (rc) {
		DPRINTF("%s: %s: failed to read head index of qtype %d\n",
			__func__, trans->name, qtype);
		return false;
	}
	headidx = rpmi_to_le32(headidx);

	rc = rpmi_shmem_read(shmem, queue_base + trans->slot_size,
			     &tailidx, sizeof(tailidx));
	if (rc) {
		DPRINTF("%s: %s: failed to read tail index of qtype %d\n",
			__func__, trans->name, qtype);
		return false;
	}
	tailidx = rpmi_to_le32(tailidx);

	return (headidx == tailidx) ? true : false;
}

static rpmi_bool_t shmem_is_full(struct rpmi_transport *trans,
				 enum rpmi_queue_type qtype)
{
	struct rpmi_transport_shmem *shtrans = trans->priv;
	rpmi_uint32_t queue_base = shtrans->queue_size * qtype;
	struct rpmi_shmem *shmem = shtrans->shmem;
	rpmi_uint32_t headidx, tailidx;
	int rc;

	rc = rpmi_shmem_read(shmem, queue_base, &headidx, sizeof(headidx));
	if (rc) {
		DPRINTF("%s: %s: failed to read head index of qtype %d\n",
			__func__, trans->name, qtype);
		return false;
	}
	headidx = rpmi_to_le32(headidx);

	rc = rpmi_shmem_read(shmem, queue_base + trans->slot_size,
			     &tailidx, sizeof(tailidx));
	if (rc) {
		DPRINTF("%s: %s: failed to read tail index of qtype %d\n",
			__func__, trans->name, qtype);
		return false;
	}
	tailidx = rpmi_to_le32(tailidx);

	return (rpmi_env_mod32(tailidx + 1, shtrans->data_slots) == headidx) ?
		true : false;
}

static enum rpmi_error shmem_enqueue(struct rpmi_transport *trans,
				     enum rpmi_queue_type qtype,
				     const struct rpmi_message *msg)
{
	struct rpmi_transport_shmem *shtrans = trans->priv;
	rpmi_uint32_t queue_base = shtrans->queue_size * qtype;
	struct rpmi_shmem *shmem = shtrans->shmem;
	rpmi_uint32_t tailidx;
	int rc;

	rc = rpmi_shmem_read(shmem, queue_base + trans->slot_size,
			     &tailidx, sizeof(tailidx));
	if (rc) {
		DPRINTF("%s: %s: failed to read tail index of qtype %d\n",
			__func__, trans->name, qtype);
		return RPMI_ERR_FAILED;
	}
	tailidx = rpmi_to_le32(tailidx);

	rc = rpmi_shmem_write(shmem, queue_base + ((tailidx + 2) * trans->slot_size),
			      msg, trans->slot_size);
	if (rc) {
		DPRINTF("%s: %s: failed to write message at tailidx %d for qtype %d\n",
			__func__, trans->name, tailidx, qtype);
		return RPMI_ERR_FAILED;
	}

	tailidx = rpmi_to_le32(rpmi_env_mod32(tailidx + 1, shtrans->data_slots));
	rc = rpmi_shmem_write(shmem, queue_base + trans->slot_size,
			      &tailidx, sizeof(tailidx));
	if (rc) {
		DPRINTF("%s: %s: failed to write tail index for qtype %d\n",
			__func__, trans->name, qtype);
		return RPMI_ERR_FAILED;
	}

	return 0;
}

static enum rpmi_error shmem_dequeue(struct rpmi_transport *trans,
				     enum rpmi_queue_type qtype,
				     struct rpmi_message *out_msg)
{
	struct rpmi_transport_shmem *shtrans = trans->priv;
	rpmi_uint32_t queue_base = shtrans->queue_size * qtype;
	struct rpmi_shmem *shmem = shtrans->shmem;
	rpmi_uint32_t headidx;
	int rc;

	rc = rpmi_shmem_read(shmem, queue_base, &headidx, sizeof(headidx));
	if (rc) {
		DPRINTF("%s: %s: failed to read head index of qtype %d\n",
			__func__, trans->name, qtype);
		return false;
	}
	headidx = rpmi_to_le32(headidx);

	rc = rpmi_shmem_read(shmem, queue_base + ((headidx + 2) * trans->slot_size),
			     out_msg, trans->slot_size);
	if (rc) {
		DPRINTF("%s: %s: failed to read message at headidx %d for qtype %d\n",
			__func__, trans->name, headidx, qtype);
		return RPMI_ERR_FAILED;
	}

	headidx = rpmi_to_le32(rpmi_env_mod32(headidx + 1, shtrans->data_slots));
	rc = rpmi_shmem_write(shmem, queue_base,
			      &headidx, sizeof(headidx));
	if (rc) {
		DPRINTF("%s: %s: failed to write head index for qtype %d\n",
			__func__, trans->name, qtype);
		return RPMI_ERR_FAILED;
	}

	return 0;
}

struct rpmi_transport *rpmi_transport_shmem_create(const char *name,
						   rpmi_uint32_t slot_size,
						   struct rpmi_shmem *shmem)
{
	struct rpmi_transport_shmem *shtrans;
	struct rpmi_transport *trans;

	/* All parameters should be non-zero */
	if (!name || !slot_size || !shmem)
		return NULL;

	/* Slot size should be power of 2 and at least RPMI_SLOT_SIZE_MIN */
	if ((slot_size & (slot_size - 1)) || slot_size < RPMI_SLOT_SIZE_MIN)
		return NULL;

	/*
	 * Shared memory size should be 4KB aligned
	 *
	 * Note: Actual physical base address of shared memory is
	 * assumed to be 4KB aligned.
	 */
	if (rpmi_shmem_size(shmem) & 0xfff)
		return NULL;

	/*
	 * Shared memory size should be aligned to slot size
	 *
	 * Note: Actual physical base address of shared memory is
	 * assumed to be aligned to slot size.
	 */
	if (rpmi_shmem_size(shmem) & (slot_size - 1))
		return NULL;

	/* Make sure we can divide shared memory into RPMI_QUEUE_MAX parts */
	if (rpmi_shmem_size(shmem) < LIBRPMI_TRANSPORT_SHMEM_MIN_SIZE(slot_size))
		return NULL;

	/* Fill the shared memory with zeros */
	if (rpmi_shmem_fill(shmem, 0, 0, rpmi_shmem_size(shmem)))
		return NULL;

	/* Allocate shared memory transport */
	shtrans = rpmi_env_zalloc(sizeof(*shtrans));
	if (!shtrans)
		return NULL;

	shtrans->shmem = shmem;
	shtrans->queue_size = rpmi_shmem_size(shmem) / RPMI_QUEUE_MAX;
	shtrans->data_slots = rpmi_env_div32(shtrans->queue_size, slot_size) - 2;

	trans = &shtrans->trans;
	trans->name = name;
	trans->is_be = false;
	trans->slot_size = slot_size;
	trans->is_empty = shmem_is_empty;
	trans->is_full = shmem_is_full;
	trans->enqueue = shmem_enqueue;
	trans->dequeue = shmem_dequeue;
	trans->lock = rpmi_env_alloc_lock();
	trans->priv = shtrans;

	return trans;
}

void rpmi_transport_shmem_destroy(struct rpmi_transport *trans)
{
	if (!trans)
		return;

	rpmi_env_free_lock(trans->lock);
	rpmi_env_free(trans->priv);
}
