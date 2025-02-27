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

enum rpmi_syssusp_state {
	RPMI_SYSSUSP_STATE_RUNNING = 0,
	RPMI_SYSSUSP_STATE_SUSPEND_PENDING,
	RPMI_SYSSUSP_STATE_SUSPENDED,
};

struct rpmi_syssusp_group {
	struct rpmi_hsm *hsm;

	rpmi_uint32_t syssusp_type_count;
	const struct rpmi_system_suspend_type *syssusp_types;

	enum rpmi_syssusp_state current_state;
	rpmi_uint32_t current_hart_index;
	const struct rpmi_system_suspend_type *current_syssusp_type;
	rpmi_uint64_t current_resume_addr;

	const struct rpmi_syssusp_platform_ops *ops;
	void *ops_priv;

	struct rpmi_service_group group;
};

static const struct rpmi_system_suspend_type *
rpmi_syssusp_find_type(struct rpmi_syssusp_group *sgsusp, rpmi_uint32_t type)
{
	rpmi_uint32_t i;

	for (i = 0; i < sgsusp->syssusp_type_count; i++) {
		if (sgsusp->syssusp_types[i].type == type)
			return &sgsusp->syssusp_types[i];
	}

	return NULL;
}

static enum rpmi_error rpmi_syssusp_get_attributes(struct rpmi_service_group *group,
						   struct rpmi_service *service,
						   struct rpmi_transport *trans,
						   rpmi_uint16_t request_datalen,
						   const rpmi_uint8_t *request_data,
						   rpmi_uint16_t *response_datalen,
						   rpmi_uint8_t *response_data)
{
	const struct rpmi_system_suspend_type *syssusp_type = NULL;
	struct rpmi_syssusp_group *sgsusp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t type, attr = 0;

	type = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	syssusp_type = rpmi_syssusp_find_type(sgsusp, type);
	if (syssusp_type) {
		attr |= RPMI_SYSSUSP_ATTRS_FLAGS_SUSPENDTYPE;
		attr |= (syssusp_type->attr &
			 RPMI_SYSSUSP_ATTRS_FLAGS_RESUMEADDR);
	}

	*response_datalen = 2 * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, attr);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_syssusp_do_suspend(struct rpmi_service_group *group,
					       struct rpmi_service *service,
					       struct rpmi_transport *trans,
					       rpmi_uint16_t request_datalen,
					       const rpmi_uint8_t *request_data,
					       rpmi_uint16_t *response_datalen,
					       rpmi_uint8_t *response_data)
{
	const struct rpmi_system_suspend_type *syssusp_type = NULL;
	struct rpmi_syssusp_group *sgsusp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t i, hart_id, hart_index, type;
	rpmi_uint64_t resume_addr;
	int status;

	hart_id = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);
	type = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[1]);
	resume_addr = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[3]);
	resume_addr = (resume_addr << 32) |
		      rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[2]);

	hart_index = rpmi_hsm_hart_id2index(sgsusp->hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		status = RPMI_ERR_INVALID_PARAM;
		goto done;
	}

	syssusp_type = rpmi_syssusp_find_type(sgsusp, type);
	if (!syssusp_type) {
		status = RPMI_ERR_INVALID_PARAM;
		goto done;
	}

	if (sgsusp->current_state != RPMI_SYSSUSP_STATE_RUNNING) {
		status = RPMI_ERR_ALREADY;
		goto done;
	}

	for (i = 0; i < rpmi_hsm_hart_count(sgsusp->hsm); i++) {
		if (i == hart_index)
			continue;
		status = rpmi_hsm_get_hart_state(sgsusp->hsm,
						 rpmi_hsm_hart_index2id(sgsusp->hsm, i));
		if (status < 0)
			goto done;
		if (status != RPMI_HSM_HART_STATE_STOPPED) {
			status = RPMI_ERR_DENIED;
			goto done;
		}
	}

	status = sgsusp->ops->system_suspend_prepare(sgsusp->ops_priv, hart_index,
						     syssusp_type, resume_addr);
	if (status)
		goto done;

	sgsusp->current_hart_index = hart_index;
	sgsusp->current_syssusp_type = syssusp_type;
	sgsusp->current_resume_addr = resume_addr;
	sgsusp->current_state = RPMI_SYSSUSP_STATE_SUSPEND_PENDING;

done:
	*response_datalen = sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_syssusp_services[RPMI_SYSSUSP_SRV_ID_MAX] = {
	[RPMI_SYSRST_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_SYSRST_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_SYSSUSP_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_SYSSUSP_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_syssusp_get_attributes,
	},
	[RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND] = {
		.service_id = RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND,
		.min_a2p_request_datalen = 16,
		.process_a2p_request = rpmi_syssusp_do_suspend,
	},
};

static enum rpmi_error rpmi_syssusp_process_events(struct rpmi_service_group *group)
{
	struct rpmi_syssusp_group *sgsusp = group->priv;
	enum rpmi_error status;

	switch (sgsusp->current_state) {
	case RPMI_SYSSUSP_STATE_SUSPEND_PENDING:
		if (!sgsusp->ops->system_suspend_ready(sgsusp->ops_priv,
						       sgsusp->current_hart_index))
			return RPMI_ERR_BUSY;
		sgsusp->ops->system_suspend_finalize(sgsusp->ops_priv,
						sgsusp->current_hart_index,
						sgsusp->current_syssusp_type,
						sgsusp->current_resume_addr);
		sgsusp->current_state = RPMI_SYSSUSP_STATE_SUSPENDED;
		break;
	case RPMI_SYSSUSP_STATE_SUSPENDED:
		if (!sgsusp->ops->system_suspend_can_resume(sgsusp->ops_priv,
							    sgsusp->current_hart_index))
			return RPMI_ERR_BUSY;
		status = sgsusp->ops->system_suspend_resume(sgsusp->ops_priv,
							sgsusp->current_hart_index,
							sgsusp->current_syssusp_type,
							sgsusp->current_resume_addr);
		if (status)
			return status;
		sgsusp->current_state = RPMI_SYSSUSP_STATE_RUNNING;
		break;
	case RPMI_SYSSUSP_STATE_RUNNING:
	default:
		break;
	}

	return RPMI_SUCCESS;
}

struct rpmi_service_group *
rpmi_service_group_syssusp_create(struct rpmi_hsm *hsm,
				  rpmi_uint32_t syssusp_type_count,
				  const struct rpmi_system_suspend_type *syssusp_types,
				  const struct rpmi_syssusp_platform_ops *ops,
				  void *ops_priv)
{
	struct rpmi_syssusp_group *sgsusp;
	struct rpmi_service_group *group;

	/* All critical parameters should be non-NULL */
	if (!hsm || !syssusp_type_count || !syssusp_types || !ops) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* All callbacks should be non-NULL */
	if (!ops->system_suspend_prepare || !ops->system_suspend_finalize ||
	    !ops->system_suspend_can_resume || !ops->system_suspend_resume) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate system suspend group */
	sgsusp = rpmi_env_zalloc(sizeof(*sgsusp));
	if (!sgsusp) {
		DPRINTF("%s: failed to allocate system suspend service group instance\n",
			__func__);
		return NULL;
	}

	sgsusp->hsm = hsm;
	sgsusp->syssusp_type_count = syssusp_type_count;
	sgsusp->syssusp_types = syssusp_types;
	sgsusp->current_state = RPMI_SYSSUSP_STATE_RUNNING;
	sgsusp->current_syssusp_type = NULL;
	sgsusp->current_resume_addr = 0;
	sgsusp->ops = ops;
	sgsusp->ops_priv = ops_priv;

	group = &sgsusp->group;
	group->name = "syssusp";
	group->servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed only for M-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK;
	group->max_service_id = RPMI_SYSSUSP_SRV_ID_MAX;
	group->services = rpmi_syssusp_services;
	group->process_events = rpmi_syssusp_process_events;
	group->lock = rpmi_env_alloc_lock();
	group->priv = sgsusp;

	return group;
}

void rpmi_service_group_syssusp_destroy(struct rpmi_service_group *group)
{
	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}
