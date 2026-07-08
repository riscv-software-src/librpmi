// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 SiFive, Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_EVENT_ID			RPMI_REQFWD_EVENT_NEW_MESSAGE
#define TEST_REQUEST_STATE_DISABLE	0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_REQUEST_STATE_RETURN	0x2

/* Test request data */
static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE
};

static rpmi_uint32_t enable_notif_expdata[] = {
	RPMI_SUCCESS,
	TEST_REQUEST_STATE_ENABLE
};

static rpmi_uint32_t disable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_DISABLE
};

static rpmi_uint32_t disable_notif_expdata[] = {
	RPMI_SUCCESS,
	TEST_REQUEST_STATE_DISABLE
};

static rpmi_uint32_t query_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_RETURN
};

static rpmi_uint32_t query_notif_expdata[] = {
	RPMI_SUCCESS,
	TEST_REQUEST_STATE_DISABLE,  /* state = 0 after disable test */
};

static rpmi_uint32_t invalid_state_reqdata[] = {
	TEST_EVENT_ID,
	3,  /* invalid req_state */
};

static rpmi_uint32_t invalid_state_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t invalid_event_reqdata[] = {
	0xFF, /* Invalid event ID */
	TEST_REQUEST_STATE_ENABLE
};

static rpmi_uint32_t invalid_event_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

/* Mock forwarded message data */
static rpmi_uint8_t mock_forwarded_msg[] = {
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
};

struct test_reqfwd_scenario_config {
	enum rpmi_privilege_level privilege_level;
	struct rpmi_service_group *grp;
};

/* Mock state */
static rpmi_bool_t mock_has_message;
static rpmi_uint32_t mock_num_pending;

/* Platform callback: get current message */
static enum rpmi_error test_reqfwd_get_current_message(void *priv,
						       struct rpmi_reqfwd_message *msg)
{
	if (!mock_has_message)
		return RPMI_ERR_NO_DATA;

	msg->msg_size = sizeof(mock_forwarded_msg);
	msg->msg_data = mock_forwarded_msg;

	return RPMI_SUCCESS;
}

/* Platform callback: complete current message */
static enum rpmi_error test_reqfwd_complete_current_message(void *priv,
							    rpmi_uint32_t resp_data_len,
							    const rpmi_uint8_t *resp_data,
							    rpmi_uint32_t *num_messages)
{
	mock_has_message = 0;
	*num_messages = mock_num_pending;

	return RPMI_SUCCESS;
}

static struct rpmi_reqfwd_platform_ops test_reqfwd_ops = {
	.get_current_message = test_reqfwd_get_current_message,
	.complete_current_message = test_reqfwd_complete_current_message,
};

static rpmi_uint32_t retrieve_reqdata[] = {
	0,  /* start_index = 0 */
};

static rpmi_uint32_t retrieve_no_msg_expdata[] = {
	RPMI_ERR_NO_DATA,
};

static rpmi_uint32_t complete_msg_expdata[] = {
	RPMI_SUCCESS,
	0,  /* num_messages = mock_num_pending = 0 */
};

static rpmi_uint32_t complete_no_retrieve_expdata[] = {
	RPMI_ERR_NO_DATA,
};

static rpmi_uint32_t retrieve_reqdata_offset[] = {
	8,  /* start_index = 8 */
};

static rpmi_uint16_t test_retrieve_msg_offset_init_expected_data(struct rpmi_test_scenario *scene,
								 struct rpmi_test *test, void *data,
								 rpmi_uint16_t max_data_len)
{
	rpmi_uint32_t *exp = (rpmi_uint32_t *)data;
	rpmi_uint16_t offset = 8;
	rpmi_uint16_t returned = sizeof(mock_forwarded_msg) - offset;

	exp[0] = RPMI_SUCCESS;
	exp[1] = 0;  /* remaining */
	exp[2] = returned;
	rpmi_env_memcpy(&exp[3], mock_forwarded_msg + offset, returned);

	return 3 * sizeof(rpmi_uint32_t) + returned;
}

static int test_retrieve_no_msg_init(struct rpmi_test_scenario *scene,
				     struct rpmi_test *test)
{
	mock_has_message = 0;
	return 0;
}

static int test_retrieve_with_msg_init(struct rpmi_test_scenario *scene,
				       struct rpmi_test *test)
{
	mock_has_message = 1;
	return 0;
}

static rpmi_uint16_t test_retrieve_msg_init_expected_data(struct rpmi_test_scenario *scene,
							  struct rpmi_test *test, void *data,
							  rpmi_uint16_t max_data_len)
{
	rpmi_uint32_t *exp = (rpmi_uint32_t *)data;

	exp[0] = RPMI_SUCCESS;
	exp[1] = 0;  /* remaining */
	exp[2] = sizeof(mock_forwarded_msg);  /* returned */
	rpmi_env_memcpy(&exp[3], mock_forwarded_msg, sizeof(mock_forwarded_msg));

	return 3 * sizeof(rpmi_uint32_t) + sizeof(mock_forwarded_msg);
}

static int test_reqfwd_scenario_init(struct rpmi_test_scenario *scene)
{
	struct test_reqfwd_scenario_config *config;
	enum rpmi_privilege_level privilege_level;
	struct rpmi_service_group *grp;
	int ret;

	if (!scene)
		return RPMI_ERR_INVALID_PARAM;

	config = scene->priv;
	if (!config)
		return RPMI_ERR_INVALID_PARAM;

	if (scene->shm || scene->shmem || scene->xport || scene->cntx)
		return RPMI_ERR_ALREADY;

	scene->shm = rpmi_env_zalloc(scene->shm_size);
	if (!scene->shm)
		return RPMI_ERR_FAILED;

	scene->shmem = rpmi_shmem_create("test_shmem",
					 (unsigned long)scene->shm,
					 scene->shm_size,
					 &rpmi_shmem_simple_ops, NULL);
	if (!scene->shmem) {
		printf("%s: failed to create test rpmi_shmem\n ", __func__);
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
		printf("%s: failed to create test rpmi_transport\n ", __func__);
		rpmi_shmem_destroy(scene->shmem);
		scene->shmem = NULL;
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
		return RPMI_ERR_FAILED;
	}

	privilege_level = config->privilege_level;
	scene->cntx = rpmi_context_create("test_context", scene->xport,
					  scene->max_num_groups,
					  privilege_level,
					  scene->base.plat_info_len,
					  scene->base.plat_info);
	if (!scene->cntx) {
		printf("%s: failed to create test rpmi_context\n ", __func__);
		ret = RPMI_ERR_FAILED;
		goto fail_cleanup;
	}

	grp = rpmi_service_group_reqfwd_create(&test_reqfwd_ops, NULL, scene->xport);
	if (!grp) {
		printf("Failed to create Request Forward service group\n");
		ret = RPMI_ERR_FAILED;
		goto fail_cleanup;
	}

	ret = rpmi_context_add_group(scene->cntx, grp);
	if (ret) {
		printf("Failed to add Request Forward service group\n");
		rpmi_service_group_reqfwd_destroy(grp);
		goto fail_cleanup;
	}

	config->grp = grp;
	scene->token_sequence = 0;
	return 0;

fail_cleanup:
	test_scenario_default_cleanup(scene);
	return ret;
}

static int test_reqfwd_scenario_cleanup(struct rpmi_test_scenario *scene)
{
	struct test_reqfwd_scenario_config *config;

	if (!scene)
		return RPMI_ERR_INVALID_PARAM;

	config = scene->priv;
	if (config)
		config->grp = NULL;

	return test_scenario_default_cleanup(scene);
}

static rpmi_uint8_t test_notif_event_data[] = { 0xAB, 0xCD, 0xEF, 0x01 };

static int test_p2a_notif_run(struct rpmi_test_scenario *scene,
			      struct rpmi_test *test,
			      struct rpmi_message *msg)
{
	struct test_reqfwd_scenario_config *config = scene->priv;

	return rpmi_reqfwd_trigger_new_message(config->grp, 0, NULL);
}

static int test_p2a_notif_with_data_run(struct rpmi_test_scenario *scene,
					struct rpmi_test *test,
					struct rpmi_message *msg)
{
	struct test_reqfwd_scenario_config *config = scene->priv;

	return rpmi_reqfwd_trigger_new_message(config->grp,
					       sizeof(test_notif_event_data),
					       test_notif_event_data);
}

static rpmi_uint16_t test_notif_with_data_init_expected_data(struct rpmi_test_scenario *scene,
							     struct rpmi_test *test, void *data,
							     rpmi_uint16_t max_data_len)
{
	rpmi_uint32_t *exp_u32 = (rpmi_uint32_t *)data;
	rpmi_uint8_t *exp_u8 = (rpmi_uint8_t *)data;

	exp_u32[0] = ((rpmi_uint32_t)RPMI_REQFWD_EVENT_NEW_MESSAGE << 16) |
		     sizeof(test_notif_event_data);
	rpmi_env_memcpy(exp_u8 + sizeof(rpmi_uint32_t), test_notif_event_data,
			sizeof(test_notif_event_data));
	return sizeof(rpmi_uint32_t) + sizeof(test_notif_event_data);
}

static void test_p2a_notif_wait(struct rpmi_test_scenario *scene,
				struct rpmi_test *test,
				struct rpmi_message *msg)
{
	int result = -1;

	rpmi_env_memset(msg, 0, scene->slot_size);
	while (result != RPMI_SUCCESS)
		result = rpmi_transport_dequeue(scene->xport,
						RPMI_QUEUE_P2A_REQ, msg);
}

static rpmi_uint32_t notif_expdata[] = {
	(rpmi_uint32_t)RPMI_REQFWD_EVENT_NEW_MESSAGE << 16,
};

static rpmi_uint32_t reenable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE
};

static rpmi_uint32_t reenable_notif_expdata[] = {
	RPMI_SUCCESS,
	TEST_REQUEST_STATE_ENABLE
};

static struct test_reqfwd_scenario_config reqfwd_m_mode_config = {
	.privilege_level = RPMI_PRIVILEGE_M_MODE,
};

static struct test_reqfwd_scenario_config reqfwd_s_mode_config = {
	.privilege_level = RPMI_PRIVILEGE_S_MODE,
};

static int test_reqfwd_group_allows_s_mode(void)
{
	struct rpmi_transport dummy_xport = { .slot_size = RPMI_SLOT_SIZE };
	struct rpmi_service_group *grp;
	int ret = 0;

	grp = rpmi_service_group_reqfwd_create(&test_reqfwd_ops, NULL,
					       &dummy_xport);
	if (!grp) {
		printf("%s: failed to create group\n", __func__);
		return RPMI_ERR_FAILED;
	}

	if (!(grp->privilege_level_bitmap & RPMI_PRIVILEGE_S_MODE_MASK)) {
		printf("%s: FAIL - S-mode not allowed\n", __func__);
		ret = RPMI_ERR_FAILED;
	} else {
		printf("%s: PASS\n", __func__);
	}

	rpmi_service_group_reqfwd_destroy(grp);
	return ret;
}

static struct rpmi_test_scenario scenario_reqfwd_default = {
	.name = "Request Forward Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = &reqfwd_m_mode_config,

	.init = test_reqfwd_scenario_init,
	.cleanup = test_reqfwd_scenario_cleanup,

	.num_tests = 13,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (enable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
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
			.name = "ENABLE NOTIFICATION TEST (disable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = disable_notif_reqdata,
				.request_data_len = sizeof(disable_notif_reqdata),
				.expected_data = disable_notif_expdata,
				.expected_data_len = sizeof(disable_notif_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "ENABLE NOTIFICATION TEST (query state)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = query_notif_reqdata,
				.request_data_len = sizeof(query_notif_reqdata),
				.expected_data = query_notif_expdata,
				.expected_data_len = sizeof(query_notif_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "ENABLE NOTIFICATION TEST (invalid state)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_state_reqdata,
				.request_data_len = sizeof(invalid_state_reqdata),
				.expected_data = invalid_state_expdata,
				.expected_data_len = sizeof(invalid_state_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "ENABLE NOTIFICATION TEST (invalid event)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_event_reqdata,
				.request_data_len = sizeof(invalid_event_reqdata),
				.expected_data = invalid_event_expdata,
				.expected_data_len = sizeof(invalid_event_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RETRIEVE CURRENT MESSAGE TEST (no message available)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_RETRIEVE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = retrieve_reqdata,
				.request_data_len = sizeof(retrieve_reqdata),
				.expected_data = retrieve_no_msg_expdata,
				.expected_data_len = sizeof(retrieve_no_msg_expdata),
			},
			.init = test_retrieve_no_msg_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RETRIEVE CURRENT MESSAGE TEST (message, start_index=0)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_RETRIEVE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = retrieve_reqdata,
				.request_data_len = sizeof(retrieve_reqdata),
			},
			.init = test_retrieve_with_msg_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_retrieve_msg_init_expected_data,
		},
		{
			.name = "RETRIEVE CURRENT MESSAGE TEST (message, start_index=8)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_RETRIEVE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = retrieve_reqdata_offset,
				.request_data_len = sizeof(retrieve_reqdata_offset),
			},
			.init = test_retrieve_with_msg_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_retrieve_msg_offset_init_expected_data,
		},
		{
			.name = "COMPLETE CURRENT MESSAGE TEST (after retrieve)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_COMPLETE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = complete_msg_expdata,
				.expected_data_len = sizeof(complete_msg_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "COMPLETE CURRENT MESSAGE TEST (without prior retrieve)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_COMPLETE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = complete_no_retrieve_expdata,
				.expected_data_len = sizeof(complete_no_retrieve_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "NOTIFICATION: re-enable for delivery test",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = reenable_notif_reqdata,
				.request_data_len = sizeof(reenable_notif_reqdata),
				.expected_data = reenable_notif_expdata,
				.expected_data_len = sizeof(reenable_notif_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "NOTIFICATION: trigger new_message and verify P2A",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.flags = RPMI_MSG_POSTED_REQUEST,
				.expected_data = notif_expdata,
				.expected_data_len = sizeof(notif_expdata),
			},
			.run = test_p2a_notif_run,
			.wait = test_p2a_notif_wait,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "NOTIFICATION: trigger with event data, P2A",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.flags = RPMI_MSG_POSTED_REQUEST,
			},
			.run = test_p2a_notif_with_data_run,
			.wait = test_p2a_notif_wait,
			.init_expected_data = test_notif_with_data_init_expected_data,
		},
	},
};

static struct rpmi_test_scenario scenario_reqfwd_s_mode = {
	.name = "Request Forward Service Group (S-mode)",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = &reqfwd_s_mode_config,

	.init = test_reqfwd_scenario_init,
	.cleanup = test_reqfwd_scenario_cleanup,

	.num_tests = 6,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (S-mode, enable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
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
			.name = "ENABLE NOTIFICATION TEST (S-mode, disable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = disable_notif_reqdata,
				.request_data_len = sizeof(disable_notif_reqdata),
				.expected_data = disable_notif_expdata,
				.expected_data_len = sizeof(disable_notif_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RETRIEVE CURRENT MESSAGE TEST (S-mode, no message)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_RETRIEVE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = retrieve_reqdata,
				.request_data_len = sizeof(retrieve_reqdata),
				.expected_data = retrieve_no_msg_expdata,
				.expected_data_len = sizeof(retrieve_no_msg_expdata),
			},
			.init = test_retrieve_no_msg_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RETRIEVE CURRENT MSG TEST (S-mode, message)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_RETRIEVE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = retrieve_reqdata,
				.request_data_len = sizeof(retrieve_reqdata),
			},
			.init = test_retrieve_with_msg_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_retrieve_msg_init_expected_data,
		},
		{
			.name = "COMPLETE CURRENT MSG TEST (S-mode, after retrieve)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_COMPLETE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = complete_msg_expdata,
				.expected_data_len = sizeof(complete_msg_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "COMPLETE CURRENT MSG TEST (S-mode, no retrieve)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_REQUEST_FORWARD,
				.service_id = RPMI_REQFWD_SRV_COMPLETE_CURRENT_MESSAGE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = complete_no_retrieve_expdata,
				.expected_data_len = sizeof(complete_no_retrieve_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	int ret;

	printf("Test Request Forward Service Group\n");

	ret = test_reqfwd_group_allows_s_mode();
	if (ret)
		return ret;

	ret = test_scenario_execute(&scenario_reqfwd_default);
	ret |= test_scenario_execute(&scenario_reqfwd_s_mode);

	return ret;
}
