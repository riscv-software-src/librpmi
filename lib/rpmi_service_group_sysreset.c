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

struct rpmi_sysreset_group {
	rpmi_uint32_t sysreset_type_count;
	const rpmi_uint32_t *sysreset_types;

	const struct rpmi_sysreset_platform_ops *ops;
	void *ops_priv;

	struct rpmi_service_group group;
};

static const rpmi_uint32_t *
rpmi_sysreset_find_type(struct rpmi_sysreset_group *sgrst, rpmi_uint32_t type)
{
	rpmi_uint32_t i;

	for (i = 0; i < sgrst->sysreset_type_count; i++) {
		if (sgrst->sysreset_types[i] == type)
			return &sgrst->sysreset_types[i];
	}

	return NULL;
}

static enum rpmi_error rpmi_sysreset_get_attributes(struct rpmi_service_group *group,
						    struct rpmi_service *service,
						    struct rpmi_transport *trans,
						    rpmi_uint16_t request_datalen,
						    const rpmi_uint8_t *request_data,
						    rpmi_uint16_t *response_datalen,
						    rpmi_uint8_t *response_data)
{
	struct rpmi_sysreset_group *sgrst = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	const rpmi_uint32_t *sysreset_type;
	rpmi_uint32_t reset_type, attr = 0;

	reset_type = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	sysreset_type = rpmi_sysreset_find_type(sgrst, reset_type);
	attr = sysreset_type ? RPMI_SYSRST_ATTRS_FLAGS_RESETTYPE : 0;

	*response_datalen = 2 * sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, attr);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_sysreset_do_reset(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *trans,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	struct rpmi_sysreset_group *sgrst = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	const rpmi_uint32_t *sysreset_type;
	rpmi_uint32_t reset_type;

	reset_type = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	sysreset_type = rpmi_sysreset_find_type(sgrst, reset_type);
	if (sysreset_type) {
		DPRINTF("%s: Entering platform sysreset ..\n", __func__);
		/* No returning back after this function */
		sgrst->ops->do_system_reset(sgrst->ops_priv, reset_type);
	}

	/* reset_type is invalid at this point */
	*response_datalen = sizeof(*resp);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);

	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_sysreset_services[RPMI_SYSRST_SRV_ID_MAX] = {
	[RPMI_SYSRST_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_SYSRST_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_SYSRST_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_SYSRST_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_sysreset_get_attributes,
	},
	[RPMI_SYSRST_SRV_SYSTEM_RESET] = {
		.service_id = RPMI_SYSRST_SRV_SYSTEM_RESET,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_sysreset_do_reset,
	},
};

struct rpmi_service_group *
rpmi_service_group_sysreset_create(rpmi_uint32_t sysreset_type_count,
				   const rpmi_uint32_t *sysreset_types,
				   const struct rpmi_sysreset_platform_ops *ops,
				   void *ops_priv)
{
	struct rpmi_sysreset_group *sgrst;
	struct rpmi_service_group *group;

	/* All critical parameters should be non-NULL */
	if (!sysreset_type_count || !sysreset_types || !ops) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate system reset group */
	sgrst = rpmi_env_zalloc(sizeof(*sgrst));
	if (!sgrst) {
		DPRINTF("%s: failed to allocate system suspend service group instance\n",
			__func__);
		return NULL;
	}

	sgrst->sysreset_type_count = sysreset_type_count;
	sgrst->sysreset_types = sysreset_types;
	sgrst->ops = ops;
	sgrst->ops_priv = ops_priv;

	group = &sgrst->group;
	group->name = "sysreset";
	group->servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed only for M-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK;
	group->max_service_id = RPMI_SYSRST_SRV_ID_MAX;
	group->services = rpmi_sysreset_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = sgrst;

	return group;
}

void rpmi_service_group_sysreset_destroy(struct rpmi_service_group *group)
{
	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}
