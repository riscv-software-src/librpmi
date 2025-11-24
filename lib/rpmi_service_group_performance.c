/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Shanghai StarFive Technology Co., Ltd.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "librpmi_internal_list.h"

#ifdef LIBRPMI_DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define RPMI_PERF_NAME_MAX_LEN	16

#define DOORBELL_REG_MASK	GENMASK(2, 1)
#define DOORBELL_SUPPORT_MASK	BIT(0)

/* notification's event IDs */
enum rpmi_perf_notification_event_ids {
	/* performance power change */
	RPMI_PERF_POWER_CHANGE = 1,
	/* performance limit change */
	RPMI_PERF_LIMIT_CHANGE = 2,
	/* performance level change */
	RPMI_PERF_LEVEL_CHANGE = 3,
	RPMI_PERF_EVENT_MAX_IDX,
};

/* A performance domain instance */
struct rpmi_perf {
	/* Lock to invoke the platform operations to
	 * protect this structure
	 */
	void *lock;
	/* Perf domain ID */
	rpmi_uint32_t id;
	/* Perf static attributes/data */
	const struct rpmi_perf_data *pdata;
};

/** RPMI Performance Service Group instance */
struct rpmi_perf_group {
	/* Total perf domains count */
	rpmi_uint32_t perf_count;
	/* Pointer to perf domains tree */
	struct rpmi_perf *perf_tree;
	/* Common perf platform operations (called with holding the lock)*/
	const struct rpmi_perf_platform_ops *ops;
	/* performance service group fast-channel address and size */
	struct rpmi_perf_fc_memory_region *fc_memory_region;
	/* Private data of platform perf operations */
	void *ops_priv;
	struct rpmi_service_group group;
};

/** Get a struct rpmi_perf instance pointer from perf id */
static inline struct rpmi_perf *
rpmi_get_perf(struct rpmi_perf_group *perf_group, rpmi_uint32_t perf_id)
{
	return &perf_group->perf_tree[perf_id];
}

static enum rpmi_error __rpmi_perf_get_attrs(struct rpmi_perf_group *perfgrp,
					     rpmi_uint32_t perfid,
					     struct rpmi_perf_attrs *attrs)
{
	struct rpmi_perf *perf;

	if (!attrs) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	perf = rpmi_get_perf(perfgrp, perfid);
	if (!perf) {
		DPRINTF("%s: perf instance with perfid-%u not found\n",
			__func__, perfid);
		return RPMI_ERR_INVALID_PARAM;
	}

	attrs->name = perf->pdata->name;
	attrs->capability = perf->pdata->perf_capabilities;
	attrs->trans_latency = perf->pdata->trans_latency;
	attrs->level_count = perf->pdata->perf_level_count;
	attrs->level_array = perf->pdata->perf_level_array;

	return RPMI_SUCCESS;
}

static enum rpmi_error __rpmi_perf_get_fc_attrs(struct rpmi_perf_group *perfgrp,
						rpmi_uint32_t perfid,
						rpmi_uint32_t srv_id,
						struct rpmi_perf_fc_attrs **fc_attrs)
{
	struct rpmi_perf *perf;

	perf = rpmi_get_perf(perfgrp, perfid);
	if (!perf) {
		DPRINTF("%s: perf instance with perfid-%u not found\n",
		__func__, perfid);
		return RPMI_ERR_INVALID_PARAM;
	}

	switch (srv_id) {
	case RPMI_PERF_SRV_GET_PERF_LEVEL:
		*fc_attrs = &(perf->pdata->fc_attrs_array[RPMI_PERF_FC_GET_LEVEL]);
		break;

	case RPMI_PERF_SRV_SET_PERF_LEVEL:
		*fc_attrs = &(perf->pdata->fc_attrs_array[RPMI_PERF_FC_SET_LEVEL]);
		break;

	case RPMI_PERF_SRV_GET_PERF_LIMIT:
		*fc_attrs = &(perf->pdata->fc_attrs_array[RPMI_PERF_FC_GET_LIMIT]);
		break;

	case RPMI_PERF_SRV_SET_PERF_LIMIT:
		*fc_attrs = &(perf->pdata->fc_attrs_array[RPMI_PERF_FC_SET_LIMIT]);
		break;

	default:
		DPRINTF("%s: invalid sevice id-%u, not support fast channel feature\n", __func__, srv_id);
		return RPMI_ERR_INVALID_PARAM;
	}

	return RPMI_SUCCESS;
}

static enum rpmi_error __rpmi_perf_get_level(struct rpmi_perf_group *perfgrp,
					     rpmi_uint32_t perfid,
					     rpmi_uint32_t *perf_level)
{
	enum rpmi_error ret;
	struct rpmi_perf *perf = rpmi_get_perf(perfgrp, perfid);
	if (!perf)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(perf->lock);

	ret = perfgrp->ops->get_level(perfgrp->ops_priv, perf->id, perf_level);

	rpmi_env_unlock(perf->lock);

	return ret;
}

static enum rpmi_error __rpmi_perf_set_level(struct rpmi_perf_group *perfgrp,
					     rpmi_uint32_t perfid,
					     rpmi_uint32_t perf_level)
{
	enum rpmi_error ret;
	struct rpmi_perf *perf = rpmi_get_perf(perfgrp, perfid);

	if (!perf)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(perf->lock);

	ret = perfgrp->ops->set_level(perfgrp->ops_priv, perf->id, perf_level);

	rpmi_env_unlock(perf->lock);

	return ret;
}

static enum rpmi_error __rpmi_perf_get_limit(struct rpmi_perf_group *perfgrp,
                                             rpmi_uint32_t perfid,
                                             rpmi_uint32_t *max_perf_limit,
                                             rpmi_uint32_t *min_perf_limit)
{
        enum rpmi_error ret;
        struct rpmi_perf *perf = rpmi_get_perf(perfgrp, perfid);

        if (!perf)
                return RPMI_ERR_INVALID_PARAM;

        rpmi_env_lock(perf->lock);

        ret = perfgrp->ops->get_limit(perfgrp->ops_priv, perf->id, max_perf_limit, min_perf_limit);

        rpmi_env_unlock(perf->lock);

        return ret;
}

static enum rpmi_error __rpmi_perf_set_limit(struct rpmi_perf_group *perfgrp,
					     rpmi_uint32_t perfid,
					     rpmi_uint32_t max_perf_limit,
					     rpmi_uint32_t min_perf_limit)
{
	enum rpmi_error ret;
	struct rpmi_perf *perf = rpmi_get_perf(perfgrp, perfid);

	if (!perf)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(perf->lock);

	ret = perfgrp->ops->set_limit(perfgrp->ops_priv, perf->id, max_perf_limit, min_perf_limit);

	rpmi_env_unlock(perf->lock);

	return ret;
}

/**
 * Initialize the perf tree from provided
 * static platform perf data.
 *
 * This function initializes the hierarchical structures
 * to represent the perf association in the platform.
 **/
static struct rpmi_perf *
rpmi_perf_tree_init(rpmi_uint32_t perf_count,
		    const struct rpmi_perf_data *perf_tree_data,
		    const struct rpmi_perf_platform_ops *ops,
		    void *ops_priv)
{
	rpmi_uint32_t perfid;
	struct rpmi_perf *perf;

	struct rpmi_perf *perf_tree =
		rpmi_env_zalloc(sizeof(struct rpmi_perf) * perf_count);
	if (!perf_tree)
		return NULL;

	/* initialize all perf domain instances */
	for (perfid = 0; perfid < perf_count; perfid++) {
		perf = &perf_tree[perfid];
		perf->id = perfid;
		perf->pdata = &perf_tree_data[perfid];
		perf->lock = rpmi_env_alloc_lock();
	}

	return perf_tree;
}

/*****************************************************************************
 * RPMI Performance Service Group Functions
 ****************************************************************************/
static enum rpmi_error
rpmi_perf_get_num_domains(struct rpmi_service_group *group,
			  struct rpmi_service *service,
			  struct rpmi_transport *trans,
			  rpmi_uint16_t request_datalen,
			  const rpmi_uint8_t *request_data,
			  rpmi_uint16_t *response_datalen,
			  rpmi_uint8_t *response_data)
{
	struct rpmi_perf_group *perfgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, perfgrp->perf_count);

	*response_datalen = 2 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_perf_get_attrs(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	struct rpmi_perf_attrs perf_attrs;
	struct rpmi_perf_group *perfgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t perfid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (perfid >= perfgrp->perf_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = __rpmi_perf_get_attrs(perfgrp, perfid, &perf_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[3] = rpmi_to_xe32(trans->is_be, perf_attrs.trans_latency);
	resp[2] = rpmi_to_xe32(trans->is_be, perf_attrs.level_count);
	resp[1] = rpmi_to_xe32(trans->is_be, perf_attrs.capability);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	if (perf_attrs.name)
		rpmi_env_strncpy((char *)&resp[4], perf_attrs.name, RPMI_PERF_NAME_MAX_LEN);

	 resp_dlen = 4 * sizeof(*resp) + RPMI_PERF_NAME_MAX_LEN;

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_perf_get_supp_levels(struct rpmi_service_group *group,
			  struct rpmi_service *service,
			  struct rpmi_transport *trans,
			  rpmi_uint16_t request_datalen,
			  const rpmi_uint8_t *request_data,
			  rpmi_uint16_t *response_datalen,
			  rpmi_uint8_t *response_data)
{
	enum rpmi_error ret;
	rpmi_uint32_t i;
	rpmi_uint32_t num_perf_level;
	rpmi_uint32_t resp_dlen = 0, perf_level_idx = 0;
	rpmi_uint32_t max_levels, remaining = 0, returned = 0;
	rpmi_uint32_t start;
	struct rpmi_perf_level *perf_level_array;
	struct rpmi_perf_attrs perf_attrs;
	struct rpmi_perf_group *perfgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t perfid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (perfid >= perfgrp->perf_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = __rpmi_perf_get_attrs(perfgrp, perfid, &perf_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	num_perf_level = perf_attrs.level_count;
	perf_level_array = perf_attrs.level_array;

	if (!num_perf_level || !perf_level_array) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_NOTSUPP);
		goto done;
	}

	perf_level_idx = rpmi_to_xe32(trans->is_be,
				      ((const rpmi_uint32_t *)request_data)[1]);

	/* max perf levels a rpmi message can accommodate */
	max_levels =
		(RPMI_MSG_DATA_SIZE(trans->slot_size) - (4 * sizeof(*resp))) /
					sizeof(struct rpmi_perf_level);

	if (perf_level_idx <= num_perf_level) {
		returned = max_levels < (num_perf_level - perf_level_idx) ?
			   max_levels : (num_perf_level - perf_level_idx);

		for (i = 0; i < returned; i++) {
			start = 4 + (i * 4);
			resp[start + 0] = rpmi_to_xe32(trans->is_be,
						(rpmi_uint32_t)perf_level_array[perf_level_idx + i].level_index);
			resp[start + 1] = rpmi_to_xe32(trans->is_be,
						(rpmi_uint32_t)perf_level_array[perf_level_idx + i].clock_freq);
			resp[start + 2] = rpmi_to_xe32(trans->is_be,
						(rpmi_uint32_t)perf_level_array[perf_level_idx + i].power_cost);
			resp[start + 3] = rpmi_to_xe32(trans->is_be,
						(rpmi_uint32_t)perf_level_array[perf_level_idx + i].transition_latency);
		}
	}
	else {
		returned = 0;
		remaining = num_perf_level;
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	remaining = num_perf_level - (perf_level_idx + returned);

	resp[3] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)returned);
	resp[2] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)remaining);
	/* No flags currently supported */
	resp[1] = rpmi_to_xe32(trans->is_be, 0);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = (4 * sizeof(*resp)) +
			(returned * sizeof(struct rpmi_perf_level));

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_perf_get_level(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	rpmi_uint32_t perf_level = 0;
	struct rpmi_perf_group *perfgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t perfid = rpmi_to_xe32(trans->is_be,
					    ((const rpmi_uint32_t *)request_data)[0]);

	if (perfid >= perfgrp->perf_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	ret = __rpmi_perf_get_level(perfgrp, perfid, &perf_level);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)perf_level);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 2 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_perf_set_level(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	struct rpmi_perf_group *perfgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t perfid = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[0]);
	rpmi_uint32_t perf_level = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[1]);

	if (perfid >= perfgrp->perf_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	ret = __rpmi_perf_set_level(perfgrp, perfid, perf_level);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_perf_get_limit(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	rpmi_uint32_t max_perf_limit = 0;
	rpmi_uint32_t min_perf_limit = 0;
	struct rpmi_perf_group *perfgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t perfid = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[0]);

	if (perfid >= perfgrp->perf_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	ret = __rpmi_perf_get_limit(perfgrp, perfid, &max_perf_limit, &min_perf_limit);
        if (ret) {
                resp_dlen = sizeof(*resp);
                resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
                goto done;
        }

	resp[2] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)min_perf_limit);
	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)max_perf_limit);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 3 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_perf_set_limit(struct rpmi_service_group *group,
		    struct rpmi_service *service,
		    struct rpmi_transport *trans,
		    rpmi_uint16_t request_datalen,
		    const rpmi_uint8_t *request_data,
		    rpmi_uint16_t *response_datalen,
		    rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	struct rpmi_perf_group *perfgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t perfid = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[0]);
	rpmi_uint32_t max_perf_limit = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[1]);
	rpmi_uint32_t min_perf_limit = rpmi_to_xe32(trans->is_be,
					((const rpmi_uint32_t *)request_data)[2]);

	if (perfid >= perfgrp->perf_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	ret = __rpmi_perf_set_limit(perfgrp, perfid, max_perf_limit, min_perf_limit);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_perf_get_fast_channel_region(struct rpmi_service_group *group,
				  struct rpmi_service *service,
				  struct rpmi_transport *trans,
				  rpmi_uint16_t request_datalen,
				  const rpmi_uint8_t *request_data,
				  rpmi_uint16_t *response_datalen,
				  rpmi_uint8_t *response_data)
{
	struct rpmi_perf_group *perfgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, perfgrp->fc_memory_region->addr_low);
	resp[2] = rpmi_to_xe32(trans->is_be, perfgrp->fc_memory_region->addr_high);
	resp[3] = rpmi_to_xe32(trans->is_be, perfgrp->fc_memory_region->size_low);
	resp[4] = rpmi_to_xe32(trans->is_be, perfgrp->fc_memory_region->size_high);

	*response_datalen = 5 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_perf_get_fast_channel_attrs(struct rpmi_service_group *group,
				 struct rpmi_service *service,
				 struct rpmi_transport *trans,
				 rpmi_uint16_t request_datalen,
				 const rpmi_uint8_t *request_data,
				 rpmi_uint16_t *response_datalen,
				 rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen = 0;
	enum rpmi_error ret;
	struct rpmi_perf_fc_attrs *perf_fc_attrs;
	struct rpmi_perf_group *perfgrp = group->priv;
	struct rpmi_perf_attrs perf_attrs;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t perfid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (perfid >= perfgrp->perf_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		ret = RPMI_ERR_INVALID_PARAM;
		goto done;
	}

	rpmi_uint32_t srvid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[1]);

	if (srvid > RPMI_PERF_SRV_GET_FAST_CHANNEL_ATTRIBUTES) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
					(rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		ret = RPMI_ERR_INVALID_PARAM;
		goto done;
	}

	ret = __rpmi_perf_get_attrs(perfgrp, perfid, &perf_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	ret = __rpmi_perf_get_fc_attrs(perfgrp, perfid, srvid, &perf_fc_attrs);

	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	if (perf_attrs.capability & RPMI_PERF_CAPABILITY_FAST_CHANNEL_SUPPORT) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
		resp[1] = rpmi_to_xe32(trans->is_be, perf_fc_attrs->flags);
		resp[2] = rpmi_to_xe32(trans->is_be, perf_fc_attrs->offset_phys_addr_low);
		resp[3] = rpmi_to_xe32(trans->is_be, perf_fc_attrs->offset_phys_addr_high);
		resp[4] = rpmi_to_xe32(trans->is_be, perf_fc_attrs->size);
		resp_dlen = 5 * sizeof(*resp);

		if (perf_fc_attrs->flags & RPMI_PERF_FST_CHN_DB_SUPP) {
			resp[5] = rpmi_to_xe32(trans->is_be, perf_fc_attrs->db_addr_low);
			resp[6] = rpmi_to_xe32(trans->is_be, perf_fc_attrs->db_addr_high);
			resp[7] = rpmi_to_xe32(trans->is_be, perf_fc_attrs->db_id);
			resp_dlen = 8 * sizeof(*resp);
		}
	}
	else {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_ERR_NOTSUPP);
	}

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_perf_services[RPMI_PERF_SRV_ID_MAX] = {
	[RPMI_PERF_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_PERF_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = NULL,
	},
	[RPMI_PERF_SRV_GET_NUM_DOMAINS] = {
		.service_id = RPMI_PERF_SRV_GET_NUM_DOMAINS,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_perf_get_num_domains,
	},
	[RPMI_PERF_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_PERF_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_perf_get_attrs,
	},
	[RPMI_PERF_SRV_GET_SUPPORTED_LEVELS] = {
		.service_id = RPMI_PERF_SRV_GET_SUPPORTED_LEVELS,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_perf_get_supp_levels,
	},
	[RPMI_PERF_SRV_GET_PERF_LEVEL] = {
		.service_id = RPMI_PERF_SRV_GET_PERF_LEVEL,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_perf_get_level,
	},
	[RPMI_PERF_SRV_SET_PERF_LEVEL] = {
		.service_id = RPMI_PERF_SRV_SET_PERF_LEVEL,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_perf_set_level,
	},
	[RPMI_PERF_SRV_GET_PERF_LIMIT] = {
		.service_id = RPMI_PERF_SRV_GET_PERF_LIMIT,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_perf_get_limit,
	},
	[RPMI_PERF_SRV_SET_PERF_LIMIT] = {
		.service_id = RPMI_PERF_SRV_SET_PERF_LIMIT,
		.min_a2p_request_datalen = 12,
		.process_a2p_request = rpmi_perf_set_limit,
	},
	[RPMI_PERF_SRV_GET_FAST_CHANNEL_REGION] = {
		.service_id = RPMI_PERF_SRV_GET_FAST_CHANNEL_REGION,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_perf_get_fast_channel_region,
	},
	[RPMI_PERF_SRV_GET_FAST_CHANNEL_ATTRIBUTES] = {
		.service_id = RPMI_PERF_SRV_GET_FAST_CHANNEL_ATTRIBUTES,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_perf_get_fast_channel_attrs,
	},
};

struct rpmi_service_group *
rpmi_service_group_perf_create(rpmi_uint32_t perf_count,
			       const struct rpmi_perf_data *perf_tree_data,
			       const struct rpmi_perf_platform_ops *ops,
			       const struct rpmi_perf_fc_memory_region *fc_mem_region,
			       void *ops_priv)
{
	struct rpmi_perf_group *perfgrp;
	struct rpmi_service_group *group;

	/* All critical parameters should be non-NULL */
	if (!perf_count || !perf_tree_data || !ops) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate perf service group */
	perfgrp = rpmi_env_zalloc(sizeof(*perfgrp));
	if (!perfgrp) {
		DPRINTF("%s: failed to allocate perf service group instance\n",
			__func__);
		return NULL;
	}

	perfgrp->fc_memory_region = rpmi_env_zalloc(sizeof(struct rpmi_perf_fc_memory_region));
	if (!perfgrp->fc_memory_region) {
		DPRINTF("%s: failed to allocate perf fastchannel region\n",
			__func__);
		rpmi_env_free(perfgrp);
		return NULL;
	}

	perfgrp->perf_tree = rpmi_perf_tree_init(perf_count,
						 perf_tree_data,
						 ops,
						 ops_priv);
	if (!perfgrp->perf_tree) {
		DPRINTF("%s: failed to initialize perf tree\n", __func__);
		rpmi_env_free(perfgrp->fc_memory_region);
		rpmi_env_free(perfgrp);
		return NULL;
	}

	perfgrp->perf_count = perf_count;
	perfgrp->ops = ops;
	perfgrp->ops_priv = ops_priv;

	perfgrp->fc_memory_region->addr_low  = fc_mem_region->addr_low;
	perfgrp->fc_memory_region->addr_high = fc_mem_region->addr_high;
	perfgrp->fc_memory_region->size_low  = fc_mem_region->size_low;
	perfgrp->fc_memory_region->size_high = fc_mem_region->size_high;

	group = &perfgrp->group;
	group->name = "perf";
	group->servicegroup_id = RPMI_SRVGRP_PERFORMANCE;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed for both M-mode and S-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK | RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_PERF_SRV_ID_MAX;
	group->services = rpmi_perf_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = perfgrp;

	return group;
}

void rpmi_service_group_perf_destroy(struct rpmi_service_group *group)
{
	rpmi_uint32_t perfid;
	struct rpmi_perf_group *perfgrp;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	perfgrp = group->priv;

	for (perfid = 0; perfid < perfgrp->perf_count; perfid++)
		rpmi_env_free_lock(perfgrp->perf_tree[perfid].lock);

	rpmi_env_free(perfgrp->perf_tree);
	rpmi_env_free(perfgrp->fc_memory_region);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}
