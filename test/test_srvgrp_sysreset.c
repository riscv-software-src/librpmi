/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_RESET_SUPPORTED		0x1
#define TEST_RESET_NOT_SUPPORTED	0x0

#define TEST_EVENT_ID			0x0

#define TEST_CURRENT_STATE_DISABLED	0x0
#define TEST_CURRENT_STATE_ENABLED	0x1

#define TEST_REQUEST_STATE_DISABLE	0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_REQUEST_STATE_RETURN	0x2

static const rpmi_uint32_t test_rpmi_reset_types[] = {
	RPMI_SYSRST_TYPE_SHUTDOWN,
	RPMI_SYSRST_TYPE_COLD_REBOOT
};

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE
};

static rpmi_uint32_t enable_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t status_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_RETURN
};

static rpmi_uint32_t status_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t get_attrs_supp_reqdata[] = {
	RPMI_SYSRST_TYPE_COLD_REBOOT,
};

static rpmi_uint32_t get_attrs_supp_expdata[] = {
	RPMI_SUCCESS,
	TEST_RESET_SUPPORTED
};

static rpmi_uint32_t get_attrs_notsupp_reqdata[] = {
	RPMI_SYSRST_TYPE_MAX,
};

static rpmi_uint32_t get_attrs_notsupp_expdata[] = {
	RPMI_SUCCESS,
	TEST_RESET_NOT_SUPPORTED
};

static rpmi_uint32_t reset_reqdata_supp[] = {
	RPMI_SYSRST_TYPE_COLD_REBOOT,
};

static rpmi_uint32_t reset_reqdata_shutdown[] = {
	RPMI_SYSRST_TYPE_SHUTDOWN,
};

static rpmi_uint32_t reset_reqdata_unsupported[] = {
	RPMI_SYSRST_TYPE_WARM_REBOOT,
};

static rpmi_uint32_t reset_type_cold = RPMI_SYSRST_TYPE_COLD_REBOOT;
static rpmi_uint32_t reset_type_shutdown = RPMI_SYSRST_TYPE_SHUTDOWN;

static rpmi_uint32_t test_reset_call_count;
static rpmi_uint32_t test_reset_last_type = RPMI_SYSRST_TYPE_MAX;

static void rpmi_do_system_reset(void *priv, rpmi_uint32_t reset_type)
{
	test_reset_call_count++;
	test_reset_last_type = reset_type;
	if (reset_type == RPMI_SYSRST_TYPE_WARM_REBOOT) {
		printf("platform callback: warm reset\n");
	} else if (reset_type == RPMI_SYSRST_TYPE_COLD_REBOOT) {
		printf("platform callback: cold reset\n");
	} else if (reset_type == RPMI_SYSRST_TYPE_SHUTDOWN) {
		printf("platform callback: shutdown\n");
	} else {
		return;
	}

	return;
}

struct rpmi_sysreset_platform_ops rpmi_reset_ops = {
	.do_system_reset = rpmi_do_system_reset
};

static int test_reset_callback_init(struct rpmi_test_scenario *scene,
				    struct rpmi_test *test)
{
	test_reset_call_count = 0;
	test_reset_last_type = RPMI_SYSRST_TYPE_MAX;
	return 0;
}

static int test_reset_callback_verify(struct rpmi_test_scenario *scene,
				      struct rpmi_test *test,
				      struct rpmi_message *msg)
{
	rpmi_uint32_t expected_type = *(rpmi_uint32_t *)test->priv;

	if (test_reset_call_count != 1) {
		printf("%s: expected one reset callback, got %d\n",
		       test->name, test_reset_call_count);
		return 1;
	}

	if (test_reset_last_type != expected_type) {
		printf("%s: expected reset type %d, got %d\n",
		       test->name, expected_type, test_reset_last_type);
		return 1;
	}

	return 0;
}

static int test_no_reset_callback_verify(struct rpmi_test_scenario *scene,
					 struct rpmi_test *test,
					 struct rpmi_message *msg)
{
	if (test_reset_call_count) {
		printf("%s: unexpected reset callback count %d, last type %d\n",
		       test->name, test_reset_call_count, test_reset_last_type);
		return 1;
	}

	return 0;
}

static int test_sysreset_scenario_init(struct rpmi_test_scenario *scene)
{
	int ret;
	struct rpmi_service_group *grp;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	grp = rpmi_service_group_sysreset_create(
				sizeof(test_rpmi_reset_types) / sizeof(rpmi_uint32_t),
				(const rpmi_uint32_t *)&test_rpmi_reset_types,
				&rpmi_reset_ops, NULL);
	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_sysreset_default = {
	.name = "System Reset Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_sysreset_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 7,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET,
				.service_id = RPMI_SYSRST_SRV_ENABLE_NOTIFICATION,
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
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET,
				.service_id = RPMI_SYSRST_SRV_ENABLE_NOTIFICATION,
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
			.name = "GET ATTRIBUTES TEST (for supported reset type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET,
				.service_id = RPMI_SYSRST_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_attrs_supp_reqdata,
				.request_data_len = sizeof(get_attrs_supp_reqdata),
				.expected_data = get_attrs_supp_expdata,
				.expected_data_len = sizeof(get_attrs_supp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES TEST (for unsupported reset type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET,
				.service_id = RPMI_SYSRST_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = get_attrs_notsupp_reqdata,
				.request_data_len = sizeof(get_attrs_notsupp_reqdata),
				.expected_data = get_attrs_notsupp_expdata,
				.expected_data_len = sizeof(get_attrs_notsupp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "SYSTEM RESET (with supported reset type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET,
				.service_id = RPMI_SYSRST_SRV_SYSTEM_RESET,
				.flags = RPMI_MSG_POSTED_REQUEST,
				.request_data = reset_reqdata_supp,
				.request_data_len = sizeof(reset_reqdata_supp),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init = test_reset_callback_init,
			.verify = test_reset_callback_verify,
			.priv = &reset_type_cold,
		},
		{
			.name = "SYSTEM RESET (shutdown reset type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET,
				.service_id = RPMI_SYSRST_SRV_SYSTEM_RESET,
				.flags = RPMI_MSG_POSTED_REQUEST,
				.request_data = reset_reqdata_shutdown,
				.request_data_len = sizeof(reset_reqdata_shutdown),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init = test_reset_callback_init,
			.verify = test_reset_callback_verify,
			.priv = &reset_type_shutdown,
		},
		{
			.name = "SYSTEM RESET (unsupported reset type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_RESET,
				.service_id = RPMI_SYSRST_SRV_SYSTEM_RESET,
				.flags = RPMI_MSG_POSTED_REQUEST,
				.request_data = reset_reqdata_unsupported,
				.request_data_len = sizeof(reset_reqdata_unsupported),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init = test_reset_callback_init,
			.verify = test_no_reset_callback_verify,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test System Reset Service Group\n");
	return test_scenario_execute(&scenario_sysreset_default);;
}
