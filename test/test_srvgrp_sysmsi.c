// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_SYSMSI_COUNT	2
#define TEST_SYSMSI_VALID_INDEX	0
#define TEST_SYSMSI_INVALID_INDEX	TEST_SYSMSI_COUNT
#define TEST_SYSMSI_ADDR_LOW	0x12345000
#define TEST_SYSMSI_ADDR_HIGH	0x0
#define TEST_SYSMSI_DATA	0x55aa55aa
#define TEST_SYSMSI_BAD_ADDR_LOW	0xdead0000
#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE,
};

static rpmi_uint32_t enable_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t get_attrs_expdata[] = {
	RPMI_SUCCESS,
	TEST_SYSMSI_COUNT,
	0,
	0,
};

static rpmi_uint32_t get_msi_attrs_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
};

static rpmi_uint32_t get_msi_attrs_expdata[] = {
	RPMI_SUCCESS,
	RPMI_SYSMSI_MSI_ATTRIBUTES_FLAG0_PREF_PRIV,
	0,
	0,
	0,
	0,
	0,
};

static rpmi_uint32_t invalid_msi_reqdata[] = {
	TEST_SYSMSI_INVALID_INDEX,
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t set_msi_state_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
	RPMI_SYSMSI_MSI_STATE_ENABLE,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t get_msi_state_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
};

static rpmi_uint32_t get_msi_state_expdata[] = {
	RPMI_SUCCESS,
	RPMI_SYSMSI_MSI_STATE_ENABLE,
};

static rpmi_uint32_t set_msi_target_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
	TEST_SYSMSI_ADDR_LOW,
	TEST_SYSMSI_ADDR_HIGH,
	TEST_SYSMSI_DATA,
};

static rpmi_uint32_t get_msi_target_expdata[] = {
	RPMI_SUCCESS,
	TEST_SYSMSI_ADDR_LOW,
	TEST_SYSMSI_ADDR_HIGH,
	TEST_SYSMSI_DATA,
};

static rpmi_uint32_t set_msi_bad_target_reqdata[] = {
	TEST_SYSMSI_VALID_INDEX,
	TEST_SYSMSI_BAD_ADDR_LOW,
	0,
	TEST_SYSMSI_DATA,
};

static rpmi_uint32_t invalid_addr_expdata[] = {
	RPMI_ERR_INVALID_ADDR,
};

static rpmi_bool_t test_validate_msi_addr(void *priv, rpmi_uint64_t msi_addr)
{
	return msi_addr == TEST_SYSMSI_ADDR_LOW;
}

static rpmi_bool_t test_mmode_preferred(void *priv, rpmi_uint32_t msi_index)
{
	return msi_index == TEST_SYSMSI_VALID_INDEX;
}

static struct rpmi_sysmsi_platform_ops test_sysmsi_ops = {
	.validate_msi_addr = test_validate_msi_addr,
	.mmode_preferred = test_mmode_preferred,
};

static int test_sysmsi_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	grp = rpmi_service_group_sysmsi_create(TEST_SYSMSI_COUNT,
						 TEST_SYSMSI_VALID_INDEX,
						 &test_sysmsi_ops, NULL);
	if (!grp) {
		printf("failed to create rpmi sysmsi service group");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_sysmsi_default = {
	.name = "System MSI Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_sysmsi_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 10,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_ENABLE_NOTIFICATION,
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
			.name = "GET ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = get_attrs_expdata,
				.expected_data_len = sizeof(get_attrs_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_attrs_reqdata,
				.request_data_len = sizeof(get_msi_attrs_reqdata),
				.expected_data = get_msi_attrs_expdata,
				.expected_data_len = sizeof(get_msi_attrs_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI ATTRIBUTES (invalid index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_msi_reqdata,
				.request_data_len = sizeof(invalid_msi_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI STATE (enable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_state_reqdata,
				.request_data_len = sizeof(set_msi_state_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI STATE (enabled)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_state_reqdata,
				.request_data_len = sizeof(get_msi_state_reqdata),
				.expected_data = get_msi_state_expdata,
				.expected_data_len = sizeof(get_msi_state_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI STATE (invalid index)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_STATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_msi_reqdata,
				.request_data_len = sizeof(invalid_msi_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI TARGET",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_target_reqdata,
				.request_data_len = sizeof(set_msi_target_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET MSI TARGET",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_GET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_msi_state_reqdata,
				.request_data_len = sizeof(get_msi_state_reqdata),
				.expected_data = get_msi_target_expdata,
				.expected_data_len = sizeof(get_msi_target_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SET MSI TARGET (invalid address)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_MSI,
				.service_id = RPMI_SYSMSI_SRV_SET_MSI_TARGET,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_bad_target_reqdata,
				.request_data_len = sizeof(set_msi_bad_target_reqdata),
				.expected_data = invalid_addr_expdata,
				.expected_data_len = sizeof(invalid_addr_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test System MSI Service Group\n");
	return test_scenario_execute(&scenario_sysmsi_default);
}
