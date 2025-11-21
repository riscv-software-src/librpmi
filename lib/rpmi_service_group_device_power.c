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

#define RPMI_DPWR_NAME_MAX_LEN  16

/** Convert list node pointer to struct rpmi_perf instance pointer */
#define to_rpmi_perf(__node)    \
	container_of((__node), struct rpmi_perf, node)

/* A device power domain instance */
struct rpmi_dpwr {
	/* Lock to invoke the platform operations to
	 * protect this structure
	 */
	void *lock;
	/* DPWR domain ID */
	rpmi_uint32_t id;
	/* DPWR static attributes/data */
	const struct rpmi_dpwr_data *pdata;
};

/** RPMI DPWR Service Group instance */
struct rpmi_dpwr_group {
	/* Total dpwr domains count */
	rpmi_uint32_t dpwr_count;
	/* Pointer to dpwr domains tree */
	struct rpmi_dpwr *dpwr_tree;
	/* Common dpwr platform operations (called with holding the lock)*/
	const struct rpmi_dpwr_platform_ops *ops;
	/* Private data of platform dpwr operations */
	void *ops_priv;
	struct rpmi_service_group group;
};

/** Get a struct rpmi_dpwr instance pointer from perf id */
static inline struct rpmi_dpwr *
rpmi_get_dpwr(struct rpmi_dpwr_group *dpwr_group, rpmi_uint32_t dpwr_id)
{
	return &dpwr_group->dpwr_tree[dpwr_id];
}

static enum rpmi_error __rpmi_dpwr_get_attributes(struct rpmi_dpwr_group *dpwrgrp,
						  rpmi_uint32_t dpwrid,
						  struct rpmi_dpwr_attrs *dpwr_attrs)
{
	enum rpmi_error ret;
	struct rpmi_dpwr *dpwr = rpmi_get_dpwr(dpwrgrp, dpwrid);

	if (!dpwr_attrs) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (!dpwr) {
		DPRINTF("%s: dpwr instance with dpwrid-%u not found\n",
			__func__, dpwrid);
		return RPMI_ERR_INVALID_PARAM;
	}

	dpwr_attrs->name = dpwr->pdata->name;
	dpwr_attrs->trans_latency = dpwr->pdata->trans_latency;
	ret = RPMI_SUCCESS;

	return ret;
}

static enum rpmi_error __rpmi_dpwr_get_state(struct rpmi_dpwr_group *dpwrgrp,
					     rpmi_uint32_t dpwrid,
					     rpmi_uint32_t *dpwr_state)
{
	enum rpmi_error ret;
	rpmi_uint32_t state;
	struct rpmi_dpwr *dpwr = rpmi_get_dpwr(dpwrgrp, dpwrid);

	if (!dpwr)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(dpwr->lock);

	ret = dpwrgrp->ops->get_state(dpwrgrp->ops_priv, dpwr->id, &state);
	if (ret)
		goto done;

	if (state != RPMI_DPWR_STATE_ON && state != RPMI_DPWR_STATE_OFF) {
		ret = RPMI_ERR_INVALID_STATE;
		goto done;
	}

	*dpwr_state = state;
	ret = RPMI_SUCCESS;

done:
	rpmi_env_unlock(dpwr->lock);

	return ret;
}

static enum rpmi_error __rpmi_dpwr_set_state(struct rpmi_dpwr_group *dpwrgrp,
					     rpmi_uint32_t dpwrid,
					     rpmi_uint32_t dpwr_state)
{
	enum rpmi_error ret;
	rpmi_uint32_t state;
	struct rpmi_dpwr *dpwr = rpmi_get_dpwr(dpwrgrp, dpwrid);

	if (!dpwr)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(dpwr->lock);

	ret = dpwrgrp->ops->get_state(dpwrgrp->ops_priv, dpwr->id, &state);
	if (ret)
		goto done;

	if (state == dpwr_state) {
		ret = RPMI_SUCCESS;
		goto done;
	}

	ret = dpwrgrp->ops->set_state(dpwrgrp->ops_priv, dpwr->id, dpwr_state);

done:
	rpmi_env_unlock(dpwr->lock);

	return ret;
}

/**
 * Initialize the dpwr tree from provided
 * static platform dpwr data.
 *
 * This function initializes the hierarchical structures
 * to represent the dpwr association in the platform.
 **/
static struct rpmi_dpwr *
rpmi_dpwr_tree_init(rpmi_uint32_t dpwr_count,
		    const struct rpmi_dpwr_data *dpwr_tree_data,
		    const struct rpmi_dpwr_platform_ops *ops,
		    void *ops_priv)
{
	rpmi_uint32_t dpwrid;
	struct rpmi_dpwr *dpwr;

	struct rpmi_dpwr *dpwr_tree =
		rpmi_env_zalloc(sizeof(struct rpmi_dpwr) * dpwr_count);
	if (!dpwr_tree)
		return NULL;

	/* initialize all dpwr domain instances */
	for (dpwrid = 0; dpwrid < dpwr_count; dpwrid++) {
		dpwr = &dpwr_tree[dpwrid];
		dpwr->id = dpwrid;
		dpwr->pdata = &dpwr_tree_data[dpwrid];
		dpwr->lock = rpmi_env_alloc_lock();
	}

	return dpwr_tree;
}

/*****************************************************************************
 * RPMI Device Power Service Group Functions
 ****************************************************************************/
static enum rpmi_error
rpmi_dpwr_get_num_domains(struct rpmi_service_group *group,
			  struct rpmi_service *service,
			  struct rpmi_transport *trans,
			  rpmi_uint16_t request_datalen,
			  const rpmi_uint8_t *request_data,
			  rpmi_uint16_t *response_datalen,
			  rpmi_uint8_t *response_data)
{
	struct rpmi_dpwr_group *dpwrgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	resp[1] = rpmi_to_xe32(trans->is_be, dpwrgrp->dpwr_count);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	*response_datalen = 2 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_dpwr_get_attributes(struct rpmi_service_group *group,
			 struct rpmi_service *service,
			 struct rpmi_transport *trans,
			 rpmi_uint16_t request_datalen,
			 const rpmi_uint8_t *request_data,
			 rpmi_uint16_t *response_datalen,
			 rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen = sizeof(rpmi_uint32_t);
	struct rpmi_dpwr_group *dpwrgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	struct rpmi_dpwr_attrs dpwr_attrs;
	enum rpmi_error ret;

	rpmi_uint32_t dpwrid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (dpwrid > dpwrgrp->dpwr_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = __rpmi_dpwr_get_attributes(dpwrgrp, dpwrid, &dpwr_attrs);
	if (ret) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	if (dpwr_attrs.name)
		rpmi_env_strncpy((char *)&resp[3], dpwr_attrs.name, RPMI_DPWR_NAME_MAX_LEN);

	resp[2] = rpmi_to_xe32(trans->is_be, dpwr_attrs.trans_latency);
	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)0x0);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 3 * sizeof(*resp) + RPMI_DPWR_NAME_MAX_LEN;

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_dpwr_get_state(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen = sizeof(rpmi_uint32_t);
	struct rpmi_dpwr_group *dpwrgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t dpwr_state = 0;
	enum rpmi_error ret;

	rpmi_uint32_t dpwrid = rpmi_to_xe32(trans->is_be,
					    ((const rpmi_uint32_t *)request_data)[0]);

	if (dpwrid > dpwrgrp->dpwr_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = __rpmi_dpwr_get_state(dpwrgrp, dpwrid, &dpwr_state);
	if (ret) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)dpwr_state);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 2 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_dpwr_set_state(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen = sizeof(rpmi_uint32_t);
	struct rpmi_dpwr_group *dpwrgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	enum rpmi_error ret;

	rpmi_uint32_t dpwrid = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[0]);
	rpmi_uint32_t dpwr_state = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[1]);

	if (dpwrid > dpwrgrp->dpwr_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = __rpmi_dpwr_set_state(dpwrgrp, dpwrid, dpwr_state);
	if (ret) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_dpwr_services[RPMI_DPWR_SRV_ID_MAX] = {
	[RPMI_DPWR_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_DPWR_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_DPWR_SRV_GET_NUM_DOMAINS] = {
		.service_id = RPMI_DPWR_SRV_GET_NUM_DOMAINS,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_dpwr_get_num_domains,
	},
	[RPMI_DPWR_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_DPWR_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_dpwr_get_attributes,
	},
	[RPMI_DPWR_SRV_SET_DPWR_STATE] = {
		.service_id = RPMI_DPWR_SRV_SET_DPWR_STATE,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_dpwr_set_state,
	},
	[RPMI_DPWR_SRV_GET_DPWR_STATE] = {
		.service_id = RPMI_DPWR_SRV_GET_DPWR_STATE,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_dpwr_get_state,
	},
};

struct rpmi_service_group *
rpmi_service_group_dpwr_create(rpmi_uint32_t dpwr_count,
			       const struct rpmi_dpwr_data *dpwr_tree_data,
			       const struct rpmi_dpwr_platform_ops *ops,
			       void *ops_priv)
{
	struct rpmi_dpwr_group *dpwrgrp;
	struct rpmi_service_group *group;

	/* All critical parameters should be non-NULL */
	if (!dpwr_count || !dpwr_tree_data || !ops) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate dpwr service group */
	dpwrgrp = rpmi_env_zalloc(sizeof(*dpwrgrp));
	if (!dpwrgrp) {
		DPRINTF("%s: failed to allocate dpwr service group instance\n",
			__func__);
		return NULL;
	}

	dpwrgrp->dpwr_tree = rpmi_dpwr_tree_init(dpwr_count,
						 dpwr_tree_data,
						 ops,
						 ops_priv);
	if (!dpwrgrp->dpwr_tree) {
		DPRINTF("%s: failed to initialize clock tree\n", __func__);
		rpmi_env_free(dpwrgrp);
		return NULL;
	}

	dpwrgrp->dpwr_count = dpwr_count;
	dpwrgrp->ops = ops;
	dpwrgrp->ops_priv = ops_priv;

	group = &dpwrgrp->group;
	group->name = "dpwr";
	group->servicegroup_id = RPMI_SRVGRP_DEVICE_POWER;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed for both M-mode and S-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK | RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_DPWR_SRV_ID_MAX;
	group->services = rpmi_dpwr_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = dpwrgrp;

	return group;
}

void rpmi_service_group_dpwr_destroy(struct rpmi_service_group *group)
{
	rpmi_uint32_t dpwrid;
	struct rpmi_dpwr_group *dpwrgrp;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	dpwrgrp = group->priv;

	for (dpwrid = 0; dpwrid < dpwrgrp->dpwr_count; dpwrid++)
		rpmi_env_free_lock(dpwrgrp->dpwr_tree[dpwrid].lock);

	rpmi_env_free(dpwrgrp->dpwr_tree);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}
