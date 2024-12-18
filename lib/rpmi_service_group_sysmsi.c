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

struct rpmi_sysmsi_irq {
	rpmi_bool_t msi_enable;
	rpmi_bool_t msi_pending;
	rpmi_uint64_t msi_addr;
	rpmi_uint32_t msi_data;
};

struct rpmi_sysmsi_group {
	rpmi_uint32_t num_msis;
	struct rpmi_sysmsi_irq *msis;

	struct rpmi_service_group group;
};

static enum rpmi_error rpmi_sysmsi_num_msis(struct rpmi_service_group *group,
					    struct rpmi_service *service,
					    struct rpmi_transport *trans,
					    rpmi_uint16_t request_datalen,
					    const rpmi_uint8_t *request_data,
					    rpmi_uint16_t *response_datalen,
					    rpmi_uint8_t *response_data)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, sgmsi->num_msis);
	*response_datalen = 2 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_sysmsi_set_config(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *trans,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t config, msi_index;
	struct rpmi_sysmsi_irq *smsi;

	msi_index = rpmi_to_xe32(trans->is_be,
				 ((const rpmi_uint32_t *)request_data)[0]);
	if (sgmsi->num_msis <= msi_index) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	config = rpmi_to_xe32(trans->is_be,
			      ((const rpmi_uint32_t *)request_data)[1]);
	smsi = &sgmsi->msis[msi_index];
	smsi->msi_enable = (config & 0x1) ? true : false;
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

done:
	*response_datalen = sizeof(*resp);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_sysmsi_get_config(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *trans,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_sysmsi_irq *smsi;
	rpmi_uint16_t resp_dlen = 0;
	rpmi_uint32_t msi_index;

	msi_index = rpmi_to_xe32(trans->is_be,
				 ((const rpmi_uint32_t *)request_data)[0]);
	if (sgmsi->num_msis <= msi_index) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	smsi = &sgmsi->msis[msi_index];
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)smsi->msi_enable);
	resp_dlen = 2 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_sysmsi_set_target(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *trans,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	rpmi_uint32_t maddr_lo, maddr_hi, mdata, msi_index;
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_sysmsi_irq *smsi;

	msi_index = rpmi_to_xe32(trans->is_be,
				 ((const rpmi_uint32_t *)request_data)[0]);
	if (sgmsi->num_msis <= msi_index) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	maddr_lo = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[1]);
	maddr_hi = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[2]);
	mdata = rpmi_to_xe32(trans->is_be,
			     ((const rpmi_uint32_t *)request_data)[3]);

	smsi = &sgmsi->msis[msi_index];
	smsi->msi_addr = ((rpmi_uint64_t)maddr_hi << 32) | maddr_lo;
	smsi->msi_data = mdata;
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

done:
	*response_datalen = sizeof(*resp);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_sysmsi_get_target(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *trans,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_sysmsi_irq *smsi;
	rpmi_uint16_t resp_dlen = 0;
	rpmi_uint32_t msi_index;

	msi_index = rpmi_to_xe32(trans->is_be,
				 ((const rpmi_uint32_t *)request_data)[0]);
	if (sgmsi->num_msis <= msi_index) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	smsi = &sgmsi->msis[msi_index];
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)smsi->msi_addr);
	resp[2] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)(smsi->msi_addr >> 32));
	resp[3] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)smsi->msi_data);
	resp_dlen = 4 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_sysmsi_services[RPMI_SYSMSI_SRV_ID_MAX] = {
	[RPMI_SYSMSI_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_SYSMSI_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_SYSMSI_SRV_NUM_MSIS] = {
		.service_id = RPMI_SYSMSI_SRV_NUM_MSIS,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_sysmsi_num_msis,
	},
	[RPMI_SYSMSI_SRV_SET_CONFIG] = {
		.service_id = RPMI_SYSMSI_SRV_SET_CONFIG,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_sysmsi_set_config,
	},
	[RPMI_SYSMSI_SRV_GET_CONFIG] = {
		.service_id = RPMI_SYSMSI_SRV_GET_CONFIG,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_sysmsi_get_config,
	},
	[RPMI_SYSMSI_SRV_SET_TARGET] = {
		.service_id = RPMI_SYSMSI_SRV_SET_TARGET,
		.min_a2p_request_datalen = 16,
		.process_a2p_request = rpmi_sysmsi_set_target,
	},
	[RPMI_SYSMSI_SRV_GET_TARGET] = {
		.service_id = RPMI_SYSMSI_SRV_GET_TARGET,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_sysmsi_get_target,
	},
};

static enum rpmi_error rpmi_sysmsi_process_events(struct rpmi_service_group *group)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	struct rpmi_sysmsi_irq *smsi;
	rpmi_uint32_t msi_index;

	for (msi_index = 0; msi_index < sgmsi->num_msis; msi_index++) {
		smsi = &sgmsi->msis[msi_index];
		if (smsi->msi_enable && smsi->msi_pending) {
			rpmi_env_writel(smsi->msi_addr, smsi->msi_data);
			smsi->msi_pending = false;
		}
	}

	return RPMI_SUCCESS;
}

enum rpmi_error rpmi_service_group_sysmsi_inject(struct rpmi_service_group *group,
						 rpmi_uint32_t msi_index)
{
	struct rpmi_sysmsi_group *sgmsi;
	struct rpmi_sysmsi_irq *smsi;

	if (!group)
		return RPMI_ERR_INVALID_PARAM;

	sgmsi = group->priv;
	if (sgmsi->num_msis <= msi_index)
		return RPMI_ERR_INVALID_PARAM;
	smsi = &sgmsi->msis[msi_index];

	rpmi_env_lock(group->lock);
	smsi->msi_pending = true;
	rpmi_env_unlock(group->lock);

	return 0;
}

struct rpmi_service_group *
rpmi_service_group_sysmsi_create(rpmi_uint32_t num_msis)
{
	struct rpmi_service_group *group;
	struct rpmi_sysmsi_group *sgmsi;

	/* All critical parameters should be non-NULL */
	if (!num_msis) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate system MSI group */
	sgmsi = rpmi_env_zalloc(sizeof(*sgmsi));
	if (!sgmsi) {
		DPRINTF("%s: failed to allocate system MSI service group instance\n",
			__func__);
		return NULL;
	}

	sgmsi->num_msis = num_msis;
	sgmsi->msis = rpmi_env_zalloc(sizeof(*sgmsi->msis) * sgmsi->num_msis);
	if (!sgmsi) {
		DPRINTF("%s: failed to allocate system MSI array\n",
			__func__);
		rpmi_env_free(sgmsi);
		return NULL;
	}

	group = &sgmsi->group;
	group->name = "sysmsi";
	group->servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK |
					RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_SYSMSI_SRV_ID_MAX;
	group->services = rpmi_sysmsi_services;
	group->process_events = rpmi_sysmsi_process_events;
	group->lock = rpmi_env_alloc_lock();
	group->priv = sgmsi;

	return group;
}

void rpmi_service_group_sysmsi_destroy(struct rpmi_service_group *group)
{
	struct rpmi_sysmsi_group *sgmsi;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}
	sgmsi = group->priv;

	rpmi_env_free_lock(group->lock);
	rpmi_env_free(sgmsi->msis);
	rpmi_env_free(sgmsi);
}
