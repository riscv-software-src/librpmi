/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 SiFive, Inc.
 */

#include <librpmi.h>

/* Response field accessors (usable as lvalues) */
#define RPMI_RAS_RESP_STATUS(__r)		((__r)[0])
#define RPMI_RAS_RESP_FLAGS(__r)		((__r)[1])
#define RPMI_RAS_RESP_REMAINING(__r)		((__r)[2])
#define RPMI_RAS_RESP_RETURNED(__r)		((__r)[3])
#define RPMI_RAS_RESP_DATA_PTR(__r)		(&(__r)[4])
#define RPMI_RAS_RESP_ERR_SRC_ID(__r, __i)	((__r)[4 + (__i)])
/* GET_NUM_ERR_SRCS response: status + count (no paginated header) */
#define RPMI_RAS_RESP_NUM_ERR_SRCS(__r)		((__r)[1])

/* Fill all four fixed fields of a paginated success response in one call */
#define RPMI_RAS_PREPARE_RESPONSE(__t, __r, __flags, __remaining, __returned)	\
	do {									\
		RPMI_RAS_RESP_STATUS(__r)    = rpmi_to_xe32((__t)->is_be, RPMI_SUCCESS);	\
		RPMI_RAS_RESP_FLAGS(__r)     = rpmi_to_xe32((__t)->is_be, (__flags));	\
		RPMI_RAS_RESP_REMAINING(__r) = rpmi_to_xe32((__t)->is_be, (__remaining));\
		RPMI_RAS_RESP_RETURNED(__r)  = rpmi_to_xe32((__t)->is_be, (__returned));	\
	} while (0)

/* Request field accessors */
#define RPMI_RAS_REQ_NOTIF_STATE(__t, __d)	RPMI_REQ_U32(__t, __d, 1)
#define RPMI_RAS_REQ_START_INDEX(__t, __d)	RPMI_REQ_U32(__t, __d, 0)
#define RPMI_RAS_REQ_ERR_SRC_ID(__t, __d)	RPMI_REQ_U32(__t, __d, 0)
#define RPMI_RAS_REQ_BYTE_OFFSET(__t, __d)	RPMI_REQ_U32(__t, __d, 1)

/*
 * Fixed response header: status, flags, remaining, returned (4 x uint32_t).
 * Max IDs / bytes that fit in the variable payload of a paginated response.
 */
#define RPMI_RAS_MAX_ERR_SRCS_PER_RESP(__slot_size)	\
	((RPMI_MSG_DATA_SIZE(__slot_size) - (4 * sizeof(rpmi_uint32_t))) /	\
	 sizeof(rpmi_uint32_t))
#define RPMI_RAS_MAX_DESC_BYTES_PER_RESP(__slot_size)	\
	(RPMI_MSG_DATA_SIZE(__slot_size) - (4 * sizeof(rpmi_uint32_t)))

#ifdef LIBRPMI_DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct rpmi_ras_group {
	const struct rpmi_ras_platform_ops *ops;
	void *ops_priv;

	struct rpmi_service_group group;
};

static enum rpmi_error rpmi_ras_enable_notification(struct rpmi_service_group *group,
						    struct rpmi_service *service,
						    struct rpmi_transport *trans,
						    rpmi_uint16_t request_datalen,
						    const rpmi_uint8_t *request_data,
						    rpmi_uint16_t *response_datalen,
						    rpmi_uint8_t *response_data)
{
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t state;

	*response_datalen = sizeof(rpmi_uint32_t);

	state = RPMI_RAS_REQ_NOTIF_STATE(trans, request_data);
	if (state > 1) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, RPMI_ERR_INVALID_PARAM);
		return RPMI_SUCCESS;
	}

	RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, RPMI_ERR_NOTSUPP);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_ras_get_num_err_srcs(struct rpmi_service_group *group,
						 struct rpmi_service *service,
						 struct rpmi_transport *trans,
						 rpmi_uint16_t request_datalen,
						 const rpmi_uint8_t *request_data,
						 rpmi_uint16_t *response_datalen,
						 rpmi_uint8_t *response_data)
{
	struct rpmi_ras_group *rasgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t num_err_srcs;
	enum rpmi_error ret;

	ret = rasgrp->ops->get_num_error_sources(rasgrp->ops_priv, &num_err_srcs);
	if (ret != RPMI_SUCCESS) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, ret);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	RPMI_RAS_RESP_STATUS(resp)       = rpmi_to_xe32(trans->is_be, RPMI_SUCCESS);
	RPMI_RAS_RESP_NUM_ERR_SRCS(resp) = rpmi_to_xe32(trans->is_be, num_err_srcs);

	*response_datalen = 2 * sizeof(rpmi_uint32_t);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_ras_get_err_srcs_id_list(struct rpmi_service_group *group,
						     struct rpmi_service *service,
						     struct rpmi_transport *trans,
						     rpmi_uint16_t request_datalen,
						     const rpmi_uint8_t *request_data,
						     rpmi_uint16_t *response_datalen,
						     rpmi_uint8_t *response_data)
{
	struct rpmi_ras_group *rasgrp = group->priv;
	rpmi_uint32_t start_index, num_err_srcs, remaining, returned;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint16_t max_err_srcs;
	enum rpmi_error ret;

	start_index = RPMI_RAS_REQ_START_INDEX(trans, request_data);

	ret = rasgrp->ops->get_num_error_sources(rasgrp->ops_priv, &num_err_srcs);
	if (ret != RPMI_SUCCESS) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, ret);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	if (start_index >= num_err_srcs) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, RPMI_ERR_INVALID_PARAM);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	max_err_srcs = RPMI_RAS_MAX_ERR_SRCS_PER_RESP(trans->slot_size);
	remaining = num_err_srcs - start_index;
	returned = (remaining > max_err_srcs) ? max_err_srcs : remaining;

	ret = rasgrp->ops->get_err_src_id_list(rasgrp->ops_priv, start_index,
					       returned, RPMI_RAS_RESP_DATA_PTR(resp));
	if (ret != RPMI_SUCCESS) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, ret);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	for (rpmi_uint32_t i = 0; i < returned; i++)
		RPMI_RAS_RESP_ERR_SRC_ID(resp, i) =
			rpmi_to_xe32(trans->is_be, RPMI_RAS_RESP_ERR_SRC_ID(resp, i));

	RPMI_RAS_PREPARE_RESPONSE(trans, resp, 0, remaining - returned, returned);

	*response_datalen = (4 + returned) * sizeof(rpmi_uint32_t);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_ras_get_err_src_desc(struct rpmi_service_group *group,
						 struct rpmi_service *service,
						 struct rpmi_transport *trans,
						 rpmi_uint16_t request_datalen,
						 const rpmi_uint8_t *request_data,
						 rpmi_uint16_t *response_datalen,
						 rpmi_uint8_t *response_data)
{
	struct rpmi_ras_group *rasgrp = group->priv;
	struct rpmi_ras_err_src_desc desc;
	rpmi_uint32_t err_src_id, byte_offset, remaining, returned;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint8_t *desc_data = (rpmi_uint8_t *)RPMI_RAS_RESP_DATA_PTR(resp);
	rpmi_uint16_t max_desc_bytes;
	enum rpmi_error ret;

	err_src_id  = RPMI_RAS_REQ_ERR_SRC_ID(trans, request_data);
	byte_offset = RPMI_RAS_REQ_BYTE_OFFSET(trans, request_data);

	/* Get descriptor from platform; platform validates err_src_id */
	ret = rasgrp->ops->get_err_src_desc(rasgrp->ops_priv, err_src_id, &desc);
	if (ret != RPMI_SUCCESS) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, ret);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Validate descriptor format: only GHESv2 and implementation-specific are defined */
	if (desc.desc_format != RPMI_RAS_DESC_FORMAT_GHESV2 &&
	    desc.desc_format != RPMI_RAS_DESC_FORMAT_IMPL_SPECIFIC) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, RPMI_ERR_INVALID_PARAM);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	/* Reject non-zero descriptor size with NULL data pointer */
	if (desc.desc_size && !desc.desc_data) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, RPMI_ERR_INVALID_PARAM);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	if (byte_offset >= desc.desc_size) {
		RPMI_RAS_RESP_STATUS(resp) = rpmi_to_xe32(trans->is_be, RPMI_ERR_INVALID_PARAM);
		*response_datalen = sizeof(rpmi_uint32_t);
		return RPMI_SUCCESS;
	}

	max_desc_bytes = RPMI_RAS_MAX_DESC_BYTES_PER_RESP(trans->slot_size);
	remaining = desc.desc_size - byte_offset;
	returned = (remaining > max_desc_bytes) ? max_desc_bytes : remaining;

	/* FLAGS[3:0] = descriptor format */
	RPMI_RAS_PREPARE_RESPONSE(trans, resp, desc.desc_format,
				  remaining - returned, returned);

	/* Copy descriptor data */
	rpmi_env_memcpy(desc_data, desc.desc_data + byte_offset, returned);

	*response_datalen = (4 * sizeof(rpmi_uint32_t)) + returned;
	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_ras_services[RPMI_RAS_SRV_ID_MAX] = {
	[RPMI_RAS_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_RAS_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_ras_enable_notification,
	},
	[RPMI_RAS_SRV_GET_NUM_ERR_SRCS] = {
		.service_id = RPMI_RAS_SRV_GET_NUM_ERR_SRCS,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_ras_get_num_err_srcs,
	},
	[RPMI_RAS_SRV_GET_ERR_SRCS_ID_LIST] = {
		.service_id = RPMI_RAS_SRV_GET_ERR_SRCS_ID_LIST,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_ras_get_err_srcs_id_list,
	},
	[RPMI_RAS_SRV_GET_ERR_SRC_DESC] = {
		.service_id = RPMI_RAS_SRV_GET_ERR_SRC_DESC,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_ras_get_err_src_desc,
	},
};

struct rpmi_service_group *
rpmi_service_group_ras_create(const struct rpmi_ras_platform_ops *ops,
			      void *ops_priv)
{
	struct rpmi_ras_group *rasgrp;
	struct rpmi_service_group *group;
	enum rpmi_error ret;

	if (!ops || !ops->get_num_error_sources ||
	    !ops->get_err_src_id_list || !ops->get_err_src_desc) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate RAS agent group */
	rasgrp = rpmi_env_zalloc(sizeof(*rasgrp));
	if (!rasgrp) {
		DPRINTF("%s: failed to allocate RAS agent service group instance\n",
			__func__);
		return NULL;
	}

	rasgrp->ops = ops;
	rasgrp->ops_priv = ops_priv;

	if (ops->init) {
		ret = ops->init(ops_priv);
		if (ret != RPMI_SUCCESS) {
			DPRINTF("%s: platform init failed\n", __func__);
			rpmi_env_free(rasgrp);
			return NULL;
		}
	}

	group = &rasgrp->group;
	group->name = "ras_agent";
	group->servicegroup_id = RPMI_SRVGRP_RAS_AGENT;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK |
					RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_RAS_SRV_ID_MAX;
	group->services = rpmi_ras_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = rasgrp;

	return group;
}

void rpmi_service_group_ras_destroy(struct rpmi_service_group *group)
{
	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}
