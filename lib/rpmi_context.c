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

struct rpmi_base_group;

struct rpmi_context {
	/** Name of the context */
	const char *name;

	/** Underlying transport instance of the context */
	struct rpmi_transport *trans;

	/** Maximum number of service groups handled by the context */
	rpmi_uint32_t max_num_groups;

	/** Current number of service groups in the context */
	rpmi_uint32_t num_groups;

	/** RISC-V privilege level of RPMI context */
	enum rpmi_privilege_level privilege_level;

	/** Current set of service groups in the context as an array */
	struct rpmi_service_group **groups;

	/** Lock to synchronize num_groups and groups array access (optional) */
	void *groups_lock;

	/** Temporary request message */
	struct rpmi_message *req_msg;

	/** Temporary acknowledgment message */
	struct rpmi_message *ack_msg;

	/** Base serivce group */
	struct rpmi_service_group *base_group;

	/** System MSI serivce group */
	struct rpmi_service_group *sysmsi_group;
};

struct rpmi_base_group {
	struct rpmi_context *cntx;

	/* Platform info string length */
	rpmi_uint32_t plat_info_len;

	/* Platform info string */
	char *plat_info;

	struct rpmi_service_group group;
};

static enum rpmi_error rpmi_base_get_impl_version(struct rpmi_service_group *group,
						  struct rpmi_service *service,
						  struct rpmi_transport *trans,
						  rpmi_uint16_t request_datalen,
						  const rpmi_uint8_t *request_data,
						  rpmi_uint16_t *response_datalen,
						  rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (void *)response_data;

	*response_datalen = 2 * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be,
				RPMI_BASE_VERSION(LIBRPMI_IMPL_VERSION_MAJOR,
						  LIBRPMI_IMPL_VERSION_MINOR));

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_base_get_impl_idn(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *trans,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (void *)response_data;

	*response_datalen = 2 * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, LIBRPMI_IMPL_ID);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_base_get_spec_version(struct rpmi_service_group *group,
						  struct rpmi_service *service,
						  struct rpmi_transport *trans,
						  rpmi_uint16_t request_datalen,
						  const rpmi_uint8_t *request_data,
						  rpmi_uint16_t *response_datalen,
						  rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (void *)response_data;

	*response_datalen = 2 * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be,
				       RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR,
							 RPMI_SPEC_VERSION_MINOR));

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_base_get_plat_info(struct rpmi_service_group *group,
					     struct rpmi_service *service,
					     struct rpmi_transport *trans,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_base_group *base = group->priv;
	rpmi_uint8_t *plat_info;

	*response_datalen = (2 * sizeof(*resp)) + base->plat_info_len;
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, base->plat_info_len);
	plat_info = (rpmi_uint8_t *)&resp[2];
	rpmi_env_strncpy((char *)plat_info, base->plat_info, base->plat_info_len);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_base_probe_group(struct rpmi_service_group *group,
					     struct rpmi_service *service,
					     struct rpmi_transport *trans,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_base_group *base = group->priv;
	struct rpmi_service_group *srvgrp;
	rpmi_uint32_t probe_id, ver;

	probe_id = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	*response_datalen = 2 * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	
	srvgrp = rpmi_context_find_group(base->cntx, probe_id);
	ver = srvgrp ? srvgrp->servicegroup_version : 0;

	resp[1] = rpmi_to_xe32(trans->is_be, ver);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_base_get_attributes(struct rpmi_service_group *group,
						struct rpmi_service *service,
						struct rpmi_transport *trans,
						rpmi_uint16_t request_datalen,
						const rpmi_uint8_t *request_data,
						rpmi_uint16_t *response_datalen,
						rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_context *cntx = ((struct rpmi_base_group *)group->priv)->cntx;
	rpmi_uint32_t flags = 0;

	/* Set the context privilege level bit if context priv mode is M-mode */
	flags |= (cntx->privilege_level == RPMI_PRIVILEGE_M_MODE) ?
					RPMI_BASE_FLAGS_F0_PRIVILEGE : 0;

	*response_datalen = 5 * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, flags);
	resp[2] = 0;
	resp[3] = 0;
	resp[4] = 0;

	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_base_services[RPMI_BASE_SRV_ID_MAX] = {
	[RPMI_BASE_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION] = {
		.service_id = RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_base_get_impl_version,
	},
	[RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN] = {
		.service_id = RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_base_get_impl_idn,
	},
	[RPMI_BASE_SRV_GET_SPEC_VERSION] = {
		.service_id = RPMI_BASE_SRV_GET_SPEC_VERSION,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_base_get_spec_version,
	},
	[RPMI_BASE_SRV_GET_PLATFORM_INFO] = {
		.service_id = RPMI_BASE_SRV_GET_PLATFORM_INFO,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_base_get_plat_info,
	},
	[RPMI_BASE_SRV_PROBE_SERVICE_GROUP] = {
		.service_id = RPMI_BASE_SRV_PROBE_SERVICE_GROUP,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_base_probe_group,
	},
	[RPMI_BASE_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_BASE_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_base_get_attributes,
	},
};

static struct rpmi_service_group *rpmi_base_group_create(struct rpmi_context *cntx,
							 rpmi_uint32_t plat_info_len,
							 const char *plat_info)
{
	struct rpmi_service_group *group;
	struct rpmi_base_group *base;
	rpmi_uint32_t max_plat_info_len;

	base = rpmi_env_zalloc(sizeof(*base));
	if (!base)
		return NULL;

	base->cntx = cntx;

	/*
	 * Make sure that platform info length can be
	 * accomodated in the message data as per the
	 * format of the base_get_platform_info service
	 */
	max_plat_info_len = RPMI_MSG_DATA_SIZE(cntx->trans->slot_size) -
						(sizeof(rpmi_uint32_t) * 2);

	if (plat_info_len > max_plat_info_len) {
		goto fail_free_base;
	}

	if (plat_info) {
		base->plat_info_len = plat_info_len;
		base->plat_info = rpmi_env_zalloc(plat_info_len);
		if (!base->plat_info)
			goto fail_free_base;

		rpmi_env_strncpy(base->plat_info, plat_info, plat_info_len);
	}

	group = &base->group;
	group->name = "base";
	group->servicegroup_id = RPMI_SRVGRP_BASE;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed for both M-mode and S-mode RPMI context */
	group->privilege_level_bitmap =
		RPMI_PRIVILEGE_M_MODE_MASK | RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_BASE_SRV_ID_MAX;
	group->services = rpmi_base_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = base;

	return group;

fail_free_base:
	rpmi_env_free(base);
	return NULL;
}

static void rpmi_base_group_destroy(struct rpmi_service_group *group)
{
	struct rpmi_base_group *base = group->priv;

	if (base->plat_info)
		rpmi_env_free(base->plat_info);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}

static enum rpmi_error rpmi_service_notsupp_a2p_request(struct rpmi_service_group *group,
							struct rpmi_service *service,
							struct rpmi_transport *trans,
							rpmi_uint16_t request_datalen,
							const rpmi_uint8_t *request_data,
							rpmi_uint16_t *response_datalen,
							rpmi_uint8_t *response_data)
{
	*response_datalen = 4;
	((rpmi_uint32_t *)response_data)[0] =
			rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_ERR_NOTSUPP);

	return RPMI_SUCCESS;
}

void rpmi_context_process_a2p_request(struct rpmi_context *cntx)
{
	rpmi_bool_t do_process, do_acknowledge;
	struct rpmi_message *rmsg, *amsg;
	struct rpmi_service_group *group;
	struct rpmi_service *service;
	struct rpmi_transport *trans;
	enum rpmi_error rc;

	if (!cntx) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	trans = cntx->trans;
	rmsg = cntx->req_msg;
	amsg = cntx->ack_msg;
	while (!rpmi_transport_dequeue(trans, RPMI_QUEUE_A2P_REQ, rmsg)) {
		group = rpmi_context_find_group(cntx, rmsg->header.servicegroup_id);
		if (!group) {
			DPRINTF("%s: %s: service group ID 0x%x not found\n",
				__func__, cntx->name, rmsg->header.servicegroup_id);
			continue;
		}

		service = NULL;
		if (rmsg->header.service_id < group->max_service_id)
			service = &group->services[rmsg->header.service_id];

		amsg->header.flags = RPMI_MSG_ACKNOWLDGEMENT;
		amsg->header.service_id = rmsg->header.service_id;
		amsg->header.servicegroup_id = rmsg->header.servicegroup_id;
		amsg->header.datalen = 0;
		amsg->header.token = rmsg->header.token;

		do_process = false;
		do_acknowledge = false;
		switch (rmsg->header.flags & RPMI_MSG_FLAGS_TYPE) {
		case RPMI_MSG_NORMAL_REQUEST:
			do_process = true;
			do_acknowledge = true;
			break;
		case RPMI_MSG_POSTED_REQUEST:
			do_process = true;
			break;
		case RPMI_MSG_ACKNOWLDGEMENT:
			DPRINTF("%s: %s: group %s ignoring acknowledgment from a2p queue\n",
				__func__, cntx->name, group->name);
			break;
		case RPMI_MSG_NOTIFICATION:
			DPRINTF("%s: %s: group %s can't handle notification from a2p queue\n",
				__func__, cntx->name, group->name);
			break;
		default:
			break;
		}

		if (!do_process)
			continue;

		rpmi_env_lock(group->lock);
		if (service && service->process_a2p_request &&
		    rmsg->header.datalen >= service->min_a2p_request_datalen)
			rc = service->process_a2p_request(group, service, trans,
							rmsg->header.datalen, rmsg->data,
							&amsg->header.datalen, amsg->data);
		else
			rc = rpmi_service_notsupp_a2p_request(group, service, trans,
							rmsg->header.datalen, rmsg->data,
							&amsg->header.datalen, amsg->data);
		rpmi_env_unlock(group->lock);

		if (rc) {
			DPRINTF("%s: %s: group %s a2p request failed (error %d)\n",
				__func__, cntx->name, group->name, rc);
			DPRINTF("%s: %s: flags 0x%x service_id 0x%x servicegroup_id 0x%x\n",
				__func__, cntx->name,
				rmsg->header.flags, rmsg->header.service_id,
				rmsg->header.servicegroup_id);
			DPRINTF("%s: %s: datalen 0x%x token 0x%x\n",
				__func__, cntx->name,
				rmsg->header.datalen, rmsg->header.token);
			continue;
		}

		if (!do_acknowledge)
			continue;

		/**
		 * Try pushing the message in queue until successful
		 * or any other error apart from input/output error
		 * in case of queue full.
		 */
		do {
			rc = rpmi_transport_enqueue(trans, RPMI_QUEUE_P2A_ACK, amsg);
		} while (rc == RPMI_ERR_IO);

		if (rc) {
			DPRINTF("%s: %s: group %s p2a acknowledgment failed (error %d)\n",
				__func__, cntx->name, group->name, rc);
		}

		if ((rmsg->header.flags & RPMI_MSG_FLAGS_DOORBELL) &&
		    cntx->sysmsi_group)
			rpmi_service_group_sysmsi_inject_p2a(cntx->sysmsi_group);
	}
}

void rpmi_context_process_group_events(struct rpmi_context *cntx,
				       rpmi_uint16_t servicegroup_id)
{
	struct rpmi_service_group *group;
	enum rpmi_error rc;

	if (!cntx) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	group = rpmi_context_find_group(cntx, servicegroup_id);
	if (!group) {
		DPRINTF("%s: %s: group not found for servicegroup_id 0x%x\n",
			__func__, cntx->name, servicegroup_id);
		return;
	}
	if (!group->process_events) {
		DPRINTF("%s: %s: group %s does not support events\n",
			__func__, cntx->name, group->name);
		return;
	}

	rpmi_env_lock(group->lock);
	rc = group->process_events(group);
	rpmi_env_unlock(group->lock);
	if (rc && rc != RPMI_ERR_BUSY) {
		DPRINTF("%s: %s: group %s failed with error %d\n",
			__func__, cntx->name, group->name, rc);
	}
}

void rpmi_context_process_all_events(struct rpmi_context *cntx)
{
	struct rpmi_service_group *group;
	enum rpmi_error rc;
	rpmi_uint32_t i;

	if (!cntx) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_lock(cntx->groups_lock);

	for (i = 0; i < cntx->num_groups; i++) {
		group = cntx->groups[i];
		if (!group->process_events)
			continue;

		rpmi_env_unlock(cntx->groups_lock);

		rpmi_env_lock(group->lock);
		rc = group->process_events(group);
		rpmi_env_unlock(group->lock);
		if (rc && rc != RPMI_ERR_BUSY) {
			DPRINTF("%s: %s: group %s failed with error %d\n",
				__func__, cntx->name, group->name, rc);
		}

		rpmi_env_lock(cntx->groups_lock);
	}

	rpmi_env_unlock(cntx->groups_lock);
}

struct rpmi_service_group *rpmi_context_find_group(struct rpmi_context *cntx,
						   rpmi_uint16_t servicegroup_id)
{
	struct rpmi_service_group *ret = NULL;
	rpmi_uint32_t i;

	if (!cntx) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	rpmi_env_lock(cntx->groups_lock);

	for (i = 0; i < cntx->num_groups; i++) {
		if (cntx->groups[i]->servicegroup_id == servicegroup_id) {
			ret = cntx->groups[i];
			break;
		}
	}

	rpmi_env_unlock(cntx->groups_lock);

	return ret;
}

static enum rpmi_error rpmi_context_verify_privilege_level(struct rpmi_context *cntx,
						      struct rpmi_service_group *group)
{
	if (!group->privilege_level_bitmap)
		return RPMI_ERR_INVALID_PARAM;

	if (group->privilege_level_bitmap & (1U << cntx->privilege_level))
		return RPMI_SUCCESS;

	return RPMI_ERR_DENIED;
}

enum rpmi_error rpmi_context_add_group(struct rpmi_context *cntx,
				       struct rpmi_service_group *group)
{
	enum rpmi_error rc = RPMI_SUCCESS;
	rpmi_uint32_t i;

	if (!cntx || !group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	rpmi_env_lock(cntx->groups_lock);

	if (cntx->max_num_groups <= cntx->num_groups) {
		DPRINTF("%s: %s: no space to add group %s\n",
			__func__, cntx->name, group->name);
		rc = RPMI_ERR_IO;
		goto fail_unlock;
	}

	for (i = 0; i < cntx->num_groups; i++) {
		if (cntx->groups[i] == group) {
			DPRINTF("%s: %s: group %s alread added\n",
				__func__, cntx->name, group->name);
			rc = RPMI_ERR_ALREADY;
			goto fail_unlock;
		}
	}

	rc = rpmi_context_verify_privilege_level(cntx, group);
	if (rc)
		goto fail_unlock;

	cntx->groups[cntx->num_groups] = group;
	cntx->num_groups++;

	if (group->servicegroup_id == RPMI_SRVGRP_SYSTEM_MSI)
		cntx->sysmsi_group = group;

fail_unlock:
	rpmi_env_unlock(cntx->groups_lock);

	return rc;
}

void rpmi_context_remove_group(struct rpmi_context *cntx,
			       struct rpmi_service_group *group)
{
	rpmi_uint32_t i, j;

	if (!cntx || !group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_lock(cntx->groups_lock);

	for (i = 0; i < cntx->num_groups; i++) {
		if (cntx->groups[i] != group)
			continue;

		for (j = i; j < (cntx->num_groups - 1); j++)
			cntx->groups[j] = cntx->groups[j + 1];

		cntx->num_groups--;
		cntx->groups[cntx->num_groups] = NULL;
		if (group->servicegroup_id == RPMI_SRVGRP_SYSTEM_MSI)
			cntx->sysmsi_group = NULL;

		break;
	}

	rpmi_env_unlock(cntx->groups_lock);
}

struct rpmi_context *rpmi_context_create(const char *name,
					 struct rpmi_transport *trans,
					 rpmi_uint32_t max_num_groups,
					 enum rpmi_privilege_level privilege_level,
					 rpmi_uint32_t plat_info_len,
					 const char *plat_info)
{
	struct rpmi_context *cntx;
	enum rpmi_error rc;

	if (!name || !trans || !max_num_groups ||
			privilege_level >= RPMI_PRIVILEGE_LEVEL_MAX_IDX) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	if (plat_info && !plat_info_len) {
		DPRINTF("%s: invalid platform info parameters\n", __func__);
		return NULL;
	}

	cntx = rpmi_env_zalloc(sizeof(*cntx));
	if (!cntx) {
		DPRINTF("%s: %s: context allocation failed\n", __func__, name);
		return NULL;
	}

	cntx->name = name;
	cntx->trans = trans;
	cntx->max_num_groups = max_num_groups;
	cntx->privilege_level = privilege_level;

	/**
	 * Allocate for the array of pointers to the service
	 * groups instances which are assigned to the context
	 */
	cntx->groups = rpmi_env_zalloc(cntx->max_num_groups * sizeof(*cntx->groups));
	if (!cntx->groups) {
		DPRINTF("%s: %s: groups array allocation failed\n", __func__, name);
		goto fail_free_cntx;
	}

	cntx->groups_lock = rpmi_env_alloc_lock();

	cntx->req_msg = rpmi_env_zalloc(trans->slot_size);
	if (!cntx->req_msg) {
		DPRINTF("%s: %s: request message allocation failed\n", __func__, name);
		goto fail_free_groups;
	}

	cntx->ack_msg = rpmi_env_zalloc(trans->slot_size);
	if (!cntx->ack_msg) {
		DPRINTF("%s: %s: acknowledgment message allocation failed\n", __func__, name);
		goto fail_free_req_msg;
	}

	cntx->base_group = rpmi_base_group_create(cntx, plat_info_len, plat_info);
	if (!cntx->base_group) {
		DPRINTF("%s: %s: base group creation failed\n", __func__, name);
		goto fail_free_ack_msg;
	}

	rc = rpmi_context_add_group(cntx, cntx->base_group);
	if (rc) {
		DPRINTF("%s: %s: failed to add base group\n", __func__, name);
		goto fail_destroy_base;
	}

	return cntx;

fail_destroy_base:
	rpmi_base_group_destroy(cntx->base_group);
fail_free_ack_msg:
	rpmi_env_free(cntx->ack_msg);
fail_free_req_msg:
	rpmi_env_free(cntx->req_msg);
fail_free_groups:
	rpmi_env_free_lock(cntx->groups_lock);
	rpmi_env_free(cntx->groups);
fail_free_cntx:
	rpmi_env_free(cntx);
	return NULL;
}

void rpmi_context_destroy(struct rpmi_context *cntx)
{
	if (cntx->num_groups > 1) {
		DPRINTF("%s: %s: failed to destroy\n", __func__, cntx->name);
		return;
	}

	rpmi_context_remove_group(cntx, cntx->base_group);
	rpmi_base_group_destroy(cntx->base_group);

	rpmi_env_free(cntx->ack_msg);
	rpmi_env_free(cntx->req_msg);
	rpmi_env_free_lock(cntx->groups_lock);
	rpmi_env_free(cntx->groups);
	rpmi_env_free(cntx);
}
