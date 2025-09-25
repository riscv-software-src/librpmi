/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_HSM_CONFIG_HART_MAX		64
#define TEST_HSM_CONFIG_HART_COUNT		4
#define TEST_HSM_CONFIG_HART_BASEHARTID		0

#define TEST_EVENT_ID				0x0

#define TEST_REQUEST_STATE_DISABLE		0x0
#define TEST_REQUEST_STATE_ENABLE		0x1
#define TEST_REQUEST_STATE_RETURN		0x2

#define TEST_HART_ID_VALID			TEST_HSM_CONFIG_HART_BASEHARTID
#define TEST_HART_ID_INVALID			TEST_HSM_CONFIG_HART_COUNT

#define TEST_HART_STATE_IGNORED_VALUE		0x0

#define TEST_HART_START_ADDR_LOW		0xdead0000
#define TEST_HART_START_ADDR_HIGH		0x0000beef

/* Hart array for hsm tests */
rpmi_uint32_t test_hartid_array[TEST_HSM_CONFIG_HART_MAX] = {
			[0 ... TEST_HSM_CONFIG_HART_MAX-1] = -1U
};

/** Hart states array. The harts are already started by the platform and executing */
rpmi_uint32_t test_hart_state[TEST_HSM_CONFIG_HART_MAX] = {
			[0 ... (TEST_HSM_CONFIG_HART_MAX - 1)] = RPMI_HART_HW_STATE_STARTED
};

/* Enable Notification - Request Data */
static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE
};

/* Enable Notification - Response Data */
static rpmi_uint32_t enable_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

/* Enable Notification for Status Check - Request Data */
static rpmi_uint32_t status_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_RETURN
};

/* Enable Notification for Status Check - Response Data */
static rpmi_uint32_t status_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

/* Get Harts List - Request Data */
static rpmi_uint32_t get_hart_list_reqdata[] = {
	0,	/* start of the list */
};

/* Get Harts List - Response Data */
static rpmi_uint32_t get_hart_list_expdata[] = {
	RPMI_SUCCESS,
	0,				/* Remaining */
	TEST_HSM_CONFIG_HART_COUNT,	/* Returned */
	0,1,2,3				/* Hart IDs, working case hart_id = (hartindex + basehartid)
					   hartindex = 0,1,2,3 and basehartid = 0,1,2,3
					   where hartindex starts from 0 */
};

/* Get Harts Status (valid hartid) - Request Data */
static rpmi_uint32_t get_hart_state_valid_hartid_reqdata[] = {
	TEST_HART_ID_VALID
};

/* Get Harts Status (valid hartid) - Response Data */
static rpmi_uint32_t get_hart_state_valid_hartid_expdata[] = {
	RPMI_SUCCESS,
	RPMI_HSM_HART_STATE_STARTED
};

/* Get Harts Status (invalid hartid) - Request Data */
static rpmi_uint32_t get_hart_state_invalid_hartid_reqdata[] = {
	TEST_HART_ID_INVALID
};

/* Get Harts Status (invalid hartid) - Response Data */
static rpmi_uint32_t get_hart_state_invalid_hartid_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
	TEST_HART_STATE_IGNORED_VALUE
};

/* Hart Start (valid hartid) - Request Data */
static rpmi_uint32_t hart_start_valid_hartid_reqdata[] = {
	TEST_HART_ID_VALID,
	TEST_HART_START_ADDR_LOW,
	TEST_HART_START_ADDR_HIGH
};

/* Harts Start (valid hartid) - Response Data */
static rpmi_uint32_t hart_start_valid_hartid_expdata[] = {
	RPMI_ERR_ALREADY,
};

/* Hart Start (valid hartid) - Request Data */
static rpmi_uint32_t hart_start_invalid_hartid_reqdata[] = {
	TEST_HART_ID_INVALID,
	TEST_HART_START_ADDR_LOW,
	TEST_HART_START_ADDR_HIGH
};

/* Harts Start (valid hartid) - Response Data */
static rpmi_uint32_t hart_start_invalid_hartid_expdata[] = {
	RPMI_ERR_INVALID_PARAM
};

/* Hart Stop (started hart) - Request Data */
static rpmi_uint32_t hart_stop_started_hart_reqdata[] = {
	TEST_HART_ID_VALID,
};

/* Harts Start (valid hartid) - Response Data */
static rpmi_uint32_t hart_stop_started_hart_expdata[] = {
	RPMI_SUCCESS,
};

/* Hart Stop (stopped hart) - Request Data */
static rpmi_uint32_t hart_stop_stopped_hart_reqdata[] = {
	TEST_HART_ID_VALID,
};

/* Harts Start (stopped hart) - Response Data */
static rpmi_uint32_t hart_stop_stopped_hart_expdata[] = {
	RPMI_ERR_ALREADY,
};

/* Hart Suspend (not supported) - Request Data */
static rpmi_uint32_t hart_suspend_notsupp_reqdata[] = {
	TEST_HART_ID_VALID,
};

/* Harts Suspend (not supported) - Response Data */
static rpmi_uint32_t hart_suspend_notsupp_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

/**
 * Platform Callbacks for Hart State Management
 */
enum rpmi_hart_hw_state test_hart_get_hw_state(void *priv, rpmi_uint32_t hart_index)
{
	return test_hart_state[hart_index];
}

enum rpmi_error test_hart_start_prepare(void *priv, rpmi_uint32_t hart_index,
					rpmi_uint64_t start_addr)
{
	test_hart_state[hart_index] = RPMI_HSM_HART_STATE_STARTED;
	return RPMI_SUCCESS;	
}

void test_hart_start_finalize(void *priv, rpmi_uint32_t hart_index,
			      rpmi_uint64_t start_addr)
{
	return;	
}

enum rpmi_error test_hart_stop_prepare(void *priv, rpmi_uint32_t hart_index)
{
	test_hart_state[hart_index] = RPMI_HSM_HART_STATE_STOPPED;
	return RPMI_SUCCESS;
}

void test_hart_stop_finalize(void *priv, rpmi_uint32_t hart_index)
{
	return;
}

enum rpmi_error test_hart_suspend_prepare(void *priv, rpmi_uint32_t hart_index,
					  const struct rpmi_hsm_suspend_type *suspend_type,
					  rpmi_uint64_t resume_addr)
{
	test_hart_state[hart_index] = RPMI_HSM_HART_STATE_SUSPENDED;
	return RPMI_SUCCESS;
}

void test_hart_suspend_finalize(void *priv,
				rpmi_uint32_t hart_index,
				const struct rpmi_hsm_suspend_type *suspend_type,
				rpmi_uint64_t resume_addr)
{
	return;
}

struct rpmi_hsm_platform_ops test_hsm_ops = {
	.hart_get_hw_state = test_hart_get_hw_state,
	.hart_start_prepare = test_hart_start_prepare,
	.hart_start_finalize = test_hart_start_finalize,
	.hart_stop_prepare = test_hart_stop_prepare,
	.hart_stop_finalize = test_hart_stop_finalize,
	.hart_suspend_prepare = test_hart_suspend_prepare,
	.hart_suspend_finalize = test_hart_suspend_finalize
};

static int test_hsm_scenario_init(struct rpmi_test_scenario *scene)
{
	int ret;
	rpmi_uint32_t hartindex;
	rpmi_uint32_t hart_count = TEST_HSM_CONFIG_HART_COUNT;
	rpmi_uint32_t basehartid = TEST_HSM_CONFIG_HART_BASEHARTID;
	struct rpmi_service_group *grp;
	struct rpmi_hsm *hsm_cntx;

	/* Create sequential hartids starting from basehartid */
	for (hartindex = 0; hartindex < hart_count; hartindex++)
		test_hartid_array[hartindex] = hartindex + basehartid;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	hsm_cntx = rpmi_hsm_create(hart_count,
				   test_hartid_array, 0, NULL,
				   &test_hsm_ops, NULL);
	if (!hsm_cntx) {
		printf("failed to create rpmi hsm");
		return RPMI_ERR_FAILED;
	}

	grp = rpmi_service_group_hsm_create(hsm_cntx);
	if (!grp) {
		printf("failed to create rpmi hsm service group");
		return RPMI_ERR_FAILED;
	}
	
	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_hsm_default = {
	.name = "System HSM Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_hsm_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 10,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_ENABLE_NOTIFICATION,
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
			.name = "NOTIFICATION STATUS TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = status_notif_reqdata,
				.request_data_len = sizeof(status_notif_reqdata),
				.expected_data = status_notif_expdata,
				.expected_data_len = sizeof(status_notif_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET HART LIST",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_GET_HART_LIST,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_hart_list_reqdata,
				.request_data_len = sizeof(get_hart_list_reqdata),
				.expected_data = get_hart_list_expdata,
				.expected_data_len = sizeof(get_hart_list_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET HART STATE (valid hart id)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_GET_HART_STATUS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_hart_state_valid_hartid_reqdata,
				.request_data_len = sizeof(get_hart_state_valid_hartid_reqdata),
				.expected_data = get_hart_state_valid_hartid_expdata,
				.expected_data_len = sizeof(get_hart_state_valid_hartid_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET HART STATE (invalid hart id)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_GET_HART_STATUS,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_hart_state_invalid_hartid_reqdata,
				.request_data_len = sizeof(get_hart_state_invalid_hartid_reqdata),
				.expected_data = get_hart_state_invalid_hartid_expdata,
				.expected_data_len = sizeof(get_hart_state_invalid_hartid_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "HART START (valid hart id, hart already started)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_HART_START,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = hart_start_valid_hartid_reqdata,
				.request_data_len = sizeof(hart_start_valid_hartid_reqdata),
				.expected_data = hart_start_valid_hartid_expdata,
				.expected_data_len = sizeof(hart_start_valid_hartid_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "HART START (invalid hart id)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_HART_START,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = hart_start_invalid_hartid_reqdata,
				.request_data_len = sizeof(hart_start_invalid_hartid_reqdata),
				.expected_data = hart_start_invalid_hartid_expdata,
				.expected_data_len = sizeof(hart_start_invalid_hartid_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "HART STOP (hart in start state)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_HART_STOP,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = hart_stop_started_hart_reqdata,
				.request_data_len = sizeof(hart_stop_started_hart_reqdata),
				.expected_data = hart_stop_started_hart_expdata,
				.expected_data_len = sizeof(hart_stop_started_hart_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "HART STOP (hart already stopped)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_HART_STOP,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = hart_stop_stopped_hart_reqdata,
				.request_data_len = sizeof(hart_stop_stopped_hart_reqdata),
				.expected_data = hart_stop_stopped_hart_expdata,
				.expected_data_len = sizeof(hart_stop_stopped_hart_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "HART Suspend (not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_HSM,
				.service_id = RPMI_HSM_SRV_HART_SUSPEND,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = hart_suspend_notsupp_reqdata,
				.request_data_len = sizeof(hart_suspend_notsupp_reqdata),
				.expected_data = hart_suspend_notsupp_expdata,
				.expected_data_len = sizeof(hart_suspend_notsupp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test Hart State Management Service Group\n");
	return test_scenario_execute(&scenario_hsm_default);;
}
