// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Qualcomm Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_LOG_TYPE			0x5a
#define TEST_LOG_TYPE_DENIED		0xd0
#define TEST_LOG_WORD0			0x11223344
#define TEST_LOG_WORD1			0x55667788
#define TEST_LOG_MAX_WORDS		4
#define TEST_EVENT_ID			0x0
#define TEST_REQUEST_STATE_ENABLE	0x1

struct test_logging_capture {
	rpmi_uint32_t call_count;
	rpmi_uint32_t log_type;
	rpmi_uint32_t num_words;
	rpmi_uint32_t words[TEST_LOG_MAX_WORDS];
};

struct test_logging_expect {
	rpmi_uint32_t call_count;
	rpmi_uint32_t log_type;
	rpmi_uint32_t num_words;
	const rpmi_uint32_t *words;
};

static struct test_logging_capture test_logging_capture;

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE,
};

static rpmi_uint32_t log_data_reqdata[] = {
	TEST_LOG_TYPE,
	2,
	TEST_LOG_WORD0,
	TEST_LOG_WORD1,
};

static rpmi_uint32_t log_data_zero_words_reqdata[] = {
	TEST_LOG_TYPE,
	0,
};

static rpmi_uint32_t log_data_bad_count_reqdata[] = {
	TEST_LOG_TYPE,
	3,
	TEST_LOG_WORD0,
	TEST_LOG_WORD1,
};

static rpmi_uint8_t log_data_unaligned_reqdata[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

static rpmi_uint32_t log_data_denied_reqdata[] = {
	TEST_LOG_TYPE_DENIED,
	0,
};

static rpmi_uint32_t logged_words[] = {
	TEST_LOG_WORD0,
	TEST_LOG_WORD1,
};

static rpmi_uint32_t notsupp_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t success_expdata[] = {
	RPMI_SUCCESS,
};

static rpmi_uint32_t invalid_param_expdata[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t denied_expdata[] = {
	RPMI_ERR_DENIED,
};

static const struct test_logging_expect expect_no_call = {
	.call_count = 0,
};

static const struct test_logging_expect expect_log_data = {
	.call_count = 1,
	.log_type = TEST_LOG_TYPE,
	.num_words = ARRAY_SIZE(logged_words),
	.words = logged_words,
};

static const struct test_logging_expect expect_log_data_zero_words = {
	.call_count = 1,
	.log_type = TEST_LOG_TYPE,
	.num_words = 0,
};

static const struct test_logging_expect expect_denied = {
	.call_count = 1,
	.log_type = TEST_LOG_TYPE_DENIED,
	.num_words = 0,
};

static enum rpmi_error test_logging_log_data(void *priv,
					     rpmi_uint32_t log_type,
					     rpmi_uint32_t num_words,
					     const rpmi_uint32_t *words)
{
	struct test_logging_capture *capture = priv;
	rpmi_uint32_t i;

	if (!capture || num_words > TEST_LOG_MAX_WORDS)
		return RPMI_ERR_INVALID_PARAM;

	capture->call_count++;
	capture->log_type = log_type;
	capture->num_words = num_words;
	rpmi_env_memset(capture->words, 0, sizeof(capture->words));

	for (i = 0; i < num_words; i++) {
		if (!words)
			return RPMI_ERR_INVALID_PARAM;
		capture->words[i] = words[i];
	}

	if (log_type == TEST_LOG_TYPE_DENIED)
		return RPMI_ERR_DENIED;

	return RPMI_SUCCESS;
}

static struct rpmi_logging_platform_ops test_logging_ops = {
	.log_data = test_logging_log_data,
};

static int test_logging_test_init(struct rpmi_test_scenario *scene,
				  struct rpmi_test *test)
{
	rpmi_env_memset(&test_logging_capture, 0, sizeof(test_logging_capture));
	return 0;
}

static int test_logging_verify_platform(struct rpmi_test_scenario *scene,
					struct rpmi_test *test,
					struct rpmi_message *msg)
{
	const struct test_logging_expect *expect = test->priv;
	rpmi_uint32_t i;
	int failed = 0;

	if (!expect)
		return 0;

	if (test_logging_capture.call_count != expect->call_count) {
		printf("%s: log callback count mismatch: expected: %u, got: %u\n",
		       test->name, expect->call_count,
		       test_logging_capture.call_count);
		failed = 1;
	}

	if (!expect->call_count)
		return failed;

	if (test_logging_capture.log_type != expect->log_type) {
		printf("%s: log type mismatch: expected: %u, got: %u\n",
		       test->name, expect->log_type,
		       test_logging_capture.log_type);
		failed = 1;
	}

	if (test_logging_capture.num_words != expect->num_words) {
		printf("%s: log word count mismatch: expected: %u, got: %u\n",
		       test->name, expect->num_words,
		       test_logging_capture.num_words);
		failed = 1;
	}

	for (i = 0; i < expect->num_words; i++) {
		if (test_logging_capture.words[i] == expect->words[i])
			continue;

		printf("%s: log word %u mismatch: expected: 0x%x, got: 0x%x\n",
		       test->name, i, expect->words[i],
		       test_logging_capture.words[i]);
		failed = 1;
	}

	return failed;
}

static int test_logging_group_allows_s_mode(void)
{
	struct rpmi_service_group *grp;
	int ret = 0;

	grp = rpmi_service_group_logging_create(&test_logging_ops,
						&test_logging_capture);
	if (!grp) {
		printf("failed to create rpmi logging service group\n");
		return 1;
	}

	if (!(grp->privilege_level_bitmap & RPMI_PRIVILEGE_S_MODE_MASK)) {
		printf("logging service group does not allow S-mode access\n");
		ret = 1;
	}

	rpmi_service_group_logging_destroy(grp);
	return ret;
}

static int test_logging_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	grp = rpmi_service_group_logging_create(&test_logging_ops,
						&test_logging_capture);
	if (!grp) {
		printf("failed to create rpmi logging service group\n");
		return RPMI_ERR_FAILED;
	}

	ret = rpmi_context_add_group(scene->cntx, grp);
	if (ret) {
		printf("failed to add rpmi logging service group\n");
		rpmi_service_group_logging_destroy(grp);
		return RPMI_ERR_FAILED;
	}

	return 0;
}

static struct rpmi_test_scenario scenario_logging_default = {
	.name = "Logging Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_logging_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 7,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = enable_notif_reqdata,
				.request_data_len = sizeof(enable_notif_reqdata),
				.expected_data = notsupp_expdata,
				.expected_data_len = sizeof(notsupp_expdata),
			},
			.priv = (void *)&expect_no_call,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = test_logging_verify_platform,
		},
		{
			.name = "LOG DATA TEST",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = log_data_reqdata,
				.request_data_len = sizeof(log_data_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.priv = (void *)&expect_log_data,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = test_logging_verify_platform,
		},
		{
			.name = "LOG DATA TEST (zero words)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = log_data_zero_words_reqdata,
				.request_data_len = sizeof(log_data_zero_words_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.priv = (void *)&expect_log_data_zero_words,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = test_logging_verify_platform,
		},
		{
			.name = "LOG DATA TEST (NUM_WORDS mismatch)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = log_data_bad_count_reqdata,
				.request_data_len = sizeof(log_data_bad_count_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.priv = (void *)&expect_no_call,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = test_logging_verify_platform,
		},
		{
			.name = "LOG DATA TEST (unaligned request)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = log_data_unaligned_reqdata,
				.request_data_len = sizeof(log_data_unaligned_reqdata),
				.expected_data = invalid_param_expdata,
				.expected_data_len = sizeof(invalid_param_expdata),
			},
			.priv = (void *)&expect_no_call,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = test_logging_verify_platform,
		},
		{
			.name = "LOG DATA TEST (platform error)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = log_data_denied_reqdata,
				.request_data_len = sizeof(log_data_denied_reqdata),
				.expected_data = denied_expdata,
				.expected_data_len = sizeof(denied_expdata),
			},
			.priv = (void *)&expect_denied,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = test_logging_verify_platform,
		},
		{
			.name = "LOG DATA TEST (posted request)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_POSTED_REQUEST,
				.request_data = log_data_reqdata,
				.request_data_len = sizeof(log_data_reqdata),
			},
			.priv = (void *)&expect_log_data,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.verify = test_logging_verify_platform,
		},
	},
};

int main(int argc, char *argv[])
{
	int ret;

	printf("Test Logging Service Group\n");

	ret = test_logging_group_allows_s_mode();
	if (ret)
		return ret;

	return test_scenario_execute(&scenario_logging_default);
}
