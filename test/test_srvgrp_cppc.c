// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "librpmi_internal.h"
#include "test_common.h"
#include "test_log.h"

#define TEST_HART_ID		0
#define TEST_HART_ID_INVALID	1
#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_CPPC_HIGHEST_PERF	0x64
#define TEST_CPPC_NOMINAL_PERF	0x50
#define TEST_CPPC_COUNTER_LO	0x55667788
#define TEST_CPPC_COUNTER_HI	0x11223344
#define TEST_CPPC_FASTCHAN_SIZE	64
#define TEST_CPPC_FASTCHAN_REQ_OFFSET	0
#define TEST_CPPC_FASTCHAN_FB_OFFSET	RPMI_CPPC_FASTCHAN_SIZE

static rpmi_uint8_t test_fastchan_mem[TEST_CPPC_FASTCHAN_SIZE]
	__aligned(RPMI_CPPC_FASTCHAN_SIZE);

static rpmi_uint32_t test_hartid_array[] = {
	TEST_HART_ID,
};

static struct rpmi_cppc_regs test_cppc_regs = {
	.highest_perf = TEST_CPPC_HIGHEST_PERF,
	.nominal_perf = TEST_CPPC_NOMINAL_PERF,
	.lowest_nonlinear_perf = 0x30,
	.lowest_perf = 0x10,
	.reference_perf = TEST_CPPC_NOMINAL_PERF,
	.lowest_freq = 1000000000U,
	.nominal_freq = 2000000000U,
	.transition_latency = 10,
};

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE,
};

static rpmi_uint32_t enable_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t probe_highest_reqdata[] = {
	TEST_HART_ID,
	RPMI_CPPC_HIGHEST_PERF,
};

static rpmi_uint32_t probe_32bit_expdata[] = {
	RPMI_SUCCESS,
	32,
};

static rpmi_uint32_t probe_not_supp_reqdata[] = {
	TEST_HART_ID,
	RPMI_CPPC_MIN_PERF,
};

static rpmi_uint32_t probe_not_supp_expdata[] = {
	RPMI_ERR_NOTSUPP,
	0,
};

static rpmi_uint32_t probe_invalid_reg_reqdata[] = {
	TEST_HART_ID,
	RPMI_CPPC_NON_ACPI_REG_MAX_IDX,
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t read_highest_reqdata[] = {
	TEST_HART_ID,
	RPMI_CPPC_HIGHEST_PERF,
};

static rpmi_uint32_t read_highest_expdata[] = {
	RPMI_SUCCESS,
	TEST_CPPC_HIGHEST_PERF,
	0,
};

static rpmi_uint32_t read_counter_reqdata[] = {
	TEST_HART_ID,
	RPMI_CPPC_REFERENCE_PERF_COUNTER,
};

static rpmi_uint32_t read_counter_expdata[] = {
	RPMI_SUCCESS,
	TEST_CPPC_COUNTER_LO,
	TEST_CPPC_COUNTER_HI,
};

static rpmi_uint32_t read_invalid_hart_reqdata[] = {
	TEST_HART_ID_INVALID,
	RPMI_CPPC_HIGHEST_PERF,
};

static rpmi_uint32_t write_readonly_reqdata[] = {
	TEST_HART_ID,
	RPMI_CPPC_HIGHEST_PERF,
	1,
	0,
};

static rpmi_uint32_t denied_expdata[] = {
	RPMI_ERR_DENIED,
};

static rpmi_uint32_t get_fastchan_offset_reqdata[] = {
	TEST_HART_ID,
};

static rpmi_uint32_t get_fastchan_offset_expdata[] = {
	RPMI_SUCCESS,
	TEST_CPPC_FASTCHAN_REQ_OFFSET,
	0,
	TEST_CPPC_FASTCHAN_FB_OFFSET,
	0,
};

static rpmi_uint32_t get_hart_list_reqdata[] = {
	0,
};

static rpmi_uint32_t get_hart_list_expdata[] = {
	RPMI_SUCCESS,
	0,
	1,
	TEST_HART_ID,
};

static enum rpmi_hart_hw_state test_hart_get_hw_state(void *priv,
						       rpmi_uint32_t hart_index)
{
	return RPMI_HART_HW_STATE_STARTED;
}

static struct rpmi_hsm_platform_ops test_hsm_ops = {
	.hart_get_hw_state = test_hart_get_hw_state,
};

static enum rpmi_error test_cppc_get_reg(void *priv, rpmi_uint32_t reg_id,
					 rpmi_uint32_t hart_index,
					 rpmi_uint64_t *val)
{
	if (reg_id == RPMI_CPPC_REFERENCE_PERF_COUNTER) {
		*val = ((rpmi_uint64_t)TEST_CPPC_COUNTER_HI << 32) |
		       TEST_CPPC_COUNTER_LO;
		return RPMI_SUCCESS;
	}

	*val = 0;
	return RPMI_ERR_NOTSUPP;
}

static enum rpmi_error test_cppc_set_reg(void *priv, rpmi_uint32_t reg_id,
					 rpmi_uint32_t hart_index,
					 rpmi_uint64_t val)
{
	return RPMI_SUCCESS;
}

static enum rpmi_error test_cppc_update_perf(void *priv,
					    rpmi_uint32_t hart_index,
					    rpmi_uint32_t desired_perf)
{
	return RPMI_SUCCESS;
}

static enum rpmi_error test_cppc_get_current_freq(void *priv,
						 rpmi_uint32_t hart_index,
						 rpmi_uint64_t *current_freq_hz)
{
	*current_freq_hz = 2000000000ULL;
	return RPMI_SUCCESS;
}

static struct rpmi_cppc_platform_ops test_cppc_ops = {
	.cppc_get_reg = test_cppc_get_reg,
	.cppc_set_reg = test_cppc_set_reg,
	.cppc_update_perf = test_cppc_update_perf,
	.cppc_get_current_freq = test_cppc_get_current_freq,
};

static rpmi_uint16_t init_fastchan_region_expected(struct rpmi_test_scenario *scene,
						   struct rpmi_test *test,
						   void *data,
						   rpmi_uint16_t max_data_len)
{
	rpmi_uint32_t *exp = data;
	rpmi_uint64_t base = (rpmi_uint64_t)(rpmi_uintptr_t)test_fastchan_mem;

	exp[0] = RPMI_SUCCESS;
	exp[1] = 0;
	exp[2] = (rpmi_uint32_t)base;
	exp[3] = (rpmi_uint32_t)(base >> 32);
	exp[4] = TEST_CPPC_FASTCHAN_SIZE;
	exp[5] = 0;
	exp[6] = 0;
	exp[7] = 0;
	exp[8] = 0;
	exp[9] = 0;
	exp[10] = 0;
	exp[11] = 0;

	return 12 * sizeof(*exp);
}

static int test_cppc_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	struct rpmi_shmem *fastchan_shmem;
	struct rpmi_hsm *hsm;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	hsm = rpmi_hsm_create(ARRAY_SIZE(test_hartid_array),
			      test_hartid_array, 0, NULL, &test_hsm_ops, NULL);
	if (!hsm) {
		printf("failed to create rpmi hsm");
		return RPMI_ERR_FAILED;
	}

	fastchan_shmem = rpmi_shmem_create("test_cppc_fastchan",
					    (rpmi_uint64_t)(rpmi_uintptr_t)test_fastchan_mem,
					    sizeof(test_fastchan_mem),
					    &rpmi_shmem_simple_ops, NULL);
	if (!fastchan_shmem) {
		printf("failed to create cppc fastchannel shmem");
		return RPMI_ERR_FAILED;
	}

	grp = rpmi_service_group_cppc_create(hsm, &test_cppc_regs,
					       RPMI_CPPC_PASSIVE_MODE,
					       fastchan_shmem,
					       TEST_CPPC_FASTCHAN_REQ_OFFSET,
					       TEST_CPPC_FASTCHAN_FB_OFFSET,
					       &test_cppc_ops, NULL);
	if (!grp) {
		printf("failed to create rpmi cppc service group");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_cppc_default = {
	.name = "CPPC Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_cppc_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 11,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_ENABLE_NOTIFICATION,
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
			.name = "PROBE REG (implemented 32-bit reg)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_highest_reqdata,
				.request_data_len = sizeof(probe_highest_reqdata),
				.expected_data = probe_32bit_expdata,
				.expected_data_len = sizeof(probe_32bit_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "PROBE REG (not supported reg)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_not_supp_reqdata,
				.request_data_len = sizeof(probe_not_supp_reqdata),
				.expected_data = probe_not_supp_expdata,
				.expected_data_len = sizeof(probe_not_supp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "PROBE REG (invalid reg)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_invalid_reg_reqdata,
				.request_data_len = sizeof(probe_invalid_reg_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "READ REG (static 32-bit reg)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_READ_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = read_highest_reqdata,
				.request_data_len = sizeof(read_highest_reqdata),
				.expected_data = read_highest_expdata,
				.expected_data_len = sizeof(read_highest_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "READ REG (platform 64-bit counter)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_READ_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = read_counter_reqdata,
				.request_data_len = sizeof(read_counter_reqdata),
				.expected_data = read_counter_expdata,
				.expected_data_len = sizeof(read_counter_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "READ REG (invalid hart)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_READ_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = read_invalid_hart_reqdata,
				.request_data_len = sizeof(read_invalid_hart_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "WRITE REG (read-only reg denied)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_WRITE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = write_readonly_reqdata,
				.request_data_len = sizeof(write_readonly_reqdata),
				.expected_data = denied_expdata,
				.expected_data_len = sizeof(denied_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET FAST CHANNEL REGION",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_fastchan_region_expected,
		},
		{
			.name = "GET FAST CHANNEL OFFSET",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_GET_FAST_CHANNEL_OFFSET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_fastchan_offset_reqdata,
				.request_data_len = sizeof(get_fastchan_offset_reqdata),
				.expected_data = get_fastchan_offset_expdata,
				.expected_data_len = sizeof(get_fastchan_offset_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET HART LIST",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_GET_HART_LIST,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_hart_list_reqdata,
				.request_data_len = sizeof(get_hart_list_reqdata),
				.expected_data = get_hart_list_expdata,
				.expected_data_len = sizeof(get_hart_list_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test CPPC Service Group\n");
	return test_scenario_execute(&scenario_cppc_default);
}
