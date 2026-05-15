// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Qualcomm Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_DPWR_COUNT		2
#define TEST_DPWR_GPU		0
#define TEST_DPWR_NPU		1
#define TEST_DPWR_INVALID	TEST_DPWR_COUNT
#define TEST_DPWR_GPU_LATENCY	15
#define TEST_DPWR_BAD_STATE	1
#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1

struct test_dpwr_attrs_expdata {
	rpmi_uint32_t status;
	rpmi_uint32_t flags;
	rpmi_uint32_t trans_latency;
	char name[16];
};

static struct rpmi_dpwr_data test_dpwr_data[TEST_DPWR_COUNT] = {
	[TEST_DPWR_GPU] = {
		.trans_latency = TEST_DPWR_GPU_LATENCY,
		.name = "gpu_pd",
	},
	[TEST_DPWR_NPU] = {
		.trans_latency = 25,
		.name = "npu_pd",
	},
};

static rpmi_uint32_t test_dpwr_state[TEST_DPWR_COUNT] = {
	RPMI_DPWR_STATE_ON,
	RPMI_DPWR_STATE_OFF,
};

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE,
};

static rpmi_uint32_t notsupp_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t get_num_domains_expdata[] = {
	RPMI_SUCCESS,
	TEST_DPWR_COUNT,
};

static rpmi_uint32_t dpwr_gpu_reqdata[] = {
	TEST_DPWR_GPU,
};

static rpmi_uint32_t dpwr_invalid_reqdata[] = {
	TEST_DPWR_INVALID,
};

static struct test_dpwr_attrs_expdata get_attrs_gpu_expdata = {
	.status = RPMI_SUCCESS,
	.flags = 0,
	.trans_latency = TEST_DPWR_GPU_LATENCY,
	.name = "gpu_pd",
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t set_state_off_reqdata[] = {
	TEST_DPWR_GPU,
	RPMI_DPWR_STATE_OFF,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t get_state_off_expdata[] = {
	RPMI_SUCCESS,
	RPMI_DPWR_STATE_OFF,
};

static rpmi_uint32_t set_state_bad_reqdata[] = {
	TEST_DPWR_GPU,
	TEST_DPWR_BAD_STATE,
};

static rpmi_uint8_t get_state_short_reqdata[] = {
	TEST_DPWR_GPU,
};

static enum rpmi_error test_dpwr_get_state(void *priv,
					  rpmi_uint32_t dpwr_id,
					  rpmi_uint32_t *state)
{
	if (dpwr_id >= TEST_DPWR_COUNT || !state)
		return RPMI_ERR_INVALID_PARAM;

	*state = test_dpwr_state[dpwr_id];
	return RPMI_SUCCESS;
}

static enum rpmi_error test_dpwr_set_state(void *priv,
					  rpmi_uint32_t dpwr_id,
					  rpmi_uint32_t state)
{
	if (dpwr_id >= TEST_DPWR_COUNT)
		return RPMI_ERR_INVALID_PARAM;

	if (state != RPMI_DPWR_STATE_ON && state != RPMI_DPWR_STATE_OFF)
		return RPMI_ERR_INVALID_PARAM;

	test_dpwr_state[dpwr_id] = state;
	return RPMI_SUCCESS;
}

static struct rpmi_dpwr_platform_ops test_dpwr_ops = {
	.get_state = test_dpwr_get_state,
	.set_state = test_dpwr_set_state,
};

static int test_dpwr_state_init(struct rpmi_test_scenario *scene,
					struct rpmi_test *test)
{
	test_dpwr_state[TEST_DPWR_GPU] = RPMI_DPWR_STATE_ON;
	test_dpwr_state[TEST_DPWR_NPU] = RPMI_DPWR_STATE_OFF;
	return 0;
}

static int test_dpwr_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	test_dpwr_state[TEST_DPWR_GPU] = RPMI_DPWR_STATE_ON;
	test_dpwr_state[TEST_DPWR_NPU] = RPMI_DPWR_STATE_OFF;

	grp = rpmi_service_group_dpwr_create(TEST_DPWR_COUNT,
						test_dpwr_data,
						&test_dpwr_ops, NULL);
	if (!grp) {
		printf("failed to create rpmi dpwr service group");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_dpwr_default = {
	.name = "Device Power Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_dpwr_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 9,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = enable_notif_reqdata,
				.request_data_len = sizeof(enable_notif_reqdata),
				.expected_data = notsupp_expdata,
				.expected_data_len = sizeof(notsupp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET NUM DOMAINS",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_GET_NUM_DOMAINS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = get_num_domains_expdata,
				.expected_data_len = sizeof(get_num_domains_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = dpwr_gpu_reqdata,
				.request_data_len = sizeof(dpwr_gpu_reqdata),
				.expected_data = &get_attrs_gpu_expdata,
				.expected_data_len = sizeof(get_attrs_gpu_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES (invalid domain)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = dpwr_invalid_reqdata,
				.request_data_len = sizeof(dpwr_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET STATE (off)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_SET_DPWR_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_state_off_reqdata,
				.request_data_len = sizeof(set_state_off_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init = test_dpwr_state_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET STATE (off)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_GET_DPWR_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = dpwr_gpu_reqdata,
				.request_data_len = sizeof(dpwr_gpu_reqdata),
				.expected_data = get_state_off_expdata,
				.expected_data_len = sizeof(get_state_off_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET STATE (unsupported state)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_SET_DPWR_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_state_bad_reqdata,
				.request_data_len = sizeof(set_state_bad_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET STATE (invalid domain)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_GET_DPWR_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = dpwr_invalid_reqdata,
				.request_data_len = sizeof(dpwr_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET STATE (truncated domain id)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_DEVICE_POWER,
				.service_id = RPMI_DPWR_SRV_GET_DPWR_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_state_short_reqdata,
				.request_data_len = sizeof(get_state_short_reqdata),
				.expected_data = notsupp_expdata,
				.expected_data_len = sizeof(notsupp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test Device Power Service Group\n");
	return test_scenario_execute(&scenario_dpwr_default);
}
