/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Qualcomm Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "librpmi_internal_list.h"

#ifdef DEBUG
#define DPRINTF(msg...)	rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct rpmi_transport_ll_message {
	struct rpmi_dlist head;
	struct rpmi_message *msg;
	rpmi_size_t msg_size;
};

struct rpmi_transport_ll {
	struct rpmi_transport trans;
	struct rpmi_dlist queues[RPMI_QUEUE_MAX];
};

static rpmi_bool_t ll_is_empty(struct rpmi_transport *trans,
			       enum rpmi_queue_type qtype)
{
	struct rpmi_transport_ll *lltrans = trans->priv;

	return rpmi_list_empty(&lltrans->queues[qtype]);
}

static rpmi_bool_t ll_is_full(struct rpmi_transport *trans,
			      enum rpmi_queue_type qtype)
{
	/* Theoretically a linked-list can never be full */
	return false;
}

static enum rpmi_error ll_enqueue(struct rpmi_transport *trans,
				  enum rpmi_queue_type qtype,
				  const struct rpmi_message *msg,
				  rpmi_size_t msg_size)
{
	struct rpmi_transport_ll *lltrans = trans->priv;
	struct rpmi_transport_ll_message *llmsg;

	/* Allocate shared memory transport */
	llmsg = rpmi_env_zalloc(sizeof(*llmsg));
	if (!llmsg) {
		DPRINTF("%s: failed to allocate linked-list message", __func__);
		return RPMI_ERR_IO;
	}

	/* Create linked-list message */
	RPMI_INIT_LIST_HEAD(&llmsg->head);
	llmsg->msg = rpmi_clone_message(msg);
	if (!llmsg->msg) {
		DPRINTF("%s: failed to clone RPMI message", __func__);
		rpmi_env_free(llmsg);
		return RPMI_ERR_IO;
	}
	llmsg->msg_size = msg_size;

	/* Add linked-list message to the queue */
	rpmi_list_add_tail(&llmsg->head, &lltrans->queues[qtype]);

	return RPMI_SUCCESS;
}

static enum rpmi_error ll_dequeue(struct rpmi_transport *trans,
				  enum rpmi_queue_type qtype,
				  struct rpmi_message *out_msg)
{
	struct rpmi_transport_ll *lltrans = trans->priv;
	struct rpmi_transport_ll_message *llmsg;

	/* Copy the first linked-list message to output message */
	llmsg = rpmi_list_first_entry(&lltrans->queues[qtype],
				      struct rpmi_transport_ll_message, head);
	rpmi_env_memcpy(out_msg, llmsg->msg, llmsg->msg_size);

	/* Remove the first linked-list message */
	rpmi_free_message(llmsg->msg);
	rpmi_list_del(&llmsg->head);
	rpmi_env_free(llmsg);

	return RPMI_SUCCESS;
}

struct rpmi_transport *rpmi_transport_ll_create(const char *name,
						rpmi_uint32_t slot_size)
{
	struct rpmi_transport_ll *lltrans;
	struct rpmi_transport *trans;
	unsigned int i;

	/* All parameters should be non-zero */
	if (!name || !slot_size)
		return NULL;

	/* Slot size should be power of 2 and at least RPMI_SLOT_SIZE_MIN */
	if ((slot_size & (slot_size - 1)) || slot_size < RPMI_SLOT_SIZE_MIN)
		return NULL;

	/* Allocate shared memory transport */
	lltrans = rpmi_env_zalloc(sizeof(*lltrans));
	if (!lltrans)
		return NULL;

	/* Setup empty queues */
	for (i = 0; i < RPMI_QUEUE_MAX; i++)
		RPMI_INIT_LIST_HEAD(&lltrans->queues[i]);

	trans = &lltrans->trans;
	trans->name = name;
	trans->is_be = false;
	trans->slot_size = slot_size;
	trans->is_p2a_channel = true;
	trans->is_empty = ll_is_empty;
	trans->is_full = ll_is_full;
	trans->enqueue = ll_enqueue;
	trans->dequeue = ll_dequeue;
	trans->lock = rpmi_env_alloc_lock();
	trans->priv = lltrans;

	return trans;
}

void rpmi_transport_ll_destroy(struct rpmi_transport *trans)
{
	enum rpmi_queue_type qtype;
	struct rpmi_message *msg;

	if (!trans)
		return;

	/* Drain all queues before destroying */
	msg = rpmi_alloc_message(trans->slot_size);
	for (qtype = 0; qtype < RPMI_QUEUE_MAX; qtype++) {
		while (!rpmi_transport_is_empty(trans, qtype))
			rpmi_transport_dequeue(trans, qtype, msg);
	}
	rpmi_free_message(msg);

	rpmi_env_free_lock(trans->lock);
	rpmi_env_free(trans->priv);
}
