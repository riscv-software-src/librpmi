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

struct rpmi_sysmsi_irq {
	/** MSI state */
	rpmi_bool_t msi_enable;
	rpmi_bool_t msi_pending;
	/** MSI target */
	rpmi_bool_t msi_valid;
	rpmi_uint64_t msi_addr;
	rpmi_uint32_t msi_data;
};

struct rpmi_sysmsi_group {
	/** Service group attributes */
	rpmi_uint32_t num_msi;
	rpmi_uint32_t p2a_msi_index;

	/** Array of system MSIs */
	struct rpmi_sysmsi_irq *msis;

	/** Private data of platform cppc operations */
	const struct rpmi_sysmsi_platform_ops *ops;
	void *ops_priv;

	struct rpmi_service_group group;
};

static enum rpmi_error rpmi_sysmsi_get_attrs(struct rpmi_service_group *group,
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
	resp[1] = rpmi_to_xe32(trans->is_be, sgmsi->num_msi);
	resp[2] = 0;
	resp[3] = 0;
	*response_datalen = 4 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_sysmsi_get_mattrs(struct rpmi_service_group *group,
					     struct rpmi_service *service,
					     struct rpmi_transport *trans,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t flag0, msi_index;
	rpmi_uint16_t resp_dlen = 0;

	msi_index = rpmi_to_xe32(trans->is_be,
				 ((const rpmi_uint32_t *)request_data)[0]);
	if (sgmsi->num_msi <= msi_index) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	flag0 = 0;
	if (sgmsi->ops->mmode_preferred)
		flag0 |= sgmsi->ops->mmode_preferred(sgmsi->ops_priv, msi_index) ?
			 RPMI_SYSMSI_MSI_ATTRIBUTES_FLAG0_PREF_PRIV : 0;
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, flag0);
	resp[2] = 0;
	resp[3] = 0;
	resp[4] = 0;
	resp[5] = 0;
	resp[6] = 0;
	if (sgmsi->ops->get_name)
		sgmsi->ops->get_name(sgmsi->ops_priv, msi_index,
				     (char *)&resp[3], 4 * sizeof(*resp));
	resp_dlen = 7 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_sysmsi_set_state(struct rpmi_service_group *group,
					     struct rpmi_service *service,
					     struct rpmi_transport *trans,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t state, msi_index;
	struct rpmi_sysmsi_irq *smsi;

	msi_index = rpmi_to_xe32(trans->is_be,
				 ((const rpmi_uint32_t *)request_data)[0]);
	if (sgmsi->num_msi <= msi_index) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	state = rpmi_to_xe32(trans->is_be,
			      ((const rpmi_uint32_t *)request_data)[1]);
	smsi = &sgmsi->msis[msi_index];
	smsi->msi_enable = (state & RPMI_SYSMSI_MSI_STATE_ENABLE) ? true : false;
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

done:
	*response_datalen = sizeof(*resp);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_sysmsi_get_state(struct rpmi_service_group *group,
					     struct rpmi_service *service,
					     struct rpmi_transport *trans,
					     rpmi_uint16_t request_datalen,
					     const rpmi_uint8_t *request_data,
					     rpmi_uint16_t *response_datalen,
					     rpmi_uint8_t *response_data)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t state, msi_index;
	struct rpmi_sysmsi_irq *smsi;
	rpmi_uint16_t resp_dlen = 0;

	msi_index = rpmi_to_xe32(trans->is_be,
				 ((const rpmi_uint32_t *)request_data)[0]);
	if (sgmsi->num_msi <= msi_index) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	smsi = &sgmsi->msis[msi_index];
	state = smsi->msi_enable ? RPMI_SYSMSI_MSI_STATE_ENABLE : 0;
	state |= smsi->msi_pending ? RPMI_SYSMSI_MSI_STATE_PENDING : 0;
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, state);
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
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_sysmsi_irq *smsi;
	rpmi_uint32_t msi_index;
	rpmi_uint64_t maddr;

	msi_index = rpmi_to_xe32(trans->is_be,
				 ((const rpmi_uint32_t *)request_data)[0]);
	if (sgmsi->num_msi <= msi_index) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	maddr = rpmi_to_xe32(trans->is_be,
			     ((const rpmi_uint32_t *)request_data)[1]);
	maddr |= ((rpmi_uint64_t)rpmi_to_xe32(trans->is_be,
			     ((const rpmi_uint32_t *)request_data)[2])) << 32;
	if (!sgmsi->ops->validate_msi_addr(sgmsi->ops_priv, maddr)) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_ADDR);
		goto done;
	}

	smsi = &sgmsi->msis[msi_index];
	smsi->msi_addr = maddr;
	smsi->msi_data = rpmi_to_xe32(trans->is_be,
				      ((const rpmi_uint32_t *)request_data)[3]);
	smsi->msi_valid = true;
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
	if (sgmsi->num_msi <= msi_index) {
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
	[RPMI_SYSMSI_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_SYSMSI_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_sysmsi_get_attrs,
	},
	[RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES] = {
		.service_id = RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_sysmsi_get_mattrs,
	},
	[RPMI_SYSMSI_SRV_SET_MSI_STATE] = {
		.service_id = RPMI_SYSMSI_SRV_SET_MSI_STATE,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_sysmsi_set_state,
	},
	[RPMI_SYSMSI_SRV_GET_MSI_STATE] = {
		.service_id = RPMI_SYSMSI_SRV_GET_MSI_STATE,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_sysmsi_get_state,
	},
	[RPMI_SYSMSI_SRV_SET_MSI_TARGET] = {
		.service_id = RPMI_SYSMSI_SRV_SET_MSI_TARGET,
		.min_a2p_request_datalen = 16,
		.process_a2p_request = rpmi_sysmsi_set_target,
	},
	[RPMI_SYSMSI_SRV_GET_MSI_TARGET] = {
		.service_id = RPMI_SYSMSI_SRV_GET_MSI_TARGET,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_sysmsi_get_target,
	},
};

static enum rpmi_error rpmi_sysmsi_process_events(struct rpmi_service_group *group)
{
	struct rpmi_sysmsi_group *sgmsi = group->priv;
	struct rpmi_sysmsi_irq *smsi;
	rpmi_uint32_t msi_index;

	for (msi_index = 0; msi_index < sgmsi->num_msi; msi_index++) {
		smsi = &sgmsi->msis[msi_index];
		if (smsi->msi_enable && smsi->msi_pending && smsi->msi_valid) {
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
	enum rpmi_error ret;

	if (!group)
		return RPMI_ERR_INVALID_PARAM;

	sgmsi = group->priv;
	if (sgmsi->num_msi <= msi_index)
		return RPMI_ERR_INVALID_PARAM;
	smsi = &sgmsi->msis[msi_index];

	rpmi_env_lock(group->lock);
	smsi->msi_pending = true;
	ret = rpmi_sysmsi_process_events(group);
	rpmi_env_unlock(group->lock);

	return ret;
}

enum rpmi_error rpmi_service_group_sysmsi_inject_p2a(struct rpmi_service_group *group)
{
	struct rpmi_sysmsi_group *sgmsi;

	if (!group)
		return RPMI_ERR_INVALID_PARAM;

	sgmsi = group->priv;
	if (sgmsi->num_msi <= sgmsi->p2a_msi_index)
		return RPMI_ERR_NOTSUPP;

	return rpmi_service_group_sysmsi_inject(group, sgmsi->p2a_msi_index);
}

struct rpmi_service_group *
rpmi_service_group_sysmsi_create(rpmi_uint32_t num_msi,
				 rpmi_uint32_t p2a_msi_index,
				 const struct rpmi_sysmsi_platform_ops *ops,
				 void *ops_priv)
{
	struct rpmi_service_group *group;
	struct rpmi_sysmsi_group *sgmsi;

	/* All critical parameters should be non-NULL */
	if (!num_msi || !ops || !ops->validate_msi_addr) {
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

	sgmsi->num_msi = num_msi;
	sgmsi->p2a_msi_index = p2a_msi_index < num_msi ? p2a_msi_index : -1U;
	sgmsi->msis = rpmi_env_zalloc(sizeof(*sgmsi->msis) * sgmsi->num_msi);
	if (!sgmsi) {
		DPRINTF("%s: failed to allocate system MSI array\n",
			__func__);
		rpmi_env_free(sgmsi);
		return NULL;
	}
	sgmsi->ops = ops;
	sgmsi->ops_priv = ops_priv;

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
