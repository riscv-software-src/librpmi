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

static inline rpmi_bool_t __rpmi_transport_is_empty(struct rpmi_transport *trans,
						    enum rpmi_queue_type qtype)
{
	return trans->is_empty ? trans->is_empty(trans, qtype) : true;
}

rpmi_bool_t rpmi_transport_is_empty(struct rpmi_transport *trans,
				    enum rpmi_queue_type qtype)
{
	rpmi_bool_t ret;

	if (!trans) {
		DPRINTF("%s: NULL transport pointer\n", __func__);
		return false;
	}

	if (qtype >= RPMI_QUEUE_MAX) {
		DPRINTF("%s: %s: invalid qtype %d\n", __func__, trans->name, qtype);
		return false;
	}

	rpmi_env_lock(trans->lock);
	ret = __rpmi_transport_is_empty(trans, qtype);
	rpmi_env_unlock(trans->lock);

	return ret;
}

static inline rpmi_bool_t __rpmi_transport_is_full(struct rpmi_transport *trans,
						  enum rpmi_queue_type qtype)
{
	return trans->is_full ? trans->is_full(trans, qtype) : false;
}

rpmi_bool_t rpmi_transport_is_full(struct rpmi_transport *trans,
				  enum rpmi_queue_type qtype)
{
	rpmi_bool_t ret;

	if (!trans) {
		DPRINTF("%s: NULL transport pointer\n", __func__);
		return true;
	}

	if (qtype >= RPMI_QUEUE_MAX) {
		DPRINTF("%s: %s: invalid qtype %d\n", __func__, trans->name, qtype);
		return true;
	}

	rpmi_env_lock(trans->lock);
	ret = __rpmi_transport_is_full(trans, qtype);
	rpmi_env_unlock(trans->lock);

	return ret;
}

enum rpmi_error rpmi_transport_enqueue(struct rpmi_transport *trans,
				      enum rpmi_queue_type qtype,
				      struct rpmi_message *msg)
{
	struct rpmi_message_header *mhdr;
	enum rpmi_error rc;

	if (!trans || !msg) {
		DPRINTF("%s: NULL transport or message pointer\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (qtype >= RPMI_QUEUE_MAX) {
		DPRINTF("%s: %s: invalid qtype %d\n", __func__, trans->name, qtype);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (!trans->is_p2a_channel && qtype >= RPMI_QUEUE_P2A_REQ) {
		DPRINTF("%s: %s: p2a channel not available, invalid qtype %d\n",
			__func__, trans->name, qtype);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (!trans->enqueue) {
		DPRINTF("%s: %s: enqueue operation not supported for qtype %d\n",
			__func__, trans->name, qtype);
		return RPMI_ERR_NOTSUPP;
	}

	/* Convert header fields to match transport endianness */
	mhdr = &msg->header;
	mhdr->servicegroup_id = rpmi_to_xe32(trans->is_be, mhdr->servicegroup_id);
	mhdr->datalen = rpmi_to_xe32(trans->is_be, mhdr->datalen);
	mhdr->token = rpmi_to_xe16(trans->is_be, mhdr->token);

	/* Enqueue the message */
	rpmi_env_lock(trans->lock);
	if (__rpmi_transport_is_full(trans, qtype)) {
		DPRINTF("%s: %s: qtype %d is full\n", __func__, trans->name, qtype);
		rpmi_env_unlock(trans->lock);
		return RPMI_ERR_IO;
	}
	rc = trans->enqueue(trans, qtype, msg);
	rpmi_env_unlock(trans->lock);

	/* Reverse the endian conversion of header fields */
	mhdr->servicegroup_id = rpmi_to_xe32(trans->is_be, mhdr->servicegroup_id);
	mhdr->datalen = rpmi_to_xe16(trans->is_be, mhdr->datalen);
	mhdr->token = rpmi_to_xe16(trans->is_be, mhdr->token);

	return rc;
}

enum rpmi_error rpmi_transport_dequeue(struct rpmi_transport *trans,
				       enum rpmi_queue_type qtype,
				       struct rpmi_message *out_msg)
{
	struct rpmi_message_header *mhdr;
	enum rpmi_error rc;

	if (!trans || !out_msg) {
		DPRINTF("%s: NULL transport or message pointer\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (qtype >= RPMI_QUEUE_MAX) {
		DPRINTF("%s: %s: invalid qtype %d\n", __func__, trans->name, qtype);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (!trans->is_p2a_channel && qtype >= RPMI_QUEUE_P2A_REQ) {
		DPRINTF("%s: %s: p2a channel not available, invalid qtype %d\n",
			__func__, trans->name, qtype);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (!trans->dequeue) {
		DPRINTF("%s: %s: dequeue operation not supported for qtype %d\n",
			__func__, trans->name, qtype);
		return RPMI_ERR_NOTSUPP;
	}

	rpmi_env_lock(trans->lock);

	/* Dequeue the message */
	if (__rpmi_transport_is_empty(trans, qtype)) {
		DPRINTF("%s: %s: qtype %d is empty\n", __func__, trans->name, qtype);
		rpmi_env_unlock(trans->lock);
		return RPMI_ERR_IO;
	}
	rc = trans->dequeue(trans, qtype, out_msg);
	rpmi_env_unlock(trans->lock);

	/* Convert header fields to native endianness */
	if (!rc) {
		mhdr = &out_msg->header;
		mhdr->servicegroup_id = rpmi_to_xe32(trans->is_be, mhdr->servicegroup_id);
		mhdr->datalen = rpmi_to_xe16(trans->is_be, mhdr->datalen);
		mhdr->token = rpmi_to_xe16(trans->is_be, mhdr->token);
	}

	return rc;
}
