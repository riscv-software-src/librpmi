// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "librpmi_internal.h"
#include "test_common.h"
#include "test_log.h"

#define TEST_HART_ID				0
#define TEST_HART_ID_INVALID			1
#define TEST_EVENT_ID				0x0
#define TEST_REQUEST_STATE_ENABLE		0x1
#define TEST_CPPC_HIGHEST_PERF			0x64
#define TEST_CPPC_NOMINAL_PERF			0x50
#define TEST_CPPC_GUARANTEED_PERF		0x48
#define TEST_CPPC_COUNTER_LO			0x55667788
#define TEST_CPPC_COUNTER_HI			0x11223344
#define TEST_CPPC_WRAPAROUND_LO			0xFFFFFFFF
#define TEST_CPPC_WRAPAROUND_HI			0x00000001
#define TEST_CPPC_ENERGY_PREF_VAL		0x80
#define TEST_CPPC_FASTCHAN_SIZE			64
#define TEST_CPPC_FASTCHAN_REQ_OFFSET		0
#define TEST_CPPC_FASTCHAN_FB_OFFSET		RPMI_CPPC_FASTCHAN_SIZE
/* FLAGS[4:3] = 0b01 for autonomous mode */
#define TEST_CPPC_AUTO_FLAGS			(0x01U << 3)
#define TEST_CPPC_AUTO_MIN_PERF			0x20
#define TEST_CPPC_AUTO_MAX_PERF			0x40

static rpmi_uint32_t test_auto_captured_min_perf;
static rpmi_uint32_t test_auto_captured_max_perf;

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
	RPMI_CPPC_HIGHEST_PERF,
	TEST_HART_ID,
};

static rpmi_uint32_t probe_32bit_expdata[] = {
	RPMI_SUCCESS,
	32,
};

static rpmi_uint32_t probe_not_supp_reqdata[] = {
	RPMI_CPPC_ENERGY_PERF_PREFERENCE,
	TEST_HART_ID,
};

static rpmi_uint32_t probe_not_supp_expdata[] = {
	RPMI_ERR_NOTSUPP,
	0,
};

static rpmi_uint32_t probe_invalid_reg_reqdata[] = {
	RPMI_CPPC_NON_ACPI_REG_MAX_IDX,
	TEST_HART_ID,
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t read_highest_reqdata[] = {
	RPMI_CPPC_HIGHEST_PERF,
	TEST_HART_ID,
};

static rpmi_uint32_t read_highest_expdata[] = {
	RPMI_SUCCESS,
	TEST_CPPC_HIGHEST_PERF,
	0,
};

static rpmi_uint32_t read_counter_reqdata[] = {
	RPMI_CPPC_REFERENCE_PERF_COUNTER,
	TEST_HART_ID,
};

static rpmi_uint32_t read_counter_expdata[] = {
	RPMI_SUCCESS,
	TEST_CPPC_COUNTER_LO,
	TEST_CPPC_COUNTER_HI,
};

static rpmi_uint32_t read_invalid_hart_reqdata[] = {
	RPMI_CPPC_HIGHEST_PERF,
	TEST_HART_ID_INVALID,
};

static rpmi_uint32_t write_readonly_reqdata[] = {
	RPMI_CPPC_HIGHEST_PERF,
	TEST_HART_ID,
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

	if (reg_id == RPMI_CPPC_ENERGY_PERF_PREFERENCE) {
		*val = TEST_CPPC_ENERGY_PREF_VAL;
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

static enum rpmi_error test_cppc_update_perf_auto(void *priv,
						  rpmi_uint32_t hart_index,
						  rpmi_uint32_t min_perf,
						  rpmi_uint32_t max_perf)
{
	test_auto_captured_min_perf = min_perf;
	test_auto_captured_max_perf = max_perf;
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
	.cppc_update_perf_auto = test_cppc_update_perf_auto,
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

struct test_cppc_scenario_config {
	enum rpmi_privilege_level privilege_level;
};

static struct test_cppc_scenario_config cppc_m_mode_config = {
	.privilege_level = RPMI_PRIVILEGE_M_MODE,
};

static struct test_cppc_scenario_config cppc_s_mode_config = {
	.privilege_level = RPMI_PRIVILEGE_S_MODE,
};

static int test_cppc_scene_base_init(struct rpmi_test_scenario *scene)
{
	struct test_cppc_scenario_config *cfg = scene->priv;
	enum rpmi_privilege_level plevel;

	if (!scene || scene->shm || scene->shmem || scene->xport || scene->cntx)
		return RPMI_ERR_ALREADY;

	plevel = cfg ? cfg->privilege_level : RPMI_PRIVILEGE_M_MODE;

	scene->shm = rpmi_env_zalloc(scene->shm_size);
	if (!scene->shm)
		return RPMI_ERR_FAILED;

	scene->shmem = rpmi_shmem_create("test_shmem",
					 (unsigned long)scene->shm,
					 scene->shm_size,
					 &rpmi_shmem_simple_ops, NULL);
	if (!scene->shmem) {
		printf("%s: failed to create test rpmi_shmem\n", __func__);
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
		return RPMI_ERR_FAILED;
	}

	scene->xport = rpmi_transport_shmem_create("test_transport",
						   scene->slot_size,
						   ((scene->shm_size * 3) / 4) / 2,
						   ((scene->shm_size * 1) / 4) / 2,
						   scene->shmem);
	if (!scene->xport) {
		printf("%s: failed to create test rpmi_transport\n", __func__);
		rpmi_shmem_destroy(scene->shmem);
		scene->shmem = NULL;
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
		return RPMI_ERR_FAILED;
	}

	scene->cntx = rpmi_context_create("test_context", scene->xport,
					  scene->max_num_groups, plevel,
					  scene->base.plat_info_len,
					  scene->base.plat_info);
	if (!scene->cntx) {
		printf("%s: failed to create test rpmi_context\n", __func__);
		rpmi_transport_shmem_destroy(scene->xport);
		scene->xport = NULL;
		rpmi_shmem_destroy(scene->shmem);
		scene->shmem = NULL;
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
		return RPMI_ERR_FAILED;
	}

	scene->token_sequence = 0;
	return 0;
}

static int test_cppc_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	struct rpmi_shmem *fastchan_shmem;
	struct rpmi_hsm *hsm;
	int ret;

	ret = test_cppc_scene_base_init(scene);
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
	.priv = &cppc_m_mode_config,

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

/* ---- Autonomous (CPPC2) mode scenario ------------------------------------ */

static rpmi_uint8_t test_auto_fastchan_mem[TEST_CPPC_FASTCHAN_SIZE]
	__aligned(RPMI_CPPC_FASTCHAN_SIZE);

static struct rpmi_cppc_regs test_cppc_auto_regs = {
	.highest_perf = TEST_CPPC_HIGHEST_PERF,
	.nominal_perf = TEST_CPPC_NOMINAL_PERF,
	.lowest_nonlinear_perf = 0x30,
	.lowest_perf = 0x10,
	.guaranteed_perf = TEST_CPPC_GUARANTEED_PERF,
	.reference_perf = TEST_CPPC_NOMINAL_PERF,
	.lowest_freq = 1000000000U,
	.nominal_freq = 2000000000U,
	.transition_latency = 10,
	.autonomous_selection_enable = 1,
	.counter_wraparound_time = ((rpmi_uint64_t)TEST_CPPC_WRAPAROUND_HI << 32) |
				   TEST_CPPC_WRAPAROUND_LO,
};

/* PROBE: autonomous-mode-only register → SUCCESS + 32 */
static rpmi_uint32_t probe_min_perf_reqdata[] = {
	RPMI_CPPC_MIN_PERF, TEST_HART_ID,
};

/* PROBE: both-modes register (64-bit) → SUCCESS + 64 */
static rpmi_uint32_t probe_wraparound_reqdata[] = {
	RPMI_CPPC_COUNTER_WRAPAROUND_TIME, TEST_HART_ID,
};

static rpmi_uint32_t probe_64bit_expdata[] = {
	RPMI_SUCCESS, 64,
};

/* PROBE: guaranteed_perf (both modes, 32-bit) */
static rpmi_uint32_t probe_guaranteed_reqdata[] = {
	RPMI_CPPC_GUARANTEED_PERF, TEST_HART_ID,
};

/* PROBE: energy_perf_preference (auto only) */
static rpmi_uint32_t probe_energy_pref_reqdata[] = {
	RPMI_CPPC_ENERGY_PERF_PREFERENCE, TEST_HART_ID,
};

/* READ: guaranteed_perf from regs struct */
static rpmi_uint32_t read_guaranteed_reqdata[] = {
	RPMI_CPPC_GUARANTEED_PERF, TEST_HART_ID,
};

static rpmi_uint32_t read_guaranteed_expdata[] = {
	RPMI_SUCCESS, TEST_CPPC_GUARANTEED_PERF, 0,
};

/* READ: counter_wraparound_time (64-bit) from regs struct */
static rpmi_uint32_t read_wraparound_reqdata[] = {
	RPMI_CPPC_COUNTER_WRAPAROUND_TIME, TEST_HART_ID,
};

static rpmi_uint32_t read_wraparound_expdata[] = {
	RPMI_SUCCESS, TEST_CPPC_WRAPAROUND_LO, TEST_CPPC_WRAPAROUND_HI,
};

/* READ: energy_perf_preference via cppc_get_reg */
static rpmi_uint32_t read_energy_pref_reqdata[] = {
	RPMI_CPPC_ENERGY_PERF_PREFERENCE, TEST_HART_ID,
};

static rpmi_uint32_t read_energy_pref_expdata[] = {
	RPMI_SUCCESS, TEST_CPPC_ENERGY_PREF_VAL, 0,
};

/* WRITE: energy_perf_preference via cppc_set_reg → SUCCESS */
static rpmi_uint32_t write_energy_pref_reqdata[] = {
	RPMI_CPPC_ENERGY_PERF_PREFERENCE, TEST_HART_ID, TEST_CPPC_ENERGY_PREF_VAL, 0,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

/* WRITE: min_perf while fastchannel is active → DENIED */
static rpmi_uint32_t write_min_perf_fc_reqdata[] = {
	RPMI_CPPC_MIN_PERF, TEST_HART_ID, 0x20, 0,
};

static rpmi_uint16_t init_auto_event_request(struct rpmi_test_scenario *scene,
					     struct rpmi_test *test,
					     void *data, rpmi_uint16_t max_data_len)
{
	union rpmi_cppc_perf_request_fastchan *fc =
		(union rpmi_cppc_perf_request_fastchan *)test_auto_fastchan_mem;

	fc->active.min_perf = TEST_CPPC_AUTO_MIN_PERF;
	fc->active.max_perf = TEST_CPPC_AUTO_MAX_PERF;

	return test_init_request_data_from_attrs(scene, test, data, max_data_len);
}

static int verify_auto_event(struct rpmi_test_scenario *scene,
			     struct rpmi_test *test,
			     struct rpmi_message *msg)
{
	struct rpmi_cppc_perf_feedback_fastchan *fb =
		(struct rpmi_cppc_perf_feedback_fastchan *)
		(test_auto_fastchan_mem + RPMI_CPPC_FASTCHAN_SIZE);
	rpmi_uint64_t freq;
	int failed = 0;

	if (test_auto_captured_min_perf != TEST_CPPC_AUTO_MIN_PERF ||
	    test_auto_captured_max_perf != TEST_CPPC_AUTO_MAX_PERF) {
		printf("%s: cppc_update_perf_auto args: min=%u max=%u (expected min=%u max=%u)\n",
		       test->name,
		       test_auto_captured_min_perf, test_auto_captured_max_perf,
		       TEST_CPPC_AUTO_MIN_PERF, TEST_CPPC_AUTO_MAX_PERF);
		failed++;
	}

	freq = ((rpmi_uint64_t)fb->cur_freq_high << 32) | fb->cur_freq_low;
	if (freq != 2000000000ULL) {
		printf("%s: feedback cur_freq_hz=%llu (expected 2000000000)\n",
		       test->name, (unsigned long long)freq);
		failed++;
	}

	return failed;
}

static rpmi_uint16_t init_auto_fastchan_region_expected(struct rpmi_test_scenario *scene,
							struct rpmi_test *test,
							void *data,
							rpmi_uint16_t max_data_len)
{
	rpmi_uint32_t *exp = data;
	rpmi_uint64_t base = (rpmi_uint64_t)(rpmi_uintptr_t)test_auto_fastchan_mem;

	exp[0] = RPMI_SUCCESS;
	exp[1] = TEST_CPPC_AUTO_FLAGS;
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

static int test_cppc_auto_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	struct rpmi_shmem *fastchan_shmem;
	struct rpmi_hsm *hsm;
	int ret;

	ret = test_cppc_scene_base_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	hsm = rpmi_hsm_create(ARRAY_SIZE(test_hartid_array),
			      test_hartid_array, 0, NULL, &test_hsm_ops, NULL);
	if (!hsm) {
		printf("failed to create rpmi hsm");
		return RPMI_ERR_FAILED;
	}

	fastchan_shmem = rpmi_shmem_create("test_cppc_auto_fastchan",
					   (rpmi_uint64_t)(rpmi_uintptr_t)test_auto_fastchan_mem,
					   sizeof(test_auto_fastchan_mem),
					   &rpmi_shmem_simple_ops, NULL);
	if (!fastchan_shmem) {
		printf("failed to create cppc auto fastchannel shmem");
		return RPMI_ERR_FAILED;
	}

	grp = rpmi_service_group_cppc_create(hsm, &test_cppc_auto_regs,
					     RPMI_CPPC_AUTO_MODE,
					     fastchan_shmem,
					     TEST_CPPC_FASTCHAN_REQ_OFFSET,
					     TEST_CPPC_FASTCHAN_FB_OFFSET,
					     &test_cppc_ops, NULL);
	if (!grp) {
		printf("failed to create rpmi cppc auto service group");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_cppc_auto = {
	.name = "CPPC Service Group (Autonomous Mode)",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = &cppc_m_mode_config,

	.init = test_cppc_auto_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 11,
	.tests = {
		{
			.name = "PROBE MIN_PERF (auto-only, available in auto mode)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_min_perf_reqdata,
				.request_data_len = sizeof(probe_min_perf_reqdata),
				.expected_data = probe_32bit_expdata,
				.expected_data_len = sizeof(probe_32bit_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "PROBE GUARANTEED_PERF (available in both modes)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_guaranteed_reqdata,
				.request_data_len = sizeof(probe_guaranteed_reqdata),
				.expected_data = probe_32bit_expdata,
				.expected_data_len = sizeof(probe_32bit_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "PROBE COUNTER_WRAPAROUND_TIME (64-bit, both modes)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_wraparound_reqdata,
				.request_data_len = sizeof(probe_wraparound_reqdata),
				.expected_data = probe_64bit_expdata,
				.expected_data_len = sizeof(probe_64bit_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "PROBE ENERGY_PERF_PREFERENCE (auto-only)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_energy_pref_reqdata,
				.request_data_len = sizeof(probe_energy_pref_reqdata),
				.expected_data = probe_32bit_expdata,
				.expected_data_len = sizeof(probe_32bit_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "READ GUARANTEED_PERF (from regs struct)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_READ_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = read_guaranteed_reqdata,
				.request_data_len = sizeof(read_guaranteed_reqdata),
				.expected_data = read_guaranteed_expdata,
				.expected_data_len = sizeof(read_guaranteed_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "READ COUNTER_WRAPAROUND_TIME (64-bit from regs struct)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_READ_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = read_wraparound_reqdata,
				.request_data_len = sizeof(read_wraparound_reqdata),
				.expected_data = read_wraparound_expdata,
				.expected_data_len = sizeof(read_wraparound_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "READ ENERGY_PERF_PREFERENCE (via platform cppc_get_reg)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_READ_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = read_energy_pref_reqdata,
				.request_data_len = sizeof(read_energy_pref_reqdata),
				.expected_data = read_energy_pref_expdata,
				.expected_data_len = sizeof(read_energy_pref_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "WRITE ENERGY_PERF_PREFERENCE (via platform cppc_set_reg)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_WRITE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = write_energy_pref_reqdata,
				.request_data_len = sizeof(write_energy_pref_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "WRITE MIN_PERF (fastchannel present, succeeds)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_WRITE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = write_min_perf_fc_reqdata,
				.request_data_len = sizeof(write_min_perf_fc_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET FAST CHANNEL REGION (auto mode flags)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_auto_fastchan_region_expected,
		},
		{
			.name = "EVENT: fastchan min/max triggers auto callback+feedback",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_highest_reqdata,
				.request_data_len = sizeof(probe_highest_reqdata),
				.expected_data = probe_32bit_expdata,
				.expected_data_len = sizeof(probe_32bit_expdata),
			},
			.init_request_data = init_auto_event_request,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = verify_auto_event,
		},
	},
};

static struct rpmi_test_scenario scenario_cppc_s_mode = {
	.name = "CPPC Service Group (S-mode)",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = &cppc_s_mode_config,

	.init = test_cppc_auto_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 4,
	.tests = {
		{
			.name = "PROBE HIGHEST_PERF (S-mode access)",
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
			.name = "READ HIGHEST_PERF (S-mode access)",
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
			.name = "PROBE MIN_PERF (S-mode access, autonomous mode)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_PROBE_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_min_perf_reqdata,
				.request_data_len = sizeof(probe_min_perf_reqdata),
				.expected_data = probe_32bit_expdata,
				.expected_data_len = sizeof(probe_32bit_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "READ GUARANTEED_PERF (S-mode access, autonomous mode)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_CPPC,
				.service_id = RPMI_CPPC_SRV_READ_REG,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = read_guaranteed_reqdata,
				.request_data_len = sizeof(read_guaranteed_reqdata),
				.expected_data = read_guaranteed_expdata,
				.expected_data_len = sizeof(read_guaranteed_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	int ret = 0;

	printf("Test CPPC Service Group\n");
	ret |= test_scenario_execute(&scenario_cppc_default);
	ret |= test_scenario_execute(&scenario_cppc_auto);
	ret |= test_scenario_execute(&scenario_cppc_s_mode);
	return ret;
}
