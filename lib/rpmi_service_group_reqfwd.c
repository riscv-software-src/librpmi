// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 SiFive, Inc.
 */

#include <librpmi.h>

#ifdef LIBRPMI_DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/** Request Forward service group private data */
struct reqfwd_service_group {
	const struct rpmi_reqfwd_platform_ops	*ops;
	void					*ops_priv;
	rpmi_bool_t				message_retrieved;
	rpmi_bool_t				notif_enabled;
	rpmi_bool_t				pending_new_message;
	rpmi_uint32_t				pending_event_data_len;
	struct rpmi_transport			*trans;
	struct rpmi_message			*notif_msg;

	struct rpmi_service_group		group;
};

static enum rpmi_error rpmi_reqfwd_enable_notification(struct rpmi_service_group *group,
						       struct rpmi_service *service,
						       struct rpmi_transport *trans,
						       rpmi_uint16_t request_datalen,
						       const rpmi_uint8_t *request_data,
						       rpmi_uint16_t *response_datalen,
						       rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (rpmi_uint32_t *)response_data;
	const rpmi_uint32_t *req = (const rpmi_uint32_t *)request_data;
	struct reqfwd_service_group *rgrp = group->priv;
	rpmi_uint32_t event_id, req_state;

	/* Notifications require a P2A channel to deliver events */
	if (!trans->is_p2a_channel) {
		resp[0] = rpmi_to_xe32(trans->is_be, RPMI_ERR_NOTSUPP);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	event_id = rpmi_to_xe32(trans->is_be, req[0]);
	req_state = rpmi_to_xe32(trans->is_be, req[1]);

	DPRINTF("%s: event_id=%d req_state=%d\n", __func__, event_id, req_state);

	/* Validate event ID */
	if (event_id != RPMI_REQFWD_EVENT_NEW_MESSAGE) {
		resp[0] = rpmi_to_xe32(trans->is_be, RPMI_ERR_INVALID_PARAM);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Validate and process request state */
	switch (req_state) {
	case 0: /* Disable */
		rgrp->notif_enabled = false;
		rgrp->pending_new_message = false;
		break;
	case 1: /* Enable */
		rgrp->notif_enabled = true;
		break;
	case 2: /* Query current state */
		break;
	default:
		resp[0] = rpmi_to_xe32(trans->is_be, RPMI_ERR_INVALID_PARAM);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Return current state */
	resp[0] = rpmi_to_xe32(trans->is_be, RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, rgrp->notif_enabled ? 1U : 0U);
	*response_datalen = 2 * sizeof(rpmi_uint32_t);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_reqfwd_retrieve_current_message(struct rpmi_service_group *group,
							    struct rpmi_service *service,
							    struct rpmi_transport *trans,
							    rpmi_uint16_t request_datalen,
							    const rpmi_uint8_t *request_data,
							    rpmi_uint16_t *response_datalen,
							    rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (rpmi_uint32_t *)response_data;
	const rpmi_uint32_t *req = (const rpmi_uint32_t *)request_data;
	struct reqfwd_service_group *rgrp = group->priv;
	struct rpmi_reqfwd_message msg;
	rpmi_uint32_t start_index, max_data, remaining, returned;
	enum rpmi_error ret;

	start_index = rpmi_to_xe32(trans->is_be, req[0]);

	DPRINTF("%s: start_index=%d\n", __func__, start_index);

	/* Get the current forwarded message from platform */
	ret = rgrp->ops->get_current_message(rgrp->ops_priv, &msg);
	if (ret != RPMI_SUCCESS) {
		resp[0] = rpmi_to_xe32(trans->is_be, ret);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Check if we have a message */
	if (msg.msg_size == 0 || !msg.msg_data) {
		resp[0] = rpmi_to_xe32(trans->is_be, RPMI_ERR_NO_DATA);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Validate start index */
	if (start_index >= msg.msg_size) {
		resp[0] = rpmi_to_xe32(trans->is_be, RPMI_ERR_INVALID_PARAM);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Calculate how much data we can return */
	max_data = RPMI_MSG_DATA_SIZE(trans->slot_size) - (3 * sizeof(rpmi_uint32_t));
	remaining = msg.msg_size - start_index;
	returned = (remaining > max_data) ? max_data : remaining;
	remaining -= returned;

	/* Fill response */
	resp[0] = rpmi_to_xe32(trans->is_be, RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, remaining);
	resp[2] = rpmi_to_xe32(trans->is_be, returned);
	rpmi_env_memcpy(&resp[3], msg.msg_data + start_index, returned);

	*response_datalen = (3 * sizeof(rpmi_uint32_t)) + returned;

	/* Mark that message has been retrieved */
	rgrp->message_retrieved = 1;

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_reqfwd_complete_current_message(struct rpmi_service_group *group,
							    struct rpmi_service *service,
							    struct rpmi_transport *trans,
							    rpmi_uint16_t request_datalen,
							    const rpmi_uint8_t *request_data,
							    rpmi_uint16_t *response_datalen,
							    rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (rpmi_uint32_t *)response_data;
	struct reqfwd_service_group *rgrp = group->priv;
	rpmi_uint32_t num_messages = 0;
	enum rpmi_error ret;

	DPRINTF("%s: request_datalen=%d\n", __func__, request_datalen);

	/* Check if message was retrieved */
	if (!rgrp->message_retrieved) {
		resp[0] = rpmi_to_xe32(trans->is_be, RPMI_ERR_NO_DATA);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Complete the current message with response data */
	ret = rgrp->ops->complete_current_message(rgrp->ops_priv,
						  request_datalen,
						  request_data,
						  &num_messages);
	if (ret != RPMI_SUCCESS) {
		resp[0] = rpmi_to_xe32(trans->is_be, ret);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Reset retrieval flag for next message */
	rgrp->message_retrieved = 0;

	/* Fill response */
	resp[0] = rpmi_to_xe32(trans->is_be, RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, num_messages);
	*response_datalen = 2 * sizeof(rpmi_uint32_t);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_reqfwd_process_events(struct rpmi_service_group *group)
{
	struct reqfwd_service_group *rgrp = group->priv;
	struct rpmi_transport *trans = rgrp->trans;
	struct rpmi_message *notif_msg = rgrp->notif_msg;
	enum rpmi_error rc;

	if (!rgrp->notif_enabled || !rgrp->pending_new_message)
		return RPMI_SUCCESS;

	notif_msg->header.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD;
	notif_msg->header.service_id = 0;
	notif_msg->header.flags = RPMI_MSG_NOTIFICATION;
	notif_msg->header.token = 0;
	notif_msg->header.datalen = sizeof(rpmi_uint32_t) + rgrp->pending_event_data_len;

	rc = rpmi_transport_enqueue(trans, RPMI_QUEUE_P2A_REQ, notif_msg);
	if (rc != RPMI_SUCCESS) {
		if (rc == RPMI_ERR_IO)
			return RPMI_ERR_BUSY;
		DPRINTF("%s: failed to enqueue notification (error %d)\n",
			__func__, rc);
		return rc;
	}
	rgrp->pending_new_message = false;

	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_reqfwd_services[RPMI_REQFWD_SRV_ID_MAX] = {
	[RPMI_REQFWD_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 2 * sizeof(rpmi_uint32_t),
		.process_a2p_request = rpmi_reqfwd_enable_notification,
	},
	[RPMI_REQFWD_SRV_RETRIEVE_CURRENT_MESSAGE] = {
		.service_id = RPMI_REQFWD_SRV_RETRIEVE_CURRENT_MESSAGE,
		.min_a2p_request_datalen = sizeof(rpmi_uint32_t),
		.process_a2p_request = rpmi_reqfwd_retrieve_current_message,
	},
	[RPMI_REQFWD_SRV_COMPLETE_CURRENT_MESSAGE] = {
		.service_id = RPMI_REQFWD_SRV_COMPLETE_CURRENT_MESSAGE,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_reqfwd_complete_current_message,
	},
};

struct rpmi_service_group *
rpmi_service_group_reqfwd_create(const struct rpmi_reqfwd_platform_ops *ops,
				 void *ops_priv,
				 struct rpmi_transport *trans)
{
	struct reqfwd_service_group *rgrp;
	struct rpmi_service_group *group;

	if (!ops || !ops->get_current_message || !ops->complete_current_message ||
	    !trans) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate request forward group */
	rgrp = rpmi_env_zalloc(sizeof(*rgrp));
	if (!rgrp) {
		DPRINTF("%s: failed to allocate request forward service group instance\n",
			__func__);
		return NULL;
	}

	rgrp->notif_msg = rpmi_env_zalloc(trans->slot_size);
	if (!rgrp->notif_msg) {
		DPRINTF("%s: failed to allocate notification message buffer\n",
			__func__);
		rpmi_env_free(rgrp);
		return NULL;
	}

	rgrp->ops = ops;
	rgrp->ops_priv = ops_priv;
	rgrp->message_retrieved = 0;
	rgrp->notif_enabled = false;
	rgrp->pending_new_message = false;
	rgrp->trans = trans;

	group = &rgrp->group;
	group->name = "reqfwd";
	group->servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK |
					RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_REQFWD_SRV_ID_MAX;
	group->services = rpmi_reqfwd_services;
	group->process_events = rpmi_reqfwd_process_events;
	group->lock = rpmi_env_alloc_lock();
	group->priv = rgrp;

	DPRINTF("%s: group=%p\n", __func__, group);

	return group;
}

void rpmi_service_group_reqfwd_destroy(struct rpmi_service_group *group)
{
	struct reqfwd_service_group *rgrp;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rgrp = group->priv;
	if (rgrp->notif_msg)
		rpmi_env_free(rgrp->notif_msg);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(rgrp);

	DPRINTF("%s: group destroyed\n", __func__);
}

enum rpmi_error rpmi_reqfwd_trigger_new_message(struct rpmi_service_group *group,
						rpmi_uint32_t event_data_len,
						const rpmi_uint8_t *event_data)
{
	struct reqfwd_service_group *rgrp;

	if (!group || group->servicegroup_id != RPMI_SRVGRP_REQUEST_FORWARD)
		return RPMI_ERR_INVALID_PARAM;

	rgrp = group->priv;

	if (!rgrp->trans->is_p2a_channel)
		return RPMI_ERR_NOTSUPP;

	if (event_data_len && !event_data)
		return RPMI_ERR_INVALID_PARAM;

	/* Clamp to what fits after the event header in one notification slot */
	rpmi_uint32_t max_event_data = RPMI_MSG_DATA_SIZE(rgrp->trans->slot_size) -
				       sizeof(rpmi_uint32_t);
	if (event_data_len > max_event_data)
		event_data_len = max_event_data;

	/* Round down to 4-byte alignment as required by spec */
	event_data_len &= ~(rpmi_uint32_t)3;

	rpmi_env_lock(group->lock);

	/* Encode EVENT_HDR: EVENT_ID in bits[23:16], EVENT_DATALEN in bits[15:0] */
	((rpmi_uint32_t *)rgrp->notif_msg->data)[0] =
		rpmi_to_xe32(rgrp->trans->is_be,
			     ((rpmi_uint32_t)RPMI_REQFWD_EVENT_NEW_MESSAGE << 16) | event_data_len);
	if (event_data_len)
		rpmi_env_memcpy((rpmi_uint8_t *)rgrp->notif_msg->data +
				sizeof(rpmi_uint32_t), event_data, event_data_len);
	rgrp->pending_event_data_len = event_data_len;
	rgrp->pending_new_message = true;

	rpmi_env_unlock(group->lock);

	return RPMI_SUCCESS;
}
