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

struct rpmi_hsm_group {
	struct rpmi_hsm *hsm;

	struct rpmi_service_group group;
};

static enum rpmi_error rpmi_hsm_sg_hart_start(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *trans,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	struct rpmi_hsm_group *sghsm = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint64_t start_addr;
	enum rpmi_error status;
	rpmi_uint32_t hart_id;

	hart_id = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);
	start_addr = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[2]);
	start_addr = (start_addr << 32) |
		     rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[1]);

	status = rpmi_hsm_hart_start(sghsm->hsm, hart_id, start_addr);

	*response_datalen = sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_hsm_sg_hart_stop(struct rpmi_service_group *group,
					     struct rpmi_service *service,
					     struct rpmi_transport *trans,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data)
{
	struct rpmi_hsm_group *sghsm = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	enum rpmi_error status;
	rpmi_uint32_t hart_id;

	hart_id = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	status = rpmi_hsm_hart_stop(sghsm->hsm, hart_id);

	*response_datalen = sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_hsm_sg_hart_suspend(struct rpmi_service_group *group,
						struct rpmi_service *service,
						struct rpmi_transport *trans,
						rpmi_uint16_t request_datalen,
						const rpmi_uint8_t *request_data,
						rpmi_uint16_t *response_datalen,
						rpmi_uint8_t *response_data)
{
	const struct rpmi_hsm_suspend_type *suspend_type;
	struct rpmi_hsm_group *sghsm = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t hart_id, type;
	rpmi_uint64_t resume_addr;
	enum rpmi_error status;

	hart_id = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);
	type = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[1]);
	resume_addr = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[3]);
	resume_addr = (resume_addr << 32) |
		      rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[2]);

	suspend_type = rpmi_hsm_find_suspend_type(sghsm->hsm, type);
	if (suspend_type)
		status = rpmi_hsm_hart_suspend(sghsm->hsm, hart_id,
					       suspend_type, resume_addr);
	else
		status = RPMI_ERR_INVALID_PARAM;

	*response_datalen = sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_hsm_sg_get_hart_status(struct rpmi_service_group *group,
						   struct rpmi_service *service,
						   struct rpmi_transport *trans,
						   rpmi_uint16_t request_datalen,
						   const rpmi_uint8_t *request_data,
						   rpmi_uint16_t *response_datalen,
						   rpmi_uint8_t *response_data)
{
	struct rpmi_hsm_group *sghsm = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t hart_id;
	int status;

	hart_id = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	status = rpmi_hsm_get_hart_state(sghsm->hsm, hart_id);

	*response_datalen = 2 * sizeof(*resp);
	if (status < 0) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
		resp[1] = 0;
	} else {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
		resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	}

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_hsm_sg_get_hart_list(struct rpmi_service_group *group,
						 struct rpmi_service *service,
						 struct rpmi_transport *trans,
						 rpmi_uint16_t request_datalen,
						 const rpmi_uint8_t *request_data,
						 rpmi_uint16_t *response_datalen,
						 rpmi_uint8_t *response_data)
{
	rpmi_uint32_t start_index, max_entries, hart_count, hart_id;
	struct rpmi_hsm_group *sghsm = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t returned, remaining, i;
	enum rpmi_error status;

	hart_count = rpmi_hsm_hart_count(sghsm->hsm);
	max_entries = RPMI_MSG_DATA_SIZE(trans->slot_size) - (3 * sizeof(*resp));
	max_entries = rpmi_env_div32(max_entries, sizeof(*resp));

	start_index = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	if (start_index <= hart_count) {
		returned = max_entries < (hart_count - start_index) ?
			   max_entries : (hart_count - start_index);
		for (i = 0; i < returned; i++) {
			hart_id = rpmi_hsm_hart_index2id(sghsm->hsm, start_index + i);
			resp[3 + i] = rpmi_to_xe32(trans->is_be, hart_id);
		}
		remaining = hart_count - (start_index + returned);
		status = RPMI_SUCCESS;
	} else {
		returned = 0;
		remaining = hart_count;
		status = RPMI_ERR_INVALID_PARAM;
	}

	*response_datalen = (returned + 3) * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	resp[1] = rpmi_to_xe32(trans->is_be, remaining);
	resp[2] = rpmi_to_xe32(trans->is_be, returned);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_hsm_sg_get_suspend_types(struct rpmi_service_group *group,
						     struct rpmi_service *service,
						     struct rpmi_transport *trans,
						     rpmi_uint16_t request_datalen,
						     const rpmi_uint8_t *request_data,
						     rpmi_uint16_t *response_datalen,
						     rpmi_uint8_t *response_data)
{
	rpmi_uint32_t start_index, max_entries, type_count;
	const struct rpmi_hsm_suspend_type *suspend_type;
	struct rpmi_hsm_group *sghsm = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t returned, remaining, i;
	enum rpmi_error status;

	type_count = rpmi_hsm_get_suspend_type_count(sghsm->hsm);
	max_entries = RPMI_MSG_DATA_SIZE(trans->slot_size) - (3 * sizeof(*resp));
	max_entries = rpmi_env_div32(max_entries, sizeof(*resp));

	start_index = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	if (start_index <= type_count) {
		returned = max_entries < (type_count - start_index) ?
			   max_entries : (type_count - start_index);
		for (i = 0; i < returned; i++) {
			suspend_type = rpmi_hsm_get_suspend_type(sghsm->hsm, start_index + i);
			resp[3 + i] = rpmi_to_xe32(trans->is_be, suspend_type->type);
		}
		remaining = type_count - (start_index + returned);
		status = RPMI_SUCCESS;
	} else {
		returned = 0;
		remaining = type_count;
		status = RPMI_ERR_INVALID_PARAM;
	}

	*response_datalen = (returned + 3) * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	resp[1] = rpmi_to_xe32(trans->is_be, remaining);
	resp[2] = rpmi_to_xe32(trans->is_be, returned);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_hsm_sg_get_suspend_info(struct rpmi_service_group *group,
						    struct rpmi_service *service,
						    struct rpmi_transport *trans,
						    rpmi_uint16_t request_datalen,
						    const rpmi_uint8_t *request_data,
						    rpmi_uint16_t *response_datalen,
						    rpmi_uint8_t *response_data)
{
	const struct rpmi_hsm_suspend_type *suspend_type;
	struct rpmi_hsm_group *sghsm = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t type;

	type = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	suspend_type = rpmi_hsm_find_suspend_type(sghsm->hsm, type);

	*response_datalen = 6 * sizeof(*resp);
	if (suspend_type) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
		resp[1] = rpmi_to_xe32(trans->is_be, suspend_type->info.flags);
		resp[2] = rpmi_to_xe32(trans->is_be, suspend_type->info.entry_latency_us);
		resp[3] = rpmi_to_xe32(trans->is_be, suspend_type->info.exit_latency_us);
		resp[4] = rpmi_to_xe32(trans->is_be, suspend_type->info.wakeup_latency_us);
		resp[5] = rpmi_to_xe32(trans->is_be, suspend_type->info.min_residency_us);
	} else {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp[1] = 0;
		resp[2] = 0;
		resp[3] = 0;
		resp[4] = 0;
		resp[5] = 0;
	}

	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_hsm_services[RPMI_HSM_SRV_ID_MAX] = {
	[RPMI_HSM_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_HSM_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_HSM_SRV_HART_START] = {
		.service_id = RPMI_HSM_SRV_HART_START,
		.min_a2p_request_datalen = 12,
		.process_a2p_request = rpmi_hsm_sg_hart_start,
	},
	[RPMI_HSM_SRV_HART_STOP] = {
		.service_id = RPMI_HSM_SRV_HART_STOP,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_hsm_sg_hart_stop,
	},
	[RPMI_HSM_SRV_HART_SUSPEND] = {
		.service_id = RPMI_HSM_SRV_HART_SUSPEND,
		.min_a2p_request_datalen = 16,
		.process_a2p_request = rpmi_hsm_sg_hart_suspend,
	},
	[RPMI_HSM_SRV_GET_HART_STATUS] = {
		.service_id = RPMI_HSM_SRV_GET_HART_STATUS,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_hsm_sg_get_hart_status,
	},
	[RPMI_HSM_SRV_GET_HART_LIST] = {
		.service_id = RPMI_HSM_SRV_GET_HART_LIST,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_hsm_sg_get_hart_list,
	},
	[RPMI_HSM_SRV_GET_SUSPEND_TYPES] = {
		.service_id = RPMI_HSM_SRV_GET_SUSPEND_TYPES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_hsm_sg_get_suspend_types,
	},
	[RPMI_HSM_SRV_GET_SUSPEND_INFO] = {
		.service_id = RPMI_HSM_SRV_GET_SUSPEND_INFO,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_hsm_sg_get_suspend_info,
	},
};

static enum rpmi_error rpmi_hsm_process_events(struct rpmi_service_group *group)
{
	struct rpmi_hsm_group *sghsm = group->priv;

	rpmi_hsm_process_state_changes(sghsm->hsm);
	return RPMI_SUCCESS;
}

struct rpmi_service_group *rpmi_service_group_hsm_create(struct rpmi_hsm *hsm)
{
	struct rpmi_service_group *group;
	struct rpmi_hsm_group *sghsm;

	/* Critical parameter should be non-NULL */
	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate HSM group */
	sghsm = rpmi_env_zalloc(sizeof(*sghsm));
	if (!sghsm) {
		DPRINTF("%s: failed to allocate HSM service group instance\n",
			__func__);
		return NULL;
	}

	sghsm->hsm = hsm;

	group = &sghsm->group;
	group->name = "hsm";
	group->servicegroup_id = RPMI_SRVGRP_HSM;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed only for M-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK;
	group->max_service_id = RPMI_HSM_SRV_ID_MAX;
	group->services = rpmi_hsm_services;
	group->process_events = rpmi_hsm_process_events;
	group->lock = rpmi_env_alloc_lock();
	group->priv = sghsm;

	return group;
}

void rpmi_service_group_hsm_destroy(struct rpmi_service_group *group)
{
	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_free(group->priv);
}
