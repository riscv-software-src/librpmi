// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_HART_ID		0
#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_RESUME_ADDR_LOW	0x80000000
#define TEST_RESUME_ADDR_HIGH	0x0

static rpmi_uint32_t test_hartid_array[] = {
	TEST_HART_ID,
};

static struct rpmi_system_suspend_type test_syssusp_types[] = {
	{
		.type = RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM,
		.attr = RPMI_SYSSUSP_ATTRS_FLAGS_RESUMEADDR,
	},
};

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE,
};

static rpmi_uint32_t enable_notif_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t get_attrs_supp_reqdata[] = {
	RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM,
};

static rpmi_uint32_t get_attrs_supp_expdata[] = {
	RPMI_SUCCESS,
	RPMI_SYSSUSP_ATTRS_FLAGS_SUSPENDTYPE | RPMI_SYSSUSP_ATTRS_FLAGS_RESUMEADDR,
};

static rpmi_uint32_t get_attrs_notsupp_reqdata[] = {
	RPMI_SYSSUSP_TYPE_MAX,
};

static rpmi_uint32_t get_attrs_notsupp_expdata[] = {
	RPMI_SUCCESS,
	0,
};

static rpmi_uint32_t suspend_reqdata[] = {
	TEST_HART_ID,
	RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM,
	TEST_RESUME_ADDR_LOW,
	TEST_RESUME_ADDR_HIGH,
};

static rpmi_uint32_t suspend_invalid_type_reqdata[] = {
	TEST_HART_ID,
	RPMI_SYSSUSP_TYPE_MAX,
	TEST_RESUME_ADDR_LOW,
	TEST_RESUME_ADDR_HIGH,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t prepare_count;
static rpmi_uint32_t finalize_count;
static rpmi_uint32_t resume_count;
static rpmi_uint32_t last_hart_index;
static rpmi_uint32_t last_suspend_type;
static rpmi_uint64_t last_resume_addr;

static enum rpmi_hart_hw_state test_hart_get_hw_state(void *priv,
						       rpmi_uint32_t hart_index)
{
	return RPMI_HART_HW_STATE_STARTED;
}

static struct rpmi_hsm_platform_ops test_hsm_ops = {
	.hart_get_hw_state = test_hart_get_hw_state,
};

static enum rpmi_error
test_system_suspend_prepare(void *priv, rpmi_uint32_t hart_index,
			    const struct rpmi_system_suspend_type *syssusp_type,
			    rpmi_uint64_t resume_addr)
{
	prepare_count++;
	last_hart_index = hart_index;
	last_suspend_type = syssusp_type->type;
	last_resume_addr = resume_addr;
	return RPMI_SUCCESS;
}

static rpmi_bool_t test_system_suspend_ready(void *priv, rpmi_uint32_t hart_index)
{
	return true;
}

static void test_system_suspend_finalize(void *priv, rpmi_uint32_t hart_index,
					 const struct rpmi_system_suspend_type *syssusp_type,
					 rpmi_uint64_t resume_addr)
{
	finalize_count++;
	last_hart_index = hart_index;
	last_suspend_type = syssusp_type->type;
	last_resume_addr = resume_addr;
}

static rpmi_bool_t test_system_suspend_can_resume(void *priv, rpmi_uint32_t hart_index)
{
	return true;
}

static enum rpmi_error
test_system_suspend_resume(void *priv, rpmi_uint32_t hart_index,
			   const struct rpmi_system_suspend_type *syssusp_type,
			   rpmi_uint64_t resume_addr)
{
	resume_count++;
	return RPMI_SUCCESS;
}

static struct rpmi_syssusp_platform_ops test_syssusp_ops = {
	.system_suspend_prepare = test_system_suspend_prepare,
	.system_suspend_ready = test_system_suspend_ready,
	.system_suspend_finalize = test_system_suspend_finalize,
	.system_suspend_can_resume = test_system_suspend_can_resume,
	.system_suspend_resume = test_system_suspend_resume,
};

static int test_suspend_callback_init(struct rpmi_test_scenario *scene,
				      struct rpmi_test *test)
{
	prepare_count = 0;
	finalize_count = 0;
	resume_count = 0;
	last_hart_index = -1U;
	last_suspend_type = RPMI_SYSSUSP_TYPE_MAX;
	last_resume_addr = 0;
	return 0;
}

static int test_suspend_callback_verify(struct rpmi_test_scenario *scene,
					struct rpmi_test *test,
					struct rpmi_message *msg)
{
	rpmi_uint64_t expected_resume_addr = TEST_RESUME_ADDR_LOW;

	if (prepare_count != 1 || finalize_count != 1) {
		printf("%s: expected prepare/finalize once, got %d/%d\n",
		       test->name, prepare_count, finalize_count);
		return 1;
	}

	if (last_hart_index != 0 || last_suspend_type != RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM ||
	    last_resume_addr != expected_resume_addr) {
		printf("%s: unexpected callback args hart=%d type=%d resume=0x%lx\n",
		       test->name, last_hart_index, last_suspend_type,
		       (unsigned long)last_resume_addr);
		return 1;
	}

	return 0;
}

static int test_syssusp_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
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

	grp = rpmi_service_group_syssusp_create(hsm,
						  ARRAY_SIZE(test_syssusp_types),
						  test_syssusp_types,
						  &test_syssusp_ops, NULL);
	if (!grp) {
		printf("failed to create rpmi syssusp service group");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_syssusp_default = {
	.name = "System Suspend Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_syssusp_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 5,
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
			.name = "SYSTEM SUSPEND (valid suspend type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND,
				.service_id = RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = suspend_reqdata,
				.request_data_len = sizeof(suspend_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.init = test_suspend_callback_init,
			.verify = test_suspend_callback_verify,
		},
		{
			.name = "SYSTEM SUSPEND (invalid suspend type)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_SYSTEM_SUSPEND,
				.service_id = RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = suspend_invalid_type_reqdata,
				.request_data_len = sizeof(suspend_invalid_type_reqdata),
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
	printf("Test System Suspend Service Group\n");
	return test_scenario_execute(&scenario_syssusp_default);
}
