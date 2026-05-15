// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_PERF_COUNT		2
#define TEST_PERF_CPU		0
#define TEST_PERF_GPU		1
#define TEST_PERF_INVALID	TEST_PERF_COUNT
#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_PERF_CPU_LATENCY	11
#define TEST_PERF_TARGET_LEVEL	30
#define TEST_PERF_BAD_LEVEL	99
#define TEST_FC_ADDR_LOW	0x20000000
#define TEST_FC_ADDR_HIGH	0x0
#define TEST_FC_SIZE_LOW	0x1000
#define TEST_FC_SIZE_HIGH	0x0
#define TEST_FC_DB_ADDR_LOW	0x30000000
#define TEST_FC_DB_ADDR_HIGH	0x0
#define TEST_FC_DB_ID		7

struct test_perf_attrs_expdata {
	rpmi_uint32_t status;
	rpmi_uint32_t capability;
	rpmi_uint32_t level_count;
	rpmi_uint32_t trans_latency;
	char name[16];
};

static struct rpmi_perf_level test_cpu_levels[] = {
	{ .level_index = 10, .clock_freq = 1000, .power_cost = 10, .transition_latency = 1 },
	{ .level_index = 20, .clock_freq = 1500, .power_cost = 20, .transition_latency = 2 },
	{ .level_index = 30, .clock_freq = 2000, .power_cost = 30, .transition_latency = 3 },
};

static struct rpmi_perf_level test_gpu_levels[] = {
	{ .level_index = 1, .clock_freq = 500, .power_cost = 5, .transition_latency = 4 },
	{ .level_index = 2, .clock_freq = 700, .power_cost = 7, .transition_latency = 5 },
};

static struct rpmi_perf_fc_attrs test_cpu_fc_attrs[RPMI_PERF_FC_MAX] = {
	[RPMI_PERF_FC_GET_LEVEL] = {
		.flags = RPMI_PERF_FST_CHN_DB_NOT_SUPP,
		.offset_phys_addr_low = 0,
		.offset_phys_addr_high = 0,
		.size = sizeof(rpmi_uint32_t),
	},
	[RPMI_PERF_FC_SET_LEVEL] = {
		.flags = RPMI_PERF_FST_CHN_DB_SUPP | RPMI_PERF_FST_CHN_DB_REG_32_BITS,
		.offset_phys_addr_low = 4,
		.offset_phys_addr_high = 0,
		.size = sizeof(rpmi_uint32_t),
		.db_addr_low = TEST_FC_DB_ADDR_LOW,
		.db_addr_high = TEST_FC_DB_ADDR_HIGH,
		.db_id = TEST_FC_DB_ID,
	},
	[RPMI_PERF_FC_GET_LIMIT] = {
		.flags = RPMI_PERF_FST_CHN_DB_NOT_SUPP,
		.offset_phys_addr_low = 8,
		.offset_phys_addr_high = 0,
		.size = 2 * sizeof(rpmi_uint32_t),
	},
	[RPMI_PERF_FC_SET_LIMIT] = {
		.flags = RPMI_PERF_FST_CHN_DB_NOT_SUPP,
		.offset_phys_addr_low = 16,
		.offset_phys_addr_high = 0,
		.size = 2 * sizeof(rpmi_uint32_t),
	},
};

static struct rpmi_perf_fc_attrs test_gpu_fc_attrs[RPMI_PERF_FC_MAX];

static struct rpmi_perf_data test_perf_data[TEST_PERF_COUNT] = {
	[TEST_PERF_CPU] = {
		.name = "perf_cpu",
		.trans_latency = TEST_PERF_CPU_LATENCY,
		.perf_capabilities = RPMI_PERF_CAPABILITY_SET_LIMIT |
				     RPMI_PERF_CAPABILITY_SET_LEVEL |
				     RPMI_PERF_CAPABILITY_FAST_CHANNEL_SUPPORT,
		.perf_level_count = ARRAY_SIZE(test_cpu_levels),
		.perf_level_array = test_cpu_levels,
		.fc_attrs_array = test_cpu_fc_attrs,
	},
	[TEST_PERF_GPU] = {
		.name = "perf_gpu",
		.trans_latency = 13,
		.perf_capabilities = RPMI_PERF_CAPABILITY_SET_LEVEL,
		.perf_level_count = ARRAY_SIZE(test_gpu_levels),
		.perf_level_array = test_gpu_levels,
		.fc_attrs_array = test_gpu_fc_attrs,
	},
};

static struct rpmi_perf_fc_memory_region test_fc_region = {
	.addr_low = TEST_FC_ADDR_LOW,
	.addr_high = TEST_FC_ADDR_HIGH,
	.size_low = TEST_FC_SIZE_LOW,
	.size_high = TEST_FC_SIZE_HIGH,
};

static rpmi_uint32_t test_perf_level[TEST_PERF_COUNT] = {
	10,
	1,
};

static rpmi_uint32_t test_perf_limit_max[TEST_PERF_COUNT] = {
	30,
	2,
};

static rpmi_uint32_t test_perf_limit_min[TEST_PERF_COUNT] = {
	10,
	1,
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
	TEST_PERF_COUNT,
};

static rpmi_uint32_t perf_cpu_reqdata[] = {
	TEST_PERF_CPU,
};

static rpmi_uint32_t perf_invalid_reqdata[] = {
	TEST_PERF_INVALID,
};

static struct test_perf_attrs_expdata get_attrs_cpu_expdata = {
	.status = RPMI_SUCCESS,
	.capability = RPMI_PERF_CAPABILITY_SET_LIMIT |
		      RPMI_PERF_CAPABILITY_SET_LEVEL |
		      RPMI_PERF_CAPABILITY_FAST_CHANNEL_SUPPORT,
	.level_count = ARRAY_SIZE(test_cpu_levels),
	.trans_latency = TEST_PERF_CPU_LATENCY,
	.name = "perf_cpu",
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t get_levels_reqdata[] = {
	TEST_PERF_CPU,
	1,
};

static rpmi_uint32_t get_levels_expdata[] = {
	RPMI_SUCCESS,
	0,
	0,
	2,
	20,
	1500,
	20,
	2,
	30,
	2000,
	30,
	3,
};

static rpmi_uint32_t get_levels_bad_index_reqdata[] = {
	TEST_PERF_CPU,
	4,
};

static rpmi_uint32_t set_level_reqdata[] = {
	TEST_PERF_CPU,
	TEST_PERF_TARGET_LEVEL,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t get_level_expdata[] = {
	RPMI_SUCCESS,
	TEST_PERF_TARGET_LEVEL,
};

static rpmi_uint32_t set_level_bad_reqdata[] = {
	TEST_PERF_CPU,
	TEST_PERF_BAD_LEVEL,
};

static rpmi_uint32_t set_limit_reqdata[] = {
	TEST_PERF_CPU,
	30,
	10,
};

static rpmi_uint32_t get_limit_expdata[] = {
	RPMI_SUCCESS,
	30,
	10,
};

static rpmi_uint32_t set_limit_bad_reqdata[] = {
	TEST_PERF_CPU,
	10,
	30,
};

static rpmi_uint32_t get_fc_region_expdata[] = {
	RPMI_SUCCESS,
	TEST_FC_ADDR_LOW,
	TEST_FC_ADDR_HIGH,
	TEST_FC_SIZE_LOW,
	TEST_FC_SIZE_HIGH,
};

static rpmi_uint32_t get_fc_attrs_reqdata[] = {
	TEST_PERF_CPU,
	RPMI_PERF_SRV_GET_PERF_LEVEL,
};

static rpmi_uint32_t get_fc_attrs_expdata[] = {
	RPMI_SUCCESS,
	RPMI_PERF_FST_CHN_DB_NOT_SUPP,
	0,
	0,
	sizeof(rpmi_uint32_t),
};

static rpmi_uint32_t get_fc_attrs_unsupported_reqdata[] = {
	TEST_PERF_GPU,
	RPMI_PERF_SRV_GET_PERF_LEVEL,
};

static rpmi_bool_t test_perf_level_supported(rpmi_uint32_t perf_id,
						    rpmi_uint32_t level)
{
	struct rpmi_perf_level *levels;
	rpmi_uint32_t count, i;

	if (perf_id >= TEST_PERF_COUNT)
		return false;

	if (perf_id == TEST_PERF_CPU) {
		levels = test_cpu_levels;
		count = ARRAY_SIZE(test_cpu_levels);
	} else {
		levels = test_gpu_levels;
		count = ARRAY_SIZE(test_gpu_levels);
	}

	for (i = 0; i < count; i++) {
		if (levels[i].level_index == level)
			return true;
	}

	return false;
}

static enum rpmi_error test_perf_get_level(void *priv,
					  rpmi_uint32_t perf_id,
					  rpmi_uint32_t *level)
{
	if (perf_id >= TEST_PERF_COUNT || !level)
		return RPMI_ERR_INVALID_PARAM;

	*level = test_perf_level[perf_id];
	return RPMI_SUCCESS;
}

static enum rpmi_error test_perf_set_level(void *priv,
					  rpmi_uint32_t perf_id,
					  rpmi_uint32_t perf_level)
{
	if (perf_id >= TEST_PERF_COUNT)
		return RPMI_ERR_INVALID_PARAM;

	if (!test_perf_level_supported(perf_id, perf_level))
		return RPMI_ERR_INVALID_PARAM;

	test_perf_level[perf_id] = perf_level;
	return RPMI_SUCCESS;
}

static enum rpmi_error test_perf_get_limit(void *priv,
					  rpmi_uint32_t perf_id,
					  rpmi_uint32_t *max_perf_limit,
					  rpmi_uint32_t *min_perf_limit)
{
	if (perf_id >= TEST_PERF_COUNT || !max_perf_limit || !min_perf_limit)
		return RPMI_ERR_INVALID_PARAM;

	*max_perf_limit = test_perf_limit_max[perf_id];
	*min_perf_limit = test_perf_limit_min[perf_id];
	return RPMI_SUCCESS;
}

static enum rpmi_error test_perf_set_limit(void *priv,
					  rpmi_uint32_t perf_id,
					  rpmi_uint32_t max_perf_limit,
					  rpmi_uint32_t min_perf_limit)
{
	if (perf_id >= TEST_PERF_COUNT)
		return RPMI_ERR_INVALID_PARAM;

	if (min_perf_limit > max_perf_limit)
		return RPMI_ERR_INVALID_PARAM;

	if (!test_perf_level_supported(perf_id, max_perf_limit) ||
	    !test_perf_level_supported(perf_id, min_perf_limit))
		return RPMI_ERR_INVALID_PARAM;

	test_perf_limit_max[perf_id] = max_perf_limit;
	test_perf_limit_min[perf_id] = min_perf_limit;
	return RPMI_SUCCESS;
}

static struct rpmi_perf_platform_ops test_perf_ops = {
	.get_level = test_perf_get_level,
	.set_level = test_perf_set_level,
	.get_limit = test_perf_get_limit,
	.set_limit = test_perf_set_limit,
};

static int test_perf_state_init(struct rpmi_test_scenario *scene,
					struct rpmi_test *test)
{
	test_perf_level[TEST_PERF_CPU] = 10;
	test_perf_level[TEST_PERF_GPU] = 1;
	test_perf_limit_max[TEST_PERF_CPU] = 30;
	test_perf_limit_min[TEST_PERF_CPU] = 10;
	test_perf_limit_max[TEST_PERF_GPU] = 2;
	test_perf_limit_min[TEST_PERF_GPU] = 1;
	return 0;
}

static int test_perf_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	test_perf_state_init(scene, NULL);

	grp = rpmi_service_group_perf_create(TEST_PERF_COUNT,
						test_perf_data,
						&test_perf_ops,
						&test_fc_region, NULL);
	if (!grp) {
		printf("failed to create rpmi perf service group");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_perf_default = {
	.name = "Performance Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_perf_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 17,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_ENABLE_NOTIFICATION,
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
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_NUM_DOMAINS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = get_num_domains_expdata,
				.expected_data_len = sizeof(get_num_domains_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = perf_cpu_reqdata,
				.request_data_len = sizeof(perf_cpu_reqdata),
				.expected_data = &get_attrs_cpu_expdata,
				.expected_data_len = sizeof(get_attrs_cpu_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES (invalid domain)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = perf_invalid_reqdata,
				.request_data_len = sizeof(perf_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET SUPPORTED LEVELS",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_SUPPORTED_LEVELS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_levels_reqdata,
				.request_data_len = sizeof(get_levels_reqdata),
				.expected_data = get_levels_expdata,
				.expected_data_len = sizeof(get_levels_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET SUPPORTED LEVELS (bad index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_SUPPORTED_LEVELS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_levels_bad_index_reqdata,
				.request_data_len = sizeof(get_levels_bad_index_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET PERF LEVEL",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_SET_PERF_LEVEL,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_level_reqdata,
				.request_data_len = sizeof(set_level_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init = test_perf_state_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET PERF LEVEL",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_PERF_LEVEL,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = perf_cpu_reqdata,
				.request_data_len = sizeof(perf_cpu_reqdata),
				.expected_data = get_level_expdata,
				.expected_data_len = sizeof(get_level_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET PERF LEVEL (unsupported level)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_SET_PERF_LEVEL,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_level_bad_reqdata,
				.request_data_len = sizeof(set_level_bad_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET PERF LEVEL (invalid domain)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_PERF_LEVEL,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = perf_invalid_reqdata,
				.request_data_len = sizeof(perf_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET PERF LIMIT",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_SET_PERF_LIMIT,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_limit_reqdata,
				.request_data_len = sizeof(set_limit_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET PERF LIMIT",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_PERF_LIMIT,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = perf_cpu_reqdata,
				.request_data_len = sizeof(perf_cpu_reqdata),
				.expected_data = get_limit_expdata,
				.expected_data_len = sizeof(get_limit_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET PERF LIMIT (invalid range)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_SET_PERF_LIMIT,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_limit_bad_reqdata,
				.request_data_len = sizeof(set_limit_bad_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET PERF LIMIT (short request)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_PERF_LIMIT,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = notsupp_expdata,
				.expected_data_len = sizeof(notsupp_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET FAST CHANNEL REGION",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_FAST_CHANNEL_REGION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = get_fc_region_expdata,
				.expected_data_len = sizeof(get_fc_region_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET FAST CHANNEL ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_FAST_CHANNEL_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_fc_attrs_reqdata,
				.request_data_len = sizeof(get_fc_attrs_reqdata),
				.expected_data = get_fc_attrs_expdata,
				.expected_data_len = sizeof(get_fc_attrs_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET FAST CHANNEL ATTRIBUTES (unsupported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_PERFORMANCE,
				.service_id = RPMI_PERF_SRV_GET_FAST_CHANNEL_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_fc_attrs_unsupported_reqdata,
				.request_data_len = sizeof(get_fc_attrs_unsupported_reqdata),
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
	printf("Test Performance Service Group\n");
	return test_scenario_execute(&scenario_perf_default);
}
