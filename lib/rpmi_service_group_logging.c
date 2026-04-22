// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Qualcomm Inc.
 *
 * Author: Subrahmanya Lingappa <subrahmanya.lingappa@qualcomm.com>
 */

#include <librpmi.h>

#ifdef DEBUG
#define DPRINTF(msg...)	rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct rpmi_logging_group {
	const struct rpmi_logging_platform_ops *ops;
	struct rpmi_service_group group;
	void *ops_priv;
};

static enum rpmi_error
rpmi_logging_set_state(struct rpmi_logging_group *sglogging,
		      rpmi_uint32_t log_type, rpmi_uint32_t datalen_bytes,
		      const void *data)
{
	enum rpmi_error ret;

	ret = sglogging->ops->do_set_state(sglogging->ops_priv, log_type,
					   datalen_bytes, data);

	return ret;
}

static enum rpmi_error
rpmi_logging_sg_set_config(struct rpmi_service_group *group,
			   struct rpmi_service *service,
			   struct rpmi_transport *trans,
			   rpmi_uint16_t request_datalen,
			   const rpmi_uint8_t *request_data,
			   rpmi_uint16_t *response_datalen,
			   rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	struct rpmi_logging_group *logginggrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t log_type;
	const rpmi_uint8_t *data;
	rpmi_uint32_t datalen_bytes;

	if (request_datalen < sizeof(rpmi_uint32_t)) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		*response_datalen = sizeof(*resp);
		return RPMI_SUCCESS;
	}

	log_type = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);
	data = request_data + sizeof(log_type);
	datalen_bytes = request_datalen - sizeof(log_type);

	/* change logging config synchronously */
	status = rpmi_logging_set_state(logginggrp, log_type, datalen_bytes,
					data);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

	*response_datalen = sizeof(*resp);

	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_logging_services[RPMI_LOGGING_SRV_ID_MAX] = {
	[RPMI_LOGGING_SRV_SET_CONFIG] = {
		.service_id = RPMI_LOGGING_SRV_SET_CONFIG,
		.min_a2p_request_datalen = 16,
		.process_a2p_request = rpmi_logging_sg_set_config,
	},
};

struct rpmi_service_group *rpmi_service_group_logging_create(
		const struct rpmi_logging_platform_ops *ops,
		void *ops_priv)
{
	struct rpmi_service_group *group;
	struct rpmi_logging_group *sglogging;

	/* All critical parameters should be non-NULL */
	if (!ops || !ops->do_set_state) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate logging group */
	sglogging = rpmi_env_zalloc(sizeof(*sglogging));
	if (!sglogging) {
		DPRINTF("%s: failed to allocate logging service group\n",
			__func__);
		return NULL;
	}

	sglogging->ops = ops;
	sglogging->ops_priv = ops_priv;

	group = &sglogging->group;
	group->name = "logging";
	group->servicegroup_id = RPMI_SRVGRP_LOGGING;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR,
				  RPMI_SPEC_VERSION_MINOR);
	/* Allowed only for M-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK;
	group->max_service_id = RPMI_LOGGING_SRV_ID_MAX;
	group->services = rpmi_logging_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = sglogging;

	return group;
}

void rpmi_service_group_logging_destroy(struct rpmi_service_group *group)
{
	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_free(group->priv);
}
