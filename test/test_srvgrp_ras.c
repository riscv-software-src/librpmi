/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 SiFive, Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_EVENT_ID			0x0
#define TEST_REQUEST_STATE_ENABLE	0x1

/* Test error source IDs */
#define TEST_RAS_ERR_SRC_ID_0		0x100
#define TEST_RAS_ERR_SRC_ID_1		0x101
#define TEST_RAS_ERR_SRC_ID_2		0x102
#define TEST_RAS_ERR_SRC_INVALID	0x999

static const rpmi_uint32_t test_ras_err_src_ids[] = {
	TEST_RAS_ERR_SRC_ID_0,
	TEST_RAS_ERR_SRC_ID_1,
	TEST_RAS_ERR_SRC_ID_2,
};

/* Mock GHESv2 descriptor (simplified for testing) */
static rpmi_uint8_t test_ghesv2_desc[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
};

/* Test data for ENABLE_NOTIFICATION */
static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE
};

static rpmi_uint32_t enable_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

/* Test data for GET_NUM_ERR_SRCS */
static rpmi_uint32_t get_num_err_srcs_expdata[] = {
	RPMI_SUCCESS,
	3, /* number of error sources */
};

/* Test data for GET_ERR_SRCS_ID_LIST */
static rpmi_uint32_t get_err_srcs_list_reqdata_start0[] = {
	0, /* start_index */
};

static rpmi_uint32_t get_err_srcs_list_expdata_start0[] = {
	RPMI_SUCCESS,
	0, /* FLAGS */
	0, /* REMAINING */
	3, /* RETURNED */
	TEST_RAS_ERR_SRC_ID_0,
	TEST_RAS_ERR_SRC_ID_1,
	TEST_RAS_ERR_SRC_ID_2,
};

static rpmi_uint32_t get_err_srcs_list_reqdata_invalid[] = {
	100, /* invalid start_index */
};

static rpmi_uint32_t get_err_srcs_list_expdata_invalid[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t get_err_srcs_list_reqdata_start1[] = {
	1, /* start_index */
};

static rpmi_uint32_t get_err_srcs_list_expdata_start1[] = {
	RPMI_SUCCESS,
	0, /* FLAGS */
	0, /* REMAINING */
	2, /* RETURNED */
	TEST_RAS_ERR_SRC_ID_1,
	TEST_RAS_ERR_SRC_ID_2,
};

/* Test data for GET_ERR_SRC_DESC */
static rpmi_uint32_t get_err_src_desc_reqdata_valid[] = {
	TEST_RAS_ERR_SRC_ID_0,
	0, /* byte_offset */
};

static rpmi_uint32_t get_err_src_desc_reqdata_invalid_id[] = {
	TEST_RAS_ERR_SRC_INVALID,
	0, /* byte_offset */
};

static rpmi_uint32_t get_err_src_desc_expdata_invalid_id[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t get_err_src_desc_reqdata_invalid_offset[] = {
	TEST_RAS_ERR_SRC_ID_0,
	1000, /* invalid byte_offset */
};

static rpmi_uint32_t get_err_src_desc_expdata_invalid_offset[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t get_err_src_desc_reqdata_offset[] = {
	TEST_RAS_ERR_SRC_ID_0,
	8, /* byte_offset */
};

/* test_ghesv2_desc[8..31] packed as LE uint32 words */
static rpmi_uint32_t get_err_src_desc_expdata_offset[] = {
	RPMI_SUCCESS,
	RPMI_RAS_DESC_FORMAT_GHESV2,
	0,  /* REMAINING = 0 */
	24, /* RETURNED = 24 bytes */
	0x0b0a0908, 0x0f0e0d0c,
	0x13121110, 0x17161514,
	0x1b1a1918, 0x1f1e1d1c,
};

/* Expected data for GET_ERR_SRC_DESC with partial descriptor */
static rpmi_uint32_t get_err_src_desc_expdata_valid[] = {
	RPMI_SUCCESS,
	RPMI_RAS_DESC_FORMAT_GHESV2, /* FLAGS[3:0] = format */
	0, /* REMAINING = 0 (all data returned) */
	32, /* RETURNED = 32 bytes */
	/* First 32 bytes of test_ghesv2_desc */
	0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c,
	0x13121110, 0x17161514, 0x1b1a1918, 0x1f1e1d1c,
};

static enum rpmi_error test_ras_get_num_error_sources(void *priv,
						      rpmi_uint32_t *num_err_srcs)
{
	*num_err_srcs = sizeof(test_ras_err_src_ids) / sizeof(rpmi_uint32_t);
	return RPMI_SUCCESS;
}

static enum rpmi_error test_ras_get_err_src_id_list(void *priv,
						    rpmi_uint32_t start_index,
						    rpmi_uint32_t count,
						    rpmi_uint32_t *ids)
{
	rpmi_uint32_t i;

	for (i = 0; i < count; i++)
		ids[i] = test_ras_err_src_ids[start_index + i];

	return RPMI_SUCCESS;
}

static enum rpmi_error test_ras_get_err_src_desc(void *priv,
						 rpmi_uint32_t err_src_id,
						 struct rpmi_ras_err_src_desc *desc)
{
	if (!desc)
		return RPMI_ERR_INVALID_PARAM;

	/* Only support the test error source IDs */
	if (err_src_id != TEST_RAS_ERR_SRC_ID_0 &&
	    err_src_id != TEST_RAS_ERR_SRC_ID_1 &&
	    err_src_id != TEST_RAS_ERR_SRC_ID_2)
		return RPMI_ERR_INVALID_PARAM;

	desc->err_src_id = err_src_id;
	desc->desc_format = RPMI_RAS_DESC_FORMAT_GHESV2;
	desc->desc_size = sizeof(test_ghesv2_desc);
	desc->desc_data = test_ghesv2_desc;

	printf("platform callback: returning descriptor for error source ID 0x%x (format=%d, size=%d bytes)\n",
	       err_src_id, desc->desc_format, desc->desc_size);

	return RPMI_SUCCESS;
}

static struct rpmi_ras_platform_ops test_ras_ops = {
	.get_num_error_sources = test_ras_get_num_error_sources,
	.get_err_src_id_list = test_ras_get_err_src_id_list,
	.get_err_src_desc = test_ras_get_err_src_desc,
};

static int test_ras_scenario_init(struct rpmi_test_scenario *scene)
{
	int ret;
	struct rpmi_service_group *grp;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	grp = rpmi_service_group_ras_create(&test_ras_ops, NULL);
	if (!grp) {
		printf("failed to create RAS agent service group\n");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_ras_default = {
	.name = "RAS Agent Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_ras_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 9,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = enable_notif_reqdata,
				.request_data_len = sizeof(enable_notif_reqdata),
				.expected_data = enable_notif_expdata,
				.expected_data_len = sizeof(enable_notif_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET NUM ERR SRCS TEST",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_GET_NUM_ERR_SRCS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = get_num_err_srcs_expdata,
				.expected_data_len = sizeof(get_num_err_srcs_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ERR SRCS ID LIST TEST (start_index=0)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_GET_ERR_SRCS_ID_LIST,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_err_srcs_list_reqdata_start0,
				.request_data_len = sizeof(get_err_srcs_list_reqdata_start0),
				.expected_data = get_err_srcs_list_expdata_start0,
				.expected_data_len = sizeof(get_err_srcs_list_expdata_start0),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ERR SRCS ID LIST TEST (start_index=1)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_GET_ERR_SRCS_ID_LIST,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_err_srcs_list_reqdata_start1,
				.request_data_len = sizeof(get_err_srcs_list_reqdata_start1),
				.expected_data = get_err_srcs_list_expdata_start1,
				.expected_data_len = sizeof(get_err_srcs_list_expdata_start1),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ERR SRCS ID LIST TEST (invalid start_index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_GET_ERR_SRCS_ID_LIST,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_err_srcs_list_reqdata_invalid,
				.request_data_len = sizeof(get_err_srcs_list_reqdata_invalid),
				.expected_data = get_err_srcs_list_expdata_invalid,
				.expected_data_len = sizeof(get_err_srcs_list_expdata_invalid),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ERR SRC DESC TEST (valid error source)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_GET_ERR_SRC_DESC,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_err_src_desc_reqdata_valid,
				.request_data_len = sizeof(get_err_src_desc_reqdata_valid),
				.expected_data = get_err_src_desc_expdata_valid,
				.expected_data_len = sizeof(get_err_src_desc_expdata_valid),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ERR SRC DESC TEST (invalid error source ID)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_GET_ERR_SRC_DESC,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_err_src_desc_reqdata_invalid_id,
				.request_data_len = sizeof(get_err_src_desc_reqdata_invalid_id),
				.expected_data = get_err_src_desc_expdata_invalid_id,
				.expected_data_len = sizeof(get_err_src_desc_expdata_invalid_id),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ERR SRC DESC TEST (non-zero byte offset)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_GET_ERR_SRC_DESC,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_err_src_desc_reqdata_offset,
				.request_data_len = sizeof(get_err_src_desc_reqdata_offset),
				.expected_data = get_err_src_desc_expdata_offset,
				.expected_data_len = sizeof(get_err_src_desc_expdata_offset),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ERR SRC DESC TEST (invalid byte offset)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_RAS_AGENT,
				.service_id = RPMI_RAS_SRV_GET_ERR_SRC_DESC,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_err_src_desc_reqdata_invalid_offset,
				.request_data_len = sizeof(get_err_src_desc_reqdata_invalid_offset),
				.expected_data = get_err_src_desc_expdata_invalid_offset,
				.expected_data_len = sizeof(get_err_src_desc_expdata_invalid_offset),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test RAS Agent Service Group\n");
	return test_scenario_execute(&scenario_ras_default);
}
