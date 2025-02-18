/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include "test_common.h"

/* dump buffer in hexadecimal format */
void hexdump(char *desc, unsigned int *buf, unsigned int len)
{
	unsigned int i;

	printf("Dumping %s at %p\n", desc, buf);
	for (i = 0; i < len/4; i++)
		printf("%d: 0x%08x\n", i, *(buf+i));

}

void *rpmi_env_zalloc(rpmi_size_t size)
{
	return calloc(1, size);
}

void rpmi_env_free(void *ptr)
{
	free(ptr);
}

void rpmi_env_writel(rpmi_uint64_t addr, rpmi_uint32_t val)
{
	rpmi_uint32_t *addr_u32 = (void *)(rpmi_uintptr_t)addr;
	*addr_u32 = val;
}

static void scenario_process(struct rpmi_test_scenario *scene)
{
	struct rpmi_context *cntx = scene->cntx;

	if (cntx) {
		rpmi_context_process_a2p_request(cntx);
		rpmi_context_process_all_events(cntx);
	} else {
		printf("%s: context not initialized!\n", __func__);
	}
}

static int test_run(struct rpmi_test_scenario *scene, struct rpmi_test *test,
		    struct rpmi_message *req_msg)
{
	int rc = 0;

	do {
		rc = rpmi_transport_enqueue(scene->xport,
					    RPMI_QUEUE_A2P_REQ, req_msg);
	} while (rc == RPMI_ERR_IO);

	if (rc)
		printf("%s: failed (error %d)\n", __func__,  rc);

	return rc;
}

static void test_wait(struct rpmi_test_scenario *scene, struct rpmi_test *test,
		      struct rpmi_message *resp_msg)
{
	int result = -1;

	rpmi_env_memset(resp_msg, 0, sizeof(*resp_msg));

	/* wait for response */
	if (test->attrs.flags != RPMI_MSG_POSTED_REQUEST) {
		while (result != RPMI_SUCCESS)
			result = rpmi_transport_dequeue(scene->xport,
							RPMI_QUEUE_P2A_ACK,
							resp_msg);
	}
}

static void test_verify(struct rpmi_test *test, struct rpmi_message *msg,
			void *exp_data, rpmi_uint16_t exp_data_len)
{
	int failed = 0;

	if (msg->header.datalen != exp_data_len) {
		/* expected data error */
		printf("%s: datalen mismatch: expected: %d, got: %d\n",
		       test->name, exp_data_len, msg->header.datalen);
		hexdump("expected",
			(unsigned int *)exp_data, exp_data_len);
		hexdump("received",
			(unsigned int *)msg->data, msg->header.datalen);
		failed = 1;
	} else if (exp_data_len) {
		if (rpmi_env_memcmp(exp_data, &msg->data, exp_data_len)) {
			printf("%s: datalen: %d, expected data mismatch\n",
			       test->name, exp_data_len);
			hexdump("expected",
				(unsigned int *)exp_data, exp_data_len);
			hexdump("received",
				(unsigned int *)msg->data, exp_data_len);
			failed = 1;
		}
	}

	printf("%-50s \t : %s!\n", test->name, failed ? "Failed" : "Succeeded");
}

static void execute_scenario(struct rpmi_test_scenario *scene)
{
	struct rpmi_message *req_msg, *resp_msg;
	rpmi_uint16_t exp_data_len;
	struct rpmi_test *test;
	void *exp_data;
	int i, rc;

	req_msg = rpmi_env_zalloc(scene->slot_size);
	if (!req_msg) {
		printf("Failed to allocate request message\n");
		return;
	}
	exp_data = rpmi_env_zalloc(RPMI_MSG_DATA_SIZE(scene->slot_size));
	if (!exp_data) {
		printf("Failed to allocate expected message\n");
		rpmi_env_free(req_msg);
		return;
	}
	resp_msg = rpmi_env_zalloc(scene->slot_size);
	if (!resp_msg) {
		printf("Failed to allocate response message\n");
		rpmi_env_free(exp_data);
		rpmi_env_free(req_msg);
		return;
	}

	printf("\nExecuting %s test scenario :\n", scene->name);
	printf("-------------------------------------------------\n");
	for (i = 0; i < scene->num_tests; i++) {
		test = &scene->tests[i];

		/* Initialize the test */
		if (test->init)
			rc = test->init(scene, test);
		else
			rc = 0;
		if (rc) {
			printf("Failed to initialize test %s (error %d)\n", test->name, rc);
			continue;
		}

		/* Initialize request message header */
		req_msg->header.servicegroup_id = test->attrs.servicegroup_id;
		req_msg->header.service_id = test->attrs.service_id;
		req_msg->header.datalen = 0;
		req_msg->header.token = scene->token_sequence;
		scene->token_sequence++;

		/* Initialize request message data */
		if (test->init_request_data)
			req_msg->header.datalen = test->init_request_data(scene, test,
				&req_msg->data, RPMI_MSG_DATA_SIZE(scene->slot_size));

		/* Initialize expected message data */
		rpmi_env_memset(exp_data, 0, RPMI_MSG_DATA_SIZE(scene->slot_size));
		if (test->init_expected_data)
			exp_data_len = test->init_expected_data(scene, test,
				exp_data, RPMI_MSG_DATA_SIZE(scene->slot_size));
		else
			exp_data_len = 0;

		/* Run the test */
		if (test->run)
			rc = test->run(scene, test, req_msg);
		else
			rc = test_run(scene, test, req_msg);
		if (rc) {
			printf("Failed to run test %s (error %d)\n", test->name, rc);
			goto skip;
		}

		if (scene->process)
			scene->process(scene);
		else
			scenario_process(scene);

		/* Wait for test to finish */
		if (test->wait)
			test->wait(scene, test, resp_msg);
		else
			test_wait(scene, test, resp_msg);

		/* Verify and report */
		test_verify(test, resp_msg, exp_data, exp_data_len);

skip:
		/* Cleanup if needed */
		if (test->cleanup)
			test->cleanup(scene, test);
	}

	rpmi_env_free(resp_msg);
	rpmi_env_free(exp_data);
	rpmi_env_free(req_msg);
}

rpmi_uint16_t test_init_request_data_from_attrs(struct rpmi_test_scenario *scene,
						struct rpmi_test *test,
						void *data, rpmi_uint16_t max_data_len)
{
	rpmi_env_memcpy(data, test->attrs.request_data, test->attrs.request_data_len);
	return test->attrs.request_data_len;
}

rpmi_uint16_t test_init_expected_data_from_attrs(struct rpmi_test_scenario *scene,
						 struct rpmi_test *test,
						 void *data, rpmi_uint16_t max_data_len)
{
	rpmi_env_memcpy(data, test->attrs.expected_data, test->attrs.expected_data_len);
	return test->attrs.expected_data_len;
}

int test_scenario_default_init(struct rpmi_test_scenario *scene)
{
	if (!scene || scene->shm || scene->shmem || scene->xport || scene->cntx)
		return RPMI_ERR_ALREADY;

	scene->shm = rpmi_env_zalloc(scene->shm_size);
	if (!scene->shm)
		return RPMI_ERR_FAILED;

	scene->shmem = rpmi_shmem_create("test_shmem",
					 (unsigned long)scene->shm, scene->shm_size,
					 &rpmi_shmem_simple_ops, NULL);
	if (!scene->shmem) {
		printf("%s: failed to create test rpmi_shmem\n ", __func__);
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
		return RPMI_ERR_FAILED;
	}

	scene->xport = rpmi_transport_shmem_create("test_transport", scene->slot_size,
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

	scene->cntx = rpmi_context_create("test_context", scene->xport,
					  scene->max_num_groups,
					  RPMI_PRIVILEGE_M_MODE,
					  scene->base.plat_info_len, scene->base.plat_info);
	if (!scene->cntx) {
		printf("%s: failed to create test rpmi_context\n ", __func__);
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

int test_scenario_default_cleanup(struct rpmi_test_scenario *scene)
{
	if (!scene) {
		printf("Invalid test scenario\n");
		return RPMI_ERR_INVALID_PARAM;
	}

	if (scene->xport) {
		rpmi_transport_shmem_destroy(scene->xport);
		scene->xport = NULL;
	}

	if (scene->shmem) {
		rpmi_shmem_destroy(scene->shmem);
		scene->shmem = NULL;
	}

	if (scene->shm) {
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
	}

	return 0;
}

int test_scenario_execute(struct rpmi_test_scenario *scene)
{
	int rc;

	if (!scene || !scene->init || !scene->cleanup) {
		printf("Invalid test scenario\n");
		return RPMI_ERR_INVALID_PARAM;
	}

	rc = scene->init(scene);
	if (rc) {
		printf("Failed to initalize test scenario %s\n", scene->name);
		return rc;
	}

	execute_scenario(scene);

	rc = scene->cleanup(scene);
	if (rc) {
		printf("Failed to cleanup test scenario %s\n", scene->name);
		return rc;
	}

	return 0;
}
