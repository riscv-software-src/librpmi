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
#define TEST_EDK2_STATUS_LOG_TYPE	0x0
#define TEST_EDK2_STATUS_CODE_TYPE	0x1
#define TEST_EDK2_PROGRESS_CODE		0x03040003
#define TEST_EDK2_BOOT_CODE		0x02011001
#define TEST_EDK2_INSTANCE		0x0
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

struct test_logging_scenario_config {
	enum rpmi_privilege_level privilege_level;
	rpmi_bool_t is_be;
	struct rpmi_service_group *grp;
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

static rpmi_uint32_t log_data_short_reqdata[] = {
	TEST_LOG_TYPE,
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

static rpmi_uint32_t log_data_denied_words_reqdata[] = {
	TEST_LOG_TYPE_DENIED,
	2,
	TEST_LOG_WORD0,
	TEST_LOG_WORD1,
};

static rpmi_uint32_t edk2_progress_reqdata[] = {
	TEST_EDK2_STATUS_LOG_TYPE,
	3,
	TEST_EDK2_STATUS_CODE_TYPE,
	TEST_EDK2_PROGRESS_CODE,
	TEST_EDK2_INSTANCE,
};

static rpmi_uint32_t edk2_boot_reqdata[] = {
	TEST_EDK2_STATUS_LOG_TYPE,
	3,
	TEST_EDK2_STATUS_CODE_TYPE,
	TEST_EDK2_BOOT_CODE,
	TEST_EDK2_INSTANCE,
};

static rpmi_uint32_t logged_words[] = {
	TEST_LOG_WORD0,
	TEST_LOG_WORD1,
};

static rpmi_uint32_t edk2_progress_words[] = {
	TEST_EDK2_STATUS_CODE_TYPE,
	TEST_EDK2_PROGRESS_CODE,
	TEST_EDK2_INSTANCE,
};

static rpmi_uint32_t edk2_boot_words[] = {
	TEST_EDK2_STATUS_CODE_TYPE,
	TEST_EDK2_BOOT_CODE,
	TEST_EDK2_INSTANCE,
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

static const struct test_logging_expect expect_edk2_progress = {
	.call_count = 1,
	.log_type = TEST_EDK2_STATUS_LOG_TYPE,
	.num_words = ARRAY_SIZE(edk2_progress_words),
	.words = edk2_progress_words,
};

static const struct test_logging_expect expect_edk2_boot = {
	.call_count = 1,
	.log_type = TEST_EDK2_STATUS_LOG_TYPE,
	.num_words = ARRAY_SIZE(edk2_boot_words),
	.words = edk2_boot_words,
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

static const struct test_logging_expect expect_denied_words = {
	.call_count = 1,
	.log_type = TEST_LOG_TYPE_DENIED,
	.num_words = ARRAY_SIZE(logged_words),
	.words = logged_words,
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

static int test_logging_big_endian_log_data(void)
{
	struct rpmi_test test = {
		.name = "LOG DATA TEST (big-endian request)",
		.priv = (void *)&expect_denied_words,
	};
	struct rpmi_transport trans = { .is_be = true };
	struct rpmi_service_group *grp;
	struct rpmi_service *service;
	rpmi_uint32_t req[ARRAY_SIZE(log_data_denied_words_reqdata)];
	rpmi_uint32_t resp[ARRAY_SIZE(denied_expdata)];
	rpmi_uint16_t resp_len = 0;
	enum rpmi_error rc;
	rpmi_uint32_t i;
	int failed = 0;

	grp = rpmi_service_group_logging_create(&test_logging_ops,
						&test_logging_capture);
	if (!grp) {
		printf("failed to create rpmi logging service group\n");
		return 1;
	}

	rpmi_env_memset(&test_logging_capture, 0, sizeof(test_logging_capture));
	for (i = 0; i < ARRAY_SIZE(req); i++)
		req[i] = rpmi_to_xe32(trans.is_be,
					 log_data_denied_words_reqdata[i]);

	service = &grp->services[RPMI_LOGGING_SRV_LOG_DATA];
	rpmi_env_lock(grp->lock);
	rc = service->process_a2p_request(grp, service, &trans, sizeof(req),
					  (const rpmi_uint8_t *)req, &resp_len,
					  (rpmi_uint8_t *)resp);
	rpmi_env_unlock(grp->lock);
	if (rc) {
		printf("%s: service callback failed: %d\n", test.name, rc);
		failed = 1;
	}

	if (resp_len != sizeof(denied_expdata)) {
		printf("%s: response length mismatch: expected: %zu, got: %u\n",
		       test.name, sizeof(denied_expdata), resp_len);
		failed = 1;
	} else if (resp[0] != rpmi_to_xe32(trans.is_be, denied_expdata[0])) {
		printf("%s: response status mismatch: expected: 0x%x, got: 0x%x\n",
		       test.name, rpmi_to_xe32(trans.is_be, denied_expdata[0]),
		       resp[0]);
		failed = 1;
	}

	failed += test_logging_verify_platform(NULL, &test, NULL);
	printf("TEST: %-50s \t : %s!\n", test.name,
	       failed ? "Failed" : "Succeeded");

	rpmi_service_group_logging_destroy(grp);
	return failed;
}

static int test_logging_scenario_cleanup(struct rpmi_test_scenario *scene)
{
	struct test_logging_scenario_config *config = scene->priv;

	if (scene->cntx && config && config->grp) {
		rpmi_context_remove_group(scene->cntx, config->grp);
		rpmi_service_group_logging_destroy(config->grp);
		config->grp = NULL;
	}

	if (scene->cntx) {
		rpmi_context_destroy(scene->cntx);
		scene->cntx = NULL;
	}

	return test_scenario_default_cleanup(scene);
}

static int test_logging_scenario_init(struct rpmi_test_scenario *scene)
{
	struct test_logging_scenario_config *config;
	struct rpmi_service_group *grp;
	enum rpmi_privilege_level privilege_level;
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
	scene->xport->is_be = config->is_be;

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

	grp = rpmi_service_group_logging_create(&test_logging_ops,
						&test_logging_capture);
	if (!grp) {
		printf("failed to create rpmi logging service group\n");
		ret = RPMI_ERR_FAILED;
		goto fail_cleanup;
	}

	ret = rpmi_context_add_group(scene->cntx, grp);
	if (ret) {
		printf("failed to add rpmi logging service group\n");
		rpmi_service_group_logging_destroy(grp);
		goto fail_cleanup;
	}
	config->grp = grp;
	scene->token_sequence = 0;

	return 0;

fail_cleanup:
	test_logging_scenario_cleanup(scene);
	return ret;
}

static struct test_logging_scenario_config logging_m_mode_config = {
	.privilege_level = RPMI_PRIVILEGE_M_MODE,
};

static struct rpmi_test_scenario scenario_logging_default = {
	.name = "Logging Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = &logging_m_mode_config,

	.init = test_logging_scenario_init,
	.cleanup = test_logging_scenario_cleanup,

	.num_tests = 10,
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
			.name = "LOG DATA TEST (short request)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = log_data_short_reqdata,
				.request_data_len = sizeof(log_data_short_reqdata),
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
			.name = "LOG DATA TEST (captured EDK2 progress code)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = edk2_progress_reqdata,
				.request_data_len = sizeof(edk2_progress_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.priv = (void *)&expect_edk2_progress,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = test_logging_verify_platform,
		},
		{
			.name = "LOG DATA TEST (captured EDK2 boot code)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = edk2_boot_reqdata,
				.request_data_len = sizeof(edk2_boot_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.priv = (void *)&expect_edk2_boot,
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

static struct test_logging_scenario_config logging_s_mode_config = {
	.privilege_level = RPMI_PRIVILEGE_S_MODE,
};

static struct rpmi_test_scenario scenario_logging_s_mode = {
	.name = "Logging Service Group (S-mode)",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = &logging_s_mode_config,

	.init = test_logging_scenario_init,
	.cleanup = test_logging_scenario_cleanup,

	.num_tests = 1,
	.tests = {
		{
			.name = "LOG DATA TEST (S-mode request)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_LOGGING,
				.service_id = RPMI_LOGGING_SRV_LOG_DATA,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = edk2_boot_reqdata,
				.request_data_len = sizeof(edk2_boot_reqdata),
				.expected_data = success_expdata,
				.expected_data_len = sizeof(success_expdata),
			},
			.priv = (void *)&expect_edk2_boot,
			.init = test_logging_test_init,
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
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

	ret = test_scenario_execute(&scenario_logging_default);
	ret |= test_scenario_execute(&scenario_logging_s_mode);
	ret |= test_logging_big_endian_log_data();

	return ret;
}
