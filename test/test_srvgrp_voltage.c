// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Qualcomm Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_VOLT_COUNT		2
#define TEST_VOLT_CPU		0
#define TEST_VOLT_MEM		1
#define TEST_VOLT_INVALID	TEST_VOLT_COUNT
#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_VOLT_CPU_LATENCY	10
#define TEST_VOLT_MEM_LATENCY	20
#define TEST_VOLT_CPU_INITIAL	800000
#define TEST_VOLT_CPU_TARGET	1000000
#define TEST_VOLT_CPU_UNSUPP	1100000
#define TEST_VOLT_BAD_CONFIG	RPMI_VOLT_CONFIG_MAX

struct test_volt_attrs_expdata {
	rpmi_uint32_t status;
	rpmi_uint32_t capability;
	rpmi_uint32_t num_levels;
	rpmi_uint32_t trans_latency;
	char name[16];
};

static rpmi_int32_t test_cpu_levels[] = {
	800000,
	900000,
	1000000,
};

static rpmi_int32_t test_mem_levels[] = {
	1800000,
	1900000,
};

static struct rpmi_voltage_discrete_range test_cpu_range = {
	.uvolt = (rpmi_uint32_t *)test_cpu_levels,
};

static struct rpmi_voltage_linear_range test_mem_range = {
	.uvolt_min = 1800000,
	.uvolt_max = 1900000,
	.uvolt_step = 100000,
};

static struct rpmi_voltage_data test_voltage_data[TEST_VOLT_COUNT] = {
	[TEST_VOLT_CPU] = {
		.name = "vdd_cpu",
		.voltage_type = RPMI_VOLT_TYPE_DISCRETE,
		.control = RPMI_VOLT_CAPABILITY_ENABLED_DISABLED,
		.config = RPMI_VOLT_CONFIG_DISABLED,
		.num_levels = ARRAY_SIZE(test_cpu_levels),
		.trans_latency = TEST_VOLT_CPU_LATENCY,
		.discrete_range = &test_cpu_range,
		.discrete_levels = test_cpu_levels,
		.level_uv = TEST_VOLT_CPU_INITIAL,
	},
	[TEST_VOLT_MEM] = {
		.name = "vdd_mem",
		.voltage_type = RPMI_VOLT_TYPE_LINEAR,
		.control = RPMI_VOLT_CAPABILITY_ALWAYS_ON,
		.config = RPMI_VOLT_CONFIG_NOT_SUPPORTED,
		.num_levels = ARRAY_SIZE(test_mem_levels),
		.trans_latency = TEST_VOLT_MEM_LATENCY,
		.linear_range = &test_mem_range,
		.linear_levels = test_mem_levels,
		.level_uv = 1800000,
	},
};

static rpmi_uint32_t test_current_config[TEST_VOLT_COUNT] = {
	RPMI_VOLT_CONFIG_DISABLED,
	RPMI_VOLT_CONFIG_NOT_SUPPORTED,
};

static rpmi_int32_t test_current_level[TEST_VOLT_COUNT] = {
	TEST_VOLT_CPU_INITIAL,
	1800000,
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
	TEST_VOLT_COUNT,
};

static rpmi_uint32_t volt_cpu_reqdata[] = {
	TEST_VOLT_CPU,
};

static rpmi_uint32_t volt_mem_reqdata[] = {
	TEST_VOLT_MEM,
};

static rpmi_uint32_t volt_invalid_reqdata[] = {
	TEST_VOLT_INVALID,
};

static struct test_volt_attrs_expdata get_attrs_cpu_expdata = {
	.status = RPMI_SUCCESS,
	.capability =
		RPMI_VOLT_CAPABILITY_FORMAT(RPMI_VOLT_TYPE_DISCRETE) |
		RPMI_VOLT_CAPABILITY_CONTROL(RPMI_VOLT_CAPABILITY_ENABLED_DISABLED),
	.num_levels = ARRAY_SIZE(test_cpu_levels),
	.trans_latency = TEST_VOLT_CPU_LATENCY,
	.name = "vdd_cpu",
};

static struct test_volt_attrs_expdata get_attrs_mem_expdata = {
	.status = RPMI_SUCCESS,
	.capability =
		RPMI_VOLT_CAPABILITY_FORMAT(RPMI_VOLT_TYPE_LINEAR) |
		RPMI_VOLT_CAPABILITY_CONTROL(RPMI_VOLT_CAPABILITY_ALWAYS_ON),
	.num_levels = ARRAY_SIZE(test_mem_levels),
	.trans_latency = TEST_VOLT_MEM_LATENCY,
	.name = "vdd_mem",
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t get_cpu_levels_reqdata[] = {
	TEST_VOLT_CPU,
	1,
};

static rpmi_uint32_t get_cpu_levels_expdata[] = {
	RPMI_SUCCESS,
	0,
	0,
	2,
	900000,
	1000000,
};

static rpmi_uint32_t get_mem_levels_reqdata[] = {
	TEST_VOLT_MEM,
	0,
};

static rpmi_uint32_t get_mem_levels_expdata[] = {
	RPMI_SUCCESS,
	0,
	0,
	2,
	1800000,
	1900000,
};

static rpmi_uint32_t get_invalid_levels_reqdata[] = {
	TEST_VOLT_INVALID,
	0,
};

static rpmi_uint32_t set_config_enable_reqdata[] = {
	TEST_VOLT_CPU,
	RPMI_VOLT_CONFIG_ENABLED,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t get_config_enabled_expdata[] = {
	RPMI_SUCCESS,
	RPMI_VOLT_CONFIG_ENABLED,
};

static rpmi_uint32_t set_config_bad_reqdata[] = {
	TEST_VOLT_CPU,
	TEST_VOLT_BAD_CONFIG,
};

static rpmi_uint32_t set_level_cpu_reqdata[] = {
	TEST_VOLT_CPU,
	TEST_VOLT_CPU_TARGET,
};

static rpmi_uint32_t get_level_cpu_expdata[] = {
	RPMI_SUCCESS,
	TEST_VOLT_CPU_TARGET,
};

static rpmi_uint32_t set_level_unsupported_reqdata[] = {
	TEST_VOLT_CPU,
	TEST_VOLT_CPU_UNSUPP,
};

static enum rpmi_error test_voltage_set_config(void *priv,
					       rpmi_uint32_t volt_id,
					       rpmi_uint32_t config)
{
	if (volt_id >= TEST_VOLT_COUNT)
		return RPMI_ERR_INVALID_PARAM;

	if (config >= RPMI_VOLT_CONFIG_MAX)
		return RPMI_ERR_INVALID_PARAM;

	test_current_config[volt_id] = config;
	return RPMI_SUCCESS;
}

static enum rpmi_error test_voltage_get_config(void *priv,
					       rpmi_uint32_t volt_id,
					       rpmi_uint32_t *config)
{
	if (volt_id >= TEST_VOLT_COUNT || !config)
		return RPMI_ERR_INVALID_PARAM;

	*config = test_current_config[volt_id];
	return RPMI_SUCCESS;
}

static rpmi_bool_t test_voltage_level_supported(rpmi_uint32_t volt_id,
						     rpmi_int32_t level)
{
	rpmi_int32_t *levels;
	rpmi_uint32_t i, count;

	if (volt_id >= TEST_VOLT_COUNT)
		return false;

	if (volt_id == TEST_VOLT_CPU) {
		levels = test_cpu_levels;
		count = ARRAY_SIZE(test_cpu_levels);
	} else {
		levels = test_mem_levels;
		count = ARRAY_SIZE(test_mem_levels);
	}

	for (i = 0; i < count; i++) {
		if (levels[i] == level)
			return true;
	}

	return false;
}

static enum rpmi_error test_voltage_set_level(void *priv,
					      rpmi_uint32_t volt_id,
					      rpmi_int32_t *volt_level)
{
	if (volt_id >= TEST_VOLT_COUNT || !volt_level)
		return RPMI_ERR_INVALID_PARAM;

	if (!test_voltage_level_supported(volt_id, *volt_level))
		return RPMI_ERR_INVALID_PARAM;

	test_current_level[volt_id] = *volt_level;
	return RPMI_SUCCESS;
}

static enum rpmi_error test_voltage_get_level(void *priv,
					      rpmi_uint32_t volt_id,
					      rpmi_int32_t *volt_level)
{
	if (volt_id >= TEST_VOLT_COUNT || !volt_level)
		return RPMI_ERR_INVALID_PARAM;

	*volt_level = test_current_level[volt_id];
	return RPMI_SUCCESS;
}

static enum rpmi_error test_voltage_get_supp_levels(void *priv,
						    rpmi_uint32_t volt_id,
						    rpmi_uint32_t max,
						    rpmi_uint32_t volt_index,
						    rpmi_uint32_t *returned_levels,
						    rpmi_int32_t *level_array)
{
	rpmi_int32_t *src;
	rpmi_uint32_t count, returned, i;

	if (volt_id >= TEST_VOLT_COUNT || !returned_levels || !level_array)
		return RPMI_ERR_INVALID_PARAM;

	if (volt_id == TEST_VOLT_CPU) {
		src = test_cpu_levels;
		count = ARRAY_SIZE(test_cpu_levels);
	} else {
		src = test_mem_levels;
		count = ARRAY_SIZE(test_mem_levels);
	}

	if (volt_index >= count)
		return RPMI_ERR_INVALID_PARAM;

	returned = count - volt_index;
	if (returned > max)
		returned = max;

	for (i = 0; i < returned; i++)
		level_array[i] = src[volt_index + i];

	*returned_levels = returned;
	return RPMI_SUCCESS;
}

static struct rpmi_voltage_platform_ops test_voltage_ops = {
	.set_config = test_voltage_set_config,
	.get_config = test_voltage_get_config,
	.set_level = test_voltage_set_level,
	.get_level = test_voltage_get_level,
	.get_supp_levels = test_voltage_get_supp_levels,
};

static int test_voltage_state_init(struct rpmi_test_scenario *scene,
					   struct rpmi_test *test)
{
	test_current_config[TEST_VOLT_CPU] = RPMI_VOLT_CONFIG_DISABLED;
	test_current_config[TEST_VOLT_MEM] = RPMI_VOLT_CONFIG_NOT_SUPPORTED;
	test_current_level[TEST_VOLT_CPU] = TEST_VOLT_CPU_INITIAL;
	test_current_level[TEST_VOLT_MEM] = test_mem_levels[0];
	return 0;
}

static int test_voltage_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	grp = rpmi_service_group_voltage_create(TEST_VOLT_COUNT,
						   test_voltage_data,
						   &test_voltage_ops, NULL);
	if (!grp) {
		printf("failed to create rpmi voltage service group");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_voltage_default = {
	.name = "Voltage Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_voltage_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 16,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_ENABLE_NOTIFICATION,
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
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_NUM_DOMAINS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = get_num_domains_expdata,
				.expected_data_len = sizeof(get_num_domains_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES (discrete)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = volt_cpu_reqdata,
				.request_data_len = sizeof(volt_cpu_reqdata),
				.expected_data = &get_attrs_cpu_expdata,
				.expected_data_len = sizeof(get_attrs_cpu_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES (linear)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = volt_mem_reqdata,
				.request_data_len = sizeof(volt_mem_reqdata),
				.expected_data = &get_attrs_mem_expdata,
				.expected_data_len = sizeof(get_attrs_mem_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES (invalid domain)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = volt_invalid_reqdata,
				.request_data_len = sizeof(volt_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET SUPPORTED LEVELS (discrete)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_cpu_levels_reqdata,
				.request_data_len = sizeof(get_cpu_levels_reqdata),
				.expected_data = get_cpu_levels_expdata,
				.expected_data_len = sizeof(get_cpu_levels_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET SUPPORTED LEVELS (linear)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_mem_levels_reqdata,
				.request_data_len = sizeof(get_mem_levels_reqdata),
				.expected_data = get_mem_levels_expdata,
				.expected_data_len = sizeof(get_mem_levels_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET SUPPORTED LEVELS (invalid domain)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_invalid_levels_reqdata,
				.request_data_len = sizeof(get_invalid_levels_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET CONFIG (enable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_SET_CONFIG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_config_enable_reqdata,
				.request_data_len = sizeof(set_config_enable_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init = test_voltage_state_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET CONFIG (enabled)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_CONFIG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = volt_cpu_reqdata,
				.request_data_len = sizeof(volt_cpu_reqdata),
				.expected_data = get_config_enabled_expdata,
				.expected_data_len = sizeof(get_config_enabled_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET CONFIG (invalid domain)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_CONFIG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = volt_invalid_reqdata,
				.request_data_len = sizeof(volt_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET CONFIG (invalid config)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_SET_CONFIG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_config_bad_reqdata,
				.request_data_len = sizeof(set_config_bad_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET VOLT LEVEL",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_SET_VOLT_LEVEL,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_level_cpu_reqdata,
				.request_data_len = sizeof(set_level_cpu_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET VOLT LEVEL",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_VOLT_LEVEL,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = volt_cpu_reqdata,
				.request_data_len = sizeof(volt_cpu_reqdata),
				.expected_data = get_level_cpu_expdata,
				.expected_data_len = sizeof(get_level_cpu_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET VOLT LEVEL (unsupported level)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_SET_VOLT_LEVEL,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_level_unsupported_reqdata,
				.request_data_len = sizeof(set_level_unsupported_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET VOLT LEVEL (invalid domain)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_VOLTAGE,
				.service_id = RPMI_VOLT_SRV_GET_VOLT_LEVEL,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = volt_invalid_reqdata,
				.request_data_len = sizeof(volt_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test Voltage Service Group\n");
	return test_scenario_execute(&scenario_voltage_default);
}
