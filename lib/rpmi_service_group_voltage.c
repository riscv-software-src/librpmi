/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Shanghai StarFive Technology Co., Ltd.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "librpmi_internal_list.h"

#ifdef LIBRPMI_DEBUG
#define DPRINTF(msg...)         rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define RPMI_VOLT_NAME_MAX_LEN  16

/** Convert list node pointer to struct rpmi_voltage instance pointer */
#define to_rpmi_voltage(__node)    \
	container_of((__node), struct rpmi_voltage_data, node)

/* A voltage domain instance */
struct rpmi_voltage {
	/* Lock to invoke the platform operations to
	 * protect this structure
	 */
	void *lock;
	/* domain ID */
	rpmi_uint32_t id;
	/* domain static attributes/data */
	const struct rpmi_voltage_data *vdata;
};

/** RPMI Voltage Service Group instance */
struct rpmi_voltage_group {
	/* Total domains count */
	rpmi_uint32_t volt_count;
	/* Pointer to voltage domains tree */
	struct rpmi_voltage *volt_tree;
	/* Common voltage platform operations (called with holding the lock)*/
	const struct rpmi_voltage_platform_ops *ops;
	/* Private data of platform voltage operations */
	void *ops_priv;
	struct rpmi_service_group group;
};

/** Get a struct rpmi_voltage instance pointer from voltage domain id */
static inline struct rpmi_voltage *
rpmi_get_voltage(struct rpmi_voltage_group *volt_group, rpmi_uint32_t volt_id)
{
	return &volt_group->volt_tree[volt_id];
}

static enum rpmi_error __rpmi_volt_get_attributes(struct rpmi_voltage_group *voltgrp,
						  rpmi_uint32_t voltid,
						  struct rpmi_voltage_attrs *attrs)
{
	struct rpmi_voltage *volt = rpmi_get_voltage(voltgrp, voltid);

	if (!attrs) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (!volt) {
		DPRINTF("%s: volt instance with voltid-%u not found\n",
			__func__, voltid);
		return RPMI_ERR_INVALID_PARAM;
	}

	attrs->name = volt->vdata->name;
	attrs->capability = volt->vdata->voltage_type | volt->vdata->control;
	attrs->num_levels = volt->vdata->num_levels;
	attrs->trans_latency = volt->vdata->trans_latency;
	attrs->config = volt->vdata->config;
	if (volt->vdata->discrete_range)
		attrs->level_array = volt->vdata->discrete_levels;
	else if (volt->vdata->linear_range)
		attrs->level_array = volt->vdata->linear_levels;
	else
		return RPMI_ERR_INVALID_PARAM;

	return RPMI_SUCCESS;
}

static enum rpmi_error __rpmi_volt_get_supp_levels(struct rpmi_voltage_group *voltgrp,
						   rpmi_int32_t *levels,
						   rpmi_uint32_t max_levels,
						   rpmi_uint32_t voltid,
						   rpmi_uint32_t index,
						   rpmi_uint32_t *returned_levels)
{
	struct rpmi_voltage *volt;
	enum rpmi_error ret;

	volt = rpmi_get_voltage(voltgrp, voltid);
	if (!volt) {
		DPRINTF("%s: volt instance with voltid-%u not found\n",
			__func__, voltid);
		return RPMI_ERR_INVALID_PARAM;
	}

	rpmi_env_lock(volt->lock);

	ret = voltgrp->ops->get_supp_levels(voltgrp->ops_priv, volt->id, max_levels,
					    index, returned_levels, levels);

	rpmi_env_unlock(volt->lock);

	return ret;
}

static enum rpmi_error __rpmi_volt_get_config(struct rpmi_voltage_group *voltgrp,
					      rpmi_uint32_t voltid,
					      rpmi_uint32_t *volt_config)
{
	enum rpmi_error ret;
	rpmi_uint32_t config;
	struct rpmi_voltage *volt = rpmi_get_voltage(voltgrp, voltid);

	if (!volt)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(volt->lock);

	ret = voltgrp->ops->get_config(voltgrp->ops_priv, volt->id, &config);

	*volt_config = config;

	rpmi_env_unlock(volt->lock);

	return ret;
}

static enum rpmi_error __rpmi_volt_set_config(struct rpmi_voltage_group *voltgrp,
					      rpmi_uint32_t voltid,
					      rpmi_uint32_t volt_config)
{
	enum rpmi_error ret;
	rpmi_uint32_t config;
	struct rpmi_voltage *volt = rpmi_get_voltage(voltgrp, voltid);

	if (!volt)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(volt->lock);

	ret = voltgrp->ops->get_config(voltgrp->ops_priv, volt->id, &config);
	if (ret)
		goto done;

	if (config == volt_config) {
		ret = RPMI_SUCCESS;
		goto done;
	}

	ret = voltgrp->ops->set_config(voltgrp->ops_priv, volt->id, volt_config);
done:
	rpmi_env_unlock(volt->lock);

	return ret;
}

static enum rpmi_error __rpmi_volt_get_level(struct rpmi_voltage_group *voltgrp,
					     rpmi_uint32_t voltid,
					     rpmi_int32_t *volt_level)
{
	enum rpmi_error ret;
	struct rpmi_voltage *volt = rpmi_get_voltage(voltgrp, voltid);

	if (!volt)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(volt->lock);

	ret = voltgrp->ops->get_level(voltgrp->ops_priv, volt->id, volt_level);

	rpmi_env_unlock(volt->lock);

	return ret;
}

static enum rpmi_error __rpmi_volt_set_level(struct rpmi_voltage_group *voltgrp,
					     rpmi_uint32_t voltid,
					     rpmi_int32_t *volt_level)
{
	enum rpmi_error ret;
	struct rpmi_voltage *volt = rpmi_get_voltage(voltgrp, voltid);

        if (!volt)
                return RPMI_ERR_INVALID_PARAM;

        rpmi_env_lock(volt->lock);

        ret = voltgrp->ops->set_level(voltgrp->ops_priv, volt->id, volt_level);

        rpmi_env_unlock(volt->lock);

        return ret;
}

/**
 * Initialize the voltage tree from provided
 * static platform voltage data.
 *
 * This function initializes the hierarchical structures
 * to represent the voltage association in the platform.
 **/
static struct rpmi_voltage *
rpmi_voltage_tree_init(rpmi_uint32_t volt_count,
		       const struct rpmi_voltage_data *volt_tree_data,
		       const struct rpmi_voltage_platform_ops *ops,
		       void *ops_priv)
{
	rpmi_uint32_t voltid;
	struct rpmi_voltage *volt;

	struct rpmi_voltage *volt_tree =
		rpmi_env_zalloc(sizeof(struct rpmi_voltage) * volt_count);

	/* initialize all volt domain instances */
	for (voltid = 0; voltid < volt_count; voltid++) {
		volt = &volt_tree[voltid];
		volt->id = voltid;
		volt->vdata = &volt_tree_data[voltid];

		volt->lock = rpmi_env_alloc_lock();
	}

	return volt_tree;
}

/*****************************************************************************
 * RPMI Voltage Service Group Functions
 ****************************************************************************/
static enum rpmi_error
rpmi_volt_get_num_domains(struct rpmi_service_group *group,
			  struct rpmi_service *service,
			  struct rpmi_transport *trans,
			  rpmi_uint16_t request_datalen,
			  const rpmi_uint8_t *request_data,
			  rpmi_uint16_t *response_datalen,
			  rpmi_uint8_t *response_data)
{
	struct rpmi_voltage_group *voltgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, voltgrp->volt_count);

	*response_datalen = 2 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_volt_get_attributes(struct rpmi_service_group *group,
			 struct rpmi_service *service,
			 struct rpmi_transport *trans,
			 rpmi_uint16_t request_datalen,
			 const rpmi_uint8_t *request_data,
			 rpmi_uint16_t *response_datalen,
			 rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	struct rpmi_voltage_attrs volt_attrs;
	struct rpmi_voltage_group *voltgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t voltid = rpmi_to_xe32(trans->is_be,
					    ((const rpmi_uint32_t *)request_data)[0]);

	if (voltid > voltgrp->volt_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = __rpmi_volt_get_attributes(voltgrp, voltid, &volt_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[1] = rpmi_to_xe32(trans->is_be, volt_attrs.capability);

	resp[2] = rpmi_to_xe32(trans->is_be, volt_attrs.num_levels);

	resp[3] = rpmi_to_xe32(trans->is_be, volt_attrs.trans_latency);

	if (volt_attrs.name)
		rpmi_env_strncpy((char *)&resp[4], volt_attrs.name, RPMI_VOLT_NAME_MAX_LEN);

	resp_dlen = 4 * sizeof(*resp) + RPMI_VOLT_NAME_MAX_LEN;

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_volt_get_config(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	rpmi_uint32_t volt_config = 0;
	enum rpmi_error ret;
	struct rpmi_voltage_group *voltgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_voltage *volt;

	rpmi_uint32_t voltid = rpmi_to_xe32(trans->is_be,
					    ((const rpmi_uint32_t *)request_data)[0]);

	if (voltid > voltgrp->volt_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	volt = rpmi_get_voltage(voltgrp, voltid);

	rpmi_env_lock(volt->lock);
	ret = __rpmi_volt_get_config(voltgrp, voltid, &volt_config);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}
	rpmi_env_unlock(volt->lock);

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)volt_config);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 2 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_volt_set_config(struct rpmi_service_group *group,
		     struct rpmi_service *service,
		     struct rpmi_transport *trans,
		     rpmi_uint16_t request_datalen,
		     const rpmi_uint8_t *request_data,
		     rpmi_uint16_t *response_datalen,
		     rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	struct rpmi_voltage_group *voltgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_voltage *volt;

	rpmi_uint32_t voltid = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[0]);
	rpmi_uint32_t volt_config = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[1]);

	if (voltid > voltgrp->volt_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	volt = rpmi_get_voltage(voltgrp, voltid);

	rpmi_env_lock(volt->lock);
	ret = __rpmi_volt_set_config(voltgrp, voltid, volt_config);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}
	rpmi_env_unlock(volt->lock);

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_volt_get_supp_levels(struct rpmi_service_group *group,
                          struct rpmi_service *service,
                          struct rpmi_transport *trans,
                          rpmi_uint16_t request_datalen,
                          const rpmi_uint8_t *request_data,
                          rpmi_uint16_t *response_datalen,
                          rpmi_uint8_t *response_data)
{
        enum rpmi_error ret;
        rpmi_uint32_t i;
        rpmi_uint32_t num_volt_level;
        rpmi_uint32_t resp_dlen = 0, volt_level_idx = 0;
        rpmi_uint32_t max_levels, remaining = 0, returned = 0;
	rpmi_uint32_t start;
        rpmi_int32_t *volt_level_array;
        struct rpmi_voltage_attrs volt_attrs;
        struct rpmi_voltage_group *voltgrp = group->priv;
        rpmi_uint32_t *resp = (void *)response_data;

        rpmi_uint32_t voltid = rpmi_to_xe32(trans->is_be,
                                ((const rpmi_uint32_t *)request_data)[0]);

        if (voltid > voltgrp->volt_count) {
                resp_dlen = sizeof(*resp);
                resp[0] = rpmi_to_xe32(trans->is_be,
                                       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
                goto done;
        }

        ret = __rpmi_volt_get_attributes(voltgrp, voltid, &volt_attrs);
        if (ret) {
                resp_dlen = sizeof(*resp);
                resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
                goto done;
        }

        num_volt_level = volt_attrs.num_levels;
        volt_level_array = volt_attrs.level_array;

        if (!num_volt_level || !volt_level_array) {
                resp_dlen = sizeof(*resp);
                resp[0] = rpmi_to_xe32(trans->is_be,
                                       (rpmi_uint32_t)RPMI_ERR_NOTSUPP);
                goto done;
        }

        volt_level_idx = rpmi_to_xe32(trans->is_be,
                                      ((const rpmi_uint32_t *)request_data)[1]);

        /* max volt levels a rpmi message can accommodate */
        max_levels =
                (RPMI_MSG_DATA_SIZE(trans->slot_size) - (4 * sizeof(*resp))) /
                                        sizeof(rpmi_int32_t);

        ret = __rpmi_volt_get_supp_levels(voltgrp, volt_level_array, max_levels,
                                          voltid, volt_level_idx, &returned);
        if (ret) {
                resp_dlen = sizeof(*resp);
                resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
                goto done;
        }

        for (i = 0; i < returned; i++) {
                start = 4;
                resp[start + i] = rpmi_to_xe32(trans->is_be,
                                        (rpmi_uint32_t)volt_level_array[i]);
        }

       remaining = num_volt_level - (volt_level_idx + returned);

        resp[3] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)returned);
        resp[2] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)remaining);
        /* No flags currently supported */
        resp[1] = rpmi_to_xe32(trans->is_be, 0);
        resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

        resp_dlen = (4 * sizeof(*resp)) +
                        (returned * sizeof(rpmi_uint32_t));

done:
        *response_datalen = resp_dlen;

        return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_volt_get_level(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	rpmi_int32_t volt_level;
	enum rpmi_error ret;
	struct rpmi_voltage_group *voltgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t voltid = rpmi_to_xe32(trans->is_be,
					    ((const rpmi_uint32_t *)request_data)[0]);

	if (voltid > voltgrp->volt_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	ret = __rpmi_volt_get_level(voltgrp, voltid, &volt_level);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_int32_t)volt_level);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 2 * sizeof(*resp);

done:

	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_volt_set_level(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	struct rpmi_voltage_group *voltgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_voltage *volt;

	rpmi_uint32_t voltid = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[0]);
	rpmi_int32_t volt_level = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[1]);

	if (voltid > voltgrp->volt_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	volt = rpmi_get_voltage(voltgrp, voltid);

	rpmi_env_lock(volt->lock);
	ret = __rpmi_volt_set_level(voltgrp, voltid, &volt_level);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}
	rpmi_env_unlock(volt->lock);

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_voltage_services[RPMI_VOLT_SRV_ID_MAX] = {
	[RPMI_VOLT_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_VOLT_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_VOLT_SRV_GET_NUM_DOMAINS] = {
		.service_id = RPMI_VOLT_SRV_GET_NUM_DOMAINS,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_volt_get_num_domains,
	},
	[RPMI_VOLT_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_VOLT_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_volt_get_attributes,
	},
	[RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS] = {
		.service_id = RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_volt_get_supp_levels,
	},
	[RPMI_VOLT_SRV_SET_CONFIG] = {
		.service_id = RPMI_VOLT_SRV_SET_CONFIG,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_volt_set_config,
	},
	[RPMI_VOLT_SRV_GET_CONFIG] = {
		.service_id = RPMI_VOLT_SRV_GET_CONFIG,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_volt_get_config,
	},
	[RPMI_VOLT_SRV_SET_VOLT_LEVEL] = {
		.service_id = RPMI_VOLT_SRV_SET_VOLT_LEVEL,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_volt_set_level,
	},
	[RPMI_VOLT_SRV_GET_VOLT_LEVEL] = {
		.service_id = RPMI_VOLT_SRV_GET_VOLT_LEVEL,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_volt_get_level,
	},
};

struct rpmi_service_group *
rpmi_service_group_voltage_create(rpmi_uint32_t volt_count,
				  const struct rpmi_voltage_data *volt_tree_data,
				  const struct rpmi_voltage_platform_ops *ops,
				  void *ops_priv)
{
	struct rpmi_voltage_group *voltgrp;
	struct rpmi_service_group *group;

	/* All critical parameters should be non-NULL */
	if (!volt_count || !volt_tree_data || !ops) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate voltage service group */
	voltgrp = rpmi_env_zalloc(sizeof(*voltgrp));
	if (!voltgrp) {
		DPRINTF("%s: failed to allocate voltage service group instance\n",
			__func__);
		return NULL;
	}

	voltgrp->volt_tree = rpmi_voltage_tree_init(volt_count,
						    volt_tree_data,
						    ops,
						    ops_priv);
	if (!voltgrp->volt_tree) {
		DPRINTF("%s: failed to initialize voltage tree\n", __func__);
		rpmi_env_free(voltgrp);
		return NULL;
	}

	voltgrp->volt_count = volt_count;
	voltgrp->ops = ops;
	voltgrp->ops_priv = ops_priv;

	group = &voltgrp->group;
	group->name = "voltage";
	group->servicegroup_id = RPMI_SRVGRP_VOLTAGE;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed for both M-mode and S-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK | RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_VOLT_SRV_ID_MAX;
	group->services = rpmi_voltage_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = voltgrp;

	return group;
}

void rpmi_service_group_voltage_destroy(struct rpmi_service_group *group)
{
	rpmi_uint32_t voltid;
	struct rpmi_voltage_group *voltgrp;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	voltgrp = group->priv;

	for (voltid = 0; voltid < voltgrp->volt_count; voltid++)
		rpmi_env_free_lock(voltgrp->volt_tree[voltid].lock);

	rpmi_env_free(voltgrp->volt_tree);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}
