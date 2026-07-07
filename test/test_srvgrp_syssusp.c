/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_SUSPEND_SUPPORTED		0x1
#define TEST_SUSPEND_NOT_SUPPORTED	0x0

#define TEST_EVENT_ID			0x0

#define TEST_CURRENT_STATE_DISABLED	0x0
#define TEST_CURRENT_STATE_ENABLED	0x1

#define TEST_REQUEST_STATE_DISABLE	0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_REQUEST_STATE_RETURN	0x2

#define TEST_HART_ID			0x0

static const struct rpmi_system_suspend_type test_rpmi_suspend_types[] = {
	{
		.type = RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM,
		.attr = RPMI_SYSSUSP_ATTRS_FLAGS_RESUMEADDR,
	}
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
	RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM,
};

static rpmi_uint32_t get_attrs_supp_expdata[] = {
	RPMI_SUCCESS,
	RPMI_SYSSUSP_ATTRS_FLAGS_SUSPENDTYPE | RPMI_SYSSUSP_ATTRS_FLAGS_RESUMEADDR
};

static rpmi_uint32_t get_attrs_notsupp_reqdata[] = {
	RPMI_SYSSUSP_TYPE_MAX,
};

static rpmi_uint32_t get_attrs_notsupp_expdata[] = {
	RPMI_SUCCESS,
	TEST_SUSPEND_NOT_SUPPORTED
};

static rpmi_uint32_t suspend_reqdata_supp[] = {
	TEST_HART_ID,              /* hart_id */
	RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM,  /* suspend_type */
	0x80000000,                /* resume_addr low */
	0x00000000,                /* resume_addr high */
};

static rpmi_uint32_t suspend_reqdata_unsupported[] = {
	TEST_HART_ID,              /* hart_id */
	RPMI_SYSSUSP_TYPE_MAX,     /* unsupported suspend_type */
	0x80000000,                /* resume_addr low */
	0x00000000,                /* resume_addr high */
};

static rpmi_uint32_t suspend_supp_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t suspend_unsupported_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t test_suspend_prepare_count;
static rpmi_uint32_t test_suspend_finalize_count;
static rpmi_uint32_t test_suspend_resume_count;
static rpmi_uint32_t test_suspend_last_type = RPMI_SYSSUSP_TYPE_MAX;

/* Minimal HSM platform operations for testing */
static enum rpmi_hart_hw_state test_hart_get_hw_state(void *priv, rpmi_uint32_t hart_index)
{
	return RPMI_HART_HW_STATE_STARTED;
}

static struct rpmi_hsm_platform_ops test_hsm_ops = {
	.hart_get_hw_state = test_hart_get_hw_state,
	.hart_start_prepare = NULL,
	.hart_start_finalize = NULL,
	.hart_stop_prepare = NULL,
	.hart_stop_finalize = NULL,
	.hart_suspend_prepare = NULL,
	.hart_suspend_finalize = NULL
};

static enum rpmi_error rpmi_do_system_suspend_prepare(void *priv, rpmi_uint32_t hart_index,
						      const struct rpmi_system_suspend_type *syssusp_type,
						      rpmi_uint64_t resume_addr)
{
	test_suspend_prepare_count++;
	test_suspend_last_type = syssusp_type->type;
	printf("platform callback: suspend prepare (type=%d)\n", syssusp_type->type);
	return RPMI_SUCCESS;
}

static void rpmi_do_system_suspend_finalize(void *priv, rpmi_uint32_t hart_index,
					    const struct rpmi_system_suspend_type *syssusp_type,
					    rpmi_uint64_t resume_addr)
{
	test_suspend_finalize_count++;
	printf("platform callback: suspend finalize\n");
}

static rpmi_bool_t rpmi_system_suspend_ready(void *priv, rpmi_uint32_t hart_index)
{
	return true;
}

static rpmi_bool_t rpmi_system_suspend_can_resume(void *priv, rpmi_uint32_t hart_index)
{
	return false;  /* For testing, we don't auto-resume */
}

static enum rpmi_error rpmi_do_system_suspend_resume(void *priv, rpmi_uint32_t hart_index,
						     const struct rpmi_system_suspend_type *syssusp_type,
						     rpmi_uint64_t resume_addr)
{
	test_suspend_resume_count++;
	printf("platform callback: suspend resume\n");
	return RPMI_SUCCESS;
}

struct rpmi_syssusp_platform_ops rpmi_suspend_ops = {
	.system_suspend_prepare = rpmi_do_system_suspend_prepare,
	.system_suspend_finalize = rpmi_do_system_suspend_finalize,
	.system_suspend_ready = rpmi_system_suspend_ready,
	.system_suspend_can_resume = rpmi_system_suspend_can_resume,
	.system_suspend_resume = rpmi_do_system_suspend_resume
};

static int test_suspend_callback_init(struct rpmi_test_scenario *scene,
				      struct rpmi_test *test)
{
	test_suspend_prepare_count = 0;
	test_suspend_finalize_count = 0;
	test_suspend_resume_count = 0;
	test_suspend_last_type = RPMI_SYSSUSP_TYPE_MAX;
	return 0;
}

static int test_suspend_callback_verify(struct rpmi_test_scenario *scene,
					struct rpmi_test *test,
					struct rpmi_message *msg)
{
	rpmi_uint32_t expected_type = *(rpmi_uint32_t *)test->priv;

	if (test_suspend_prepare_count != 1) {
		printf("%s: expected one suspend prepare callback, got %d\n",
		       test->name, test_suspend_prepare_count);
		return 1;
	}

	if (test_suspend_last_type != expected_type) {
		printf("%s: expected suspend type %d, got %d\n",
		       test->name, expected_type, test_suspend_last_type);
		return 1;
	}

	return 0;
}

static int test_no_suspend_callback_verify(struct rpmi_test_scenario *scene,
					   struct rpmi_test *test,
					   struct rpmi_message *msg)
{
	if (test_suspend_prepare_count) {
		printf("%s: unexpected suspend prepare callback count %d, last type %d\n",
		       test->name, test_suspend_prepare_count, test_suspend_last_type);
		return 1;
	}

	return 0;
}

static int test_syssusp_scenario_init(struct rpmi_test_scenario *scene)
{
	int ret;
	struct rpmi_service_group *grp;
	struct rpmi_hsm *hsm_cntx;
	rpmi_uint32_t test_hartid_array[1] = { TEST_HART_ID };

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	/* Create a minimal HSM instance for testing */
	hsm_cntx = rpmi_hsm_create(1, test_hartid_array, 0, NULL, &test_hsm_ops, NULL);
	if (!hsm_cntx) {
		printf("failed to create rpmi hsm\n");
		return RPMI_ERR_FAILED;
	}

	grp = rpmi_service_group_syssusp_create(
				hsm_cntx,
				sizeof(test_rpmi_suspend_types) / sizeof(struct rpmi_system_suspend_type),
				test_rpmi_suspend_types,
				&rpmi_suspend_ops, NULL);
	if (!grp) {
		printf("failed to create syssusp service group\n");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static rpmi_uint32_t suspend_type_s2ram = RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM;

static struct rpmi_test_scenario scenario_syssusp_default = {
	.name = "System Suspend Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_syssusp_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 6,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND,
				.service_id = RPMI_SYSSUSP_SRV_ENABLE_NOTIFICATION,
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
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND,
				.service_id = RPMI_SYSSUSP_SRV_ENABLE_NOTIFICATION,
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
			.name = "GET ATTRIBUTES TEST (for supported suspend type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND,
				.service_id = RPMI_SYSSUSP_SRV_GET_ATTRIBUTES,
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
			.name = "GET ATTRIBUTES TEST (for unsupported suspend type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND,
				.service_id = RPMI_SYSSUSP_SRV_GET_ATTRIBUTES,
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
			.name = "SYSTEM SUSPEND (with supported suspend type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND,
				.service_id = RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = suspend_reqdata_supp,
				.request_data_len = sizeof(suspend_reqdata_supp),
				.expected_data = suspend_supp_expdata,
				.expected_data_len = sizeof(suspend_supp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.init = test_suspend_callback_init,
			.verify = test_suspend_callback_verify,
			.priv = &suspend_type_s2ram,
		},
		{
			.name = "SYSTEM SUSPEND (unsupported suspend type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND,
				.service_id = RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = suspend_reqdata_unsupported,
				.request_data_len = sizeof(suspend_reqdata_unsupported),
				.expected_data = suspend_unsupported_expdata,
				.expected_data_len = sizeof(suspend_unsupported_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.init = test_suspend_callback_init,
			.verify = test_no_suspend_callback_verify,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test System Suspend Service Group\n");
	return test_scenario_execute(&scenario_syssusp_default);;
}
