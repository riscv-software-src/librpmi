// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_CLK_COUNT		2
#define TEST_CLK_CPU		0
#define TEST_CLK_BUS		1
#define TEST_CLK_INVALID	TEST_CLK_COUNT
#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_CLK_CPU_LATENCY	5
#define TEST_CLK_BUS_LATENCY	7
#define TEST_CLK_CPU_INITIAL_RATE	100000000ULL
#define TEST_CLK_CPU_TARGET_RATE	5000000000ULL
#define TEST_CLK_CPU_MAX_RATE		6000000000ULL
#define TEST_CLK_CPU_UNSUPP_RATE	7000000000ULL
#define TEST_CLK_BUS_MIN_RATE	500000000ULL
#define TEST_CLK_BUS_MAX_RATE	1000000000ULL
#define TEST_CLK_BUS_STEP_RATE	100000000ULL

#define RATE_LO(_rate)	((rpmi_uint32_t)((rpmi_uint64_t)(_rate)))
#define RATE_HI(_rate)	((rpmi_uint32_t)(((rpmi_uint64_t)(_rate)) >> 32))

struct test_clk_attrs_expdata {
	rpmi_uint32_t status;
	rpmi_uint32_t flags;
	rpmi_uint32_t rate_count;
	rpmi_uint32_t transition_latency;
	char name[16];
};

static const rpmi_uint64_t test_cpu_rates[] = {
	TEST_CLK_CPU_INITIAL_RATE,
	TEST_CLK_CPU_TARGET_RATE,
	TEST_CLK_CPU_MAX_RATE,
};

static const rpmi_uint64_t test_bus_rates[] = {
	TEST_CLK_BUS_MIN_RATE,
	TEST_CLK_BUS_MAX_RATE,
	TEST_CLK_BUS_STEP_RATE,
};

static struct rpmi_clock_data test_clock_data[TEST_CLK_COUNT] = {
	[TEST_CLK_CPU] = {
		.parent_id = -1U,
		.transition_latency_ms = TEST_CLK_CPU_LATENCY,
		.rate_count = ARRAY_SIZE(test_cpu_rates),
		.clock_type = RPMI_CLK_TYPE_DISCRETE,
		.name = "clk_cpu",
		.clock_rate_array = test_cpu_rates,
	},
	[TEST_CLK_BUS] = {
		.parent_id = -1U,
		.transition_latency_ms = TEST_CLK_BUS_LATENCY,
		.rate_count = ARRAY_SIZE(test_bus_rates),
		.clock_type = RPMI_CLK_TYPE_LINEAR,
		.name = "clk_bus",
		.clock_rate_array = test_bus_rates,
	},
};

static enum rpmi_clock_state test_clock_state[TEST_CLK_COUNT] = {
	RPMI_CLK_STATE_DISABLED,
	RPMI_CLK_STATE_ENABLED,
};

static rpmi_uint64_t test_clock_rate[TEST_CLK_COUNT] = {
	TEST_CLK_CPU_INITIAL_RATE,
	TEST_CLK_BUS_MIN_RATE,
};

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE,
};

static rpmi_uint32_t notsupp_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t get_num_clocks_expdata[] = {
	RPMI_SUCCESS,
	TEST_CLK_COUNT,
};

static rpmi_uint32_t clk_cpu_reqdata[] = {
	TEST_CLK_CPU,
};

static rpmi_uint32_t clk_invalid_reqdata[] = {
	TEST_CLK_INVALID,
};

static struct test_clk_attrs_expdata get_attrs_cpu_expdata = {
	.status = RPMI_SUCCESS,
	.flags = 0,
	.rate_count = ARRAY_SIZE(test_cpu_rates),
	.transition_latency = TEST_CLK_CPU_LATENCY,
	.name = "clk_cpu",
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t get_cpu_rates_reqdata[] = {
	TEST_CLK_CPU,
	1,
};

static rpmi_uint32_t get_cpu_rates_expdata[] = {
	RPMI_SUCCESS,
	0,
	0,
	2,
	RATE_LO(TEST_CLK_CPU_TARGET_RATE),
	RATE_HI(TEST_CLK_CPU_TARGET_RATE),
	RATE_LO(TEST_CLK_CPU_MAX_RATE),
	RATE_HI(TEST_CLK_CPU_MAX_RATE),
};

static rpmi_uint32_t get_bus_rates_reqdata[] = {
	TEST_CLK_BUS,
	0,
};

static rpmi_uint32_t get_bus_rates_expdata[] = {
	RPMI_SUCCESS,
	0,
	0,
	3,
	RATE_LO(TEST_CLK_BUS_MIN_RATE),
	RATE_HI(TEST_CLK_BUS_MIN_RATE),
	RATE_LO(TEST_CLK_BUS_MAX_RATE),
	RATE_HI(TEST_CLK_BUS_MAX_RATE),
	RATE_LO(TEST_CLK_BUS_STEP_RATE),
	RATE_HI(TEST_CLK_BUS_STEP_RATE),
};

static rpmi_uint32_t get_cpu_rates_bad_index_reqdata[] = {
	TEST_CLK_CPU,
	4,
};

static rpmi_uint32_t set_config_enable_reqdata[] = {
	TEST_CLK_CPU,
	1,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t get_config_enabled_expdata[] = {
	RPMI_SUCCESS,
	1,
};

static rpmi_uint8_t get_config_short_reqdata[] = {
	TEST_CLK_CPU,
};

static rpmi_uint32_t set_rate_cpu_reqdata[] = {
	TEST_CLK_CPU,
	RPMI_CLK_RATE_MATCH_ROUND_UP,
	RATE_LO(TEST_CLK_CPU_TARGET_RATE),
	RATE_HI(TEST_CLK_CPU_TARGET_RATE),
};

static rpmi_uint32_t get_rate_cpu_expdata[] = {
	RPMI_SUCCESS,
	RATE_LO(TEST_CLK_CPU_TARGET_RATE),
	RATE_HI(TEST_CLK_CPU_TARGET_RATE),
};

static rpmi_uint32_t set_rate_bad_match_reqdata[] = {
	TEST_CLK_CPU,
	RPMI_CLK_RATE_MATCH_MAX,
	RATE_LO(TEST_CLK_CPU_TARGET_RATE),
	RATE_HI(TEST_CLK_CPU_TARGET_RATE),
};

static rpmi_uint32_t set_rate_zero_reqdata[] = {
	TEST_CLK_CPU,
	RPMI_CLK_RATE_MATCH_PLATFORM,
	0,
	0,
};

static rpmi_uint32_t set_rate_unsupported_reqdata[] = {
	TEST_CLK_CPU,
	RPMI_CLK_RATE_MATCH_PLATFORM,
	RATE_LO(TEST_CLK_CPU_UNSUPP_RATE),
	RATE_HI(TEST_CLK_CPU_UNSUPP_RATE),
};

static rpmi_bool_t test_clock_rate_supported(rpmi_uint32_t clock_id,
						   rpmi_uint64_t rate)
{
	const rpmi_uint64_t *rates;
	rpmi_uint32_t count, i;

	if (clock_id >= TEST_CLK_COUNT)
		return false;

	if (clock_id == TEST_CLK_CPU) {
		rates = test_cpu_rates;
		count = ARRAY_SIZE(test_cpu_rates);
	} else {
		rates = test_bus_rates;
		count = ARRAY_SIZE(test_bus_rates);
	}

	for (i = 0; i < count; i++) {
		if (rates[i] == rate)
			return true;
	}

	return false;
}

static enum rpmi_error test_clock_set_state(void *priv,
					    rpmi_uint32_t clock_id,
					    enum rpmi_clock_state state)
{
	if (clock_id >= TEST_CLK_COUNT || state >= RPMI_CLK_STATE_MAX)
		return RPMI_ERR_INVALID_PARAM;

	test_clock_state[clock_id] = state;
	return RPMI_SUCCESS;
}

static enum rpmi_error test_clock_get_state_and_rate(void *priv,
						     rpmi_uint32_t clock_id,
						     enum rpmi_clock_state *state,
						     rpmi_uint64_t *rate)
{
	if (clock_id >= TEST_CLK_COUNT)
		return RPMI_ERR_INVALID_PARAM;

	if (state)
		*state = test_clock_state[clock_id];
	if (rate)
		*rate = test_clock_rate[clock_id];

	return RPMI_SUCCESS;
}

static rpmi_bool_t test_clock_rate_change_match(void *priv,
						      rpmi_uint32_t clock_id,
						      rpmi_uint64_t rate)
{
	if (clock_id >= TEST_CLK_COUNT)
		return false;

	return test_clock_rate[clock_id] != rate;
}

static enum rpmi_error test_clock_set_rate(void *priv,
					   rpmi_uint32_t clock_id,
					   enum rpmi_clock_rate_match match,
					   rpmi_uint64_t rate,
					   rpmi_uint64_t *new_rate)
{
	if (clock_id >= TEST_CLK_COUNT || !new_rate)
		return RPMI_ERR_INVALID_PARAM;

	if (!test_clock_rate_supported(clock_id, rate))
		return RPMI_ERR_INVALID_PARAM;

	test_clock_rate[clock_id] = rate;
	*new_rate = rate;
	return RPMI_SUCCESS;
}

static enum rpmi_error test_clock_set_rate_recalc(void *priv,
						 rpmi_uint32_t clock_id,
						 rpmi_uint64_t parent_rate,
						 rpmi_uint64_t *new_rate)
{
	if (clock_id >= TEST_CLK_COUNT || !new_rate)
		return RPMI_ERR_INVALID_PARAM;

	test_clock_rate[clock_id] = parent_rate;
	*new_rate = parent_rate;
	return RPMI_SUCCESS;
}

static struct rpmi_clock_platform_ops test_clock_ops = {
	.set_state = test_clock_set_state,
	.get_state_and_rate = test_clock_get_state_and_rate,
	.rate_change_match = test_clock_rate_change_match,
	.set_rate = test_clock_set_rate,
	.set_rate_recalc = test_clock_set_rate_recalc,
};

static int test_clock_state_init(struct rpmi_test_scenario *scene,
					 struct rpmi_test *test)
{
	test_clock_state[TEST_CLK_CPU] = RPMI_CLK_STATE_DISABLED;
	test_clock_state[TEST_CLK_BUS] = RPMI_CLK_STATE_ENABLED;
	test_clock_rate[TEST_CLK_CPU] = TEST_CLK_CPU_INITIAL_RATE;
	test_clock_rate[TEST_CLK_BUS] = TEST_CLK_BUS_MIN_RATE;
	return 0;
}

static int test_clock_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	test_clock_state[TEST_CLK_CPU] = RPMI_CLK_STATE_DISABLED;
	test_clock_state[TEST_CLK_BUS] = RPMI_CLK_STATE_ENABLED;
	test_clock_rate[TEST_CLK_CPU] = TEST_CLK_CPU_INITIAL_RATE;
	test_clock_rate[TEST_CLK_BUS] = TEST_CLK_BUS_MIN_RATE;

	grp = rpmi_service_group_clock_create(TEST_CLK_COUNT,
						 test_clock_data,
						 &test_clock_ops, NULL);
	if (!grp) {
		printf("failed to create rpmi clock service group");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_clock_default = {
	.name = "Clock Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_clock_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 16,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_ENABLE_NOTIFICATION,
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
			.name = "GET NUM CLOCKS",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_NUM_CLOCKS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = get_num_clocks_expdata,
				.expected_data_len = sizeof(get_num_clocks_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = clk_cpu_reqdata,
				.request_data_len = sizeof(clk_cpu_reqdata),
				.expected_data = &get_attrs_cpu_expdata,
				.expected_data_len = sizeof(get_attrs_cpu_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES (invalid clock)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = clk_invalid_reqdata,
				.request_data_len = sizeof(clk_invalid_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET SUPPORTED RATES (discrete)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_SUPPORTED_RATES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_cpu_rates_reqdata,
				.request_data_len = sizeof(get_cpu_rates_reqdata),
				.expected_data = get_cpu_rates_expdata,
				.expected_data_len = sizeof(get_cpu_rates_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET SUPPORTED RATES (linear)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_SUPPORTED_RATES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_bus_rates_reqdata,
				.request_data_len = sizeof(get_bus_rates_reqdata),
				.expected_data = get_bus_rates_expdata,
				.expected_data_len = sizeof(get_bus_rates_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET SUPPORTED RATES (bad index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_SUPPORTED_RATES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_cpu_rates_bad_index_reqdata,
				.request_data_len = sizeof(get_cpu_rates_bad_index_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET CONFIG (enable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_SET_CONFIG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_config_enable_reqdata,
				.request_data_len = sizeof(set_config_enable_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init = test_clock_state_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET CONFIG (enabled)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_CONFIG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = clk_cpu_reqdata,
				.request_data_len = sizeof(clk_cpu_reqdata),
				.expected_data = get_config_enabled_expdata,
				.expected_data_len = sizeof(get_config_enabled_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET CONFIG (truncated clock id)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_CONFIG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_config_short_reqdata,
				.request_data_len = sizeof(get_config_short_reqdata),
				.expected_data = notsupp_expdata,
				.expected_data_len = sizeof(notsupp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET RATE",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_SET_RATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_rate_cpu_reqdata,
				.request_data_len = sizeof(set_rate_cpu_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET RATE",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_RATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = clk_cpu_reqdata,
				.request_data_len = sizeof(clk_cpu_reqdata),
				.expected_data = get_rate_cpu_expdata,
				.expected_data_len = sizeof(get_rate_cpu_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET RATE (bad match)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_SET_RATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_rate_bad_match_reqdata,
				.request_data_len = sizeof(set_rate_bad_match_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET RATE (zero rate)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_SET_RATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_rate_zero_reqdata,
				.request_data_len = sizeof(set_rate_zero_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET RATE (unsupported rate)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_SET_RATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_rate_unsupported_reqdata,
				.request_data_len = sizeof(set_rate_unsupported_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET RATE (invalid clock)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CLOCK,
				.service_id = RPMI_CLK_SRV_GET_RATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = clk_invalid_reqdata,
				.request_data_len = sizeof(clk_invalid_reqdata),
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
	printf("Test Clock Service Group\n");
	return test_scenario_execute(&scenario_clock_default);
}
