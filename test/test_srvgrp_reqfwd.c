/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

/* Mock state */
static rpmi_bool_t mock_has_message = 0;
static rpmi_uint32_t mock_num_pending = 0;

/* Platform callback: get current message */
static enum rpmi_error test_reqfwd_get_current_message(void *priv,
						       struct rpmi_reqfwd_message *msg)
{
	if (!mock_has_message)
		return RPMI_ERR_NO_DATA;

	msg->msg_size = sizeof(mock_forwarded_msg);
	msg->msg_data = mock_forwarded_msg;

	printf("platform callback: returning forwarded message (%d bytes)\n",
	       msg->msg_size);

	return RPMI_SUCCESS;
}

/* Platform callback: complete current message */
static enum rpmi_error test_reqfwd_complete_current_message(void *priv,
							    rpmi_uint32_t resp_data_len,
							    const rpmi_uint8_t *resp_data,
							    rpmi_uint32_t *num_messages)
{
	printf("platform callback: completing message with %d bytes response\n",
	       resp_data_len);

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

static rpmi_uint16_t test_retrieve_msg_offset_init_expected_data(
	struct rpmi_test_scenario *scene, struct rpmi_test *test,
	void *data, rpmi_uint16_t max_data_len)
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

static rpmi_uint16_t test_retrieve_msg_init_expected_data(
	struct rpmi_test_scenario *scene, struct rpmi_test *test,
	void *data, rpmi_uint16_t max_data_len)
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
	int ret;
	struct rpmi_service_group *grp;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	grp = rpmi_service_group_reqfwd_create(&test_reqfwd_ops, NULL);
	if (!grp) {
		printf("Failed to create Request Forward service group\n");
		return RPMI_ERR_FAILED;
	}
	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_reqfwd_default = {
	.name = "Request Forward Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_reqfwd_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 10,
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
			.name = "RETRIEVE CURRENT MESSAGE TEST (message available, start_index=0)",
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
			.name = "RETRIEVE CURRENT MESSAGE TEST (message available, start_index=8)",
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
	},
};

int main(int argc, char *argv[])
{
	printf("Test Request Forward Service Group\n");
	return test_scenario_execute(&scenario_reqfwd_default);
}

