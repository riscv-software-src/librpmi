/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Qualcomm Inc.
 */

#include <librpmi.h>

#ifdef DEBUG
#define DPRINTF(msg...)	rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct rpmi_message *rpmi_alloc_message(rpmi_size_t msg_size)
{
	if (!msg_size) {
		DPRINTF("%s: zero message size\n", __func__);
		return NULL;
	}

	return rpmi_env_zalloc(msg_size);
}

struct rpmi_message *rpmi_alloc_and_populate_message(rpmi_uint16_t servicegroup_id,
						     rpmi_uint8_t service_id,
						     rpmi_uint8_t flags,
						     rpmi_uint16_t token,
						     const void *data,
						     rpmi_uint16_t datalen)
{
	struct rpmi_message *msg;

	msg = rpmi_alloc_message(RPMI_MSG_HDR_SIZE + datalen);
	if (!msg) {
		DPRINTF("%s: failed to allocate message\n", __func__);
		return NULL;
	}

	/* Initialize message header */
	msg->header.servicegroup_id = servicegroup_id;
	msg->header.service_id = service_id;
	msg->header.flags = flags;
	msg->header.datalen = datalen;
	msg->header.token = token;

	/* Copy message data */
	if (data && datalen)
		rpmi_env_memcpy(msg->data, data, datalen);

	return msg;
}

struct rpmi_message *rpmi_clone_message(const struct rpmi_message *src)
{
	if (!src) {
		DPRINTF("%s: source message is NULL\n", __func__);
		return NULL;
	}

	return rpmi_alloc_and_populate_message(src->header.servicegroup_id,
					       src->header.service_id,
					       src->header.flags,
					       src->header.token,
					       src->data,
					       src->header.datalen);
}

void rpmi_free_message(struct rpmi_message *msg)
{
	if (msg)
		rpmi_env_free(msg);
}
