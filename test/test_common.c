/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include "test_common.h"

#define VENDOR_ID 0x1337
#define VENDOR_SUB_ID 0x7331

#define NUM_SOCKETS     1

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
	*((rpmi_uint32_t *) addr) = val;
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

	req_msg->header.servicegroup_id = test->attrs.svc_grp_id;
	req_msg->header.service_id = test->attrs.svc_id;
	req_msg->header.datalen = test->query_data_len;
	rpmi_env_memcpy(req_msg->data, test->query_data, test->query_data_len);

	do {
		rc = rpmi_transport_enqueue(scene->xport,
					    RPMI_QUEUE_A2P_REQ, req_msg);
	} while (rc == RPMI_ERR_OUTOFRES);

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
	if (test->attrs.svc_type != RPMI_MSG_POSTED_REQUEST) {
		while (result != RPMI_SUCCESS)
			result = rpmi_transport_dequeue(scene->xport,
							RPMI_QUEUE_P2A_ACK,
							resp_msg);
	}
}

static void test_verify(struct rpmi_test_scenario *scene, struct rpmi_test *test,
			struct rpmi_message *msg)
{
	int failed = 0;

	if (msg->header.datalen != test->resp_data_len) {
		/* expected data error */
		printf("%s: datalen mismatch: expected: %d, got: %d\n",
		       test->name, test->resp_data_len, msg->header.datalen);
		failed = 1;
	} else {
		if (rpmi_env_memcmp(&test->resp_data, &msg->data,
				    test->resp_data_len)) {
			printf("%s: datalen: %d, expected data mismatch\n",
			       test->name, test->resp_data_len);
			hexdump("expected",
				(unsigned int *)&test->resp_data,
				test->resp_data_len);
			hexdump("received",
				(unsigned int *)msg->data, test->resp_data_len);
			failed = 1;
		}
	}
	printf("%-50s \t : %s!\n", test->name, failed ? "Failed":"Succeeded");
}

static void execute_scenario(struct rpmi_test_scenario *scene)
{
	struct rpmi_message *req_msg, *resp_msg;
	struct rpmi_test *test;
	int i;

	req_msg = rpmi_env_zalloc(scene->xport->slot_size);
	if (!req_msg) {
		printf("Failed to allocate request message\n");
		return;
	}
	resp_msg = rpmi_env_zalloc(scene->xport->slot_size);
	if (!resp_msg) {
		printf("Failed to allocate response message\n");
		return;
	}

	printf("\nExecuting %s test scenario :\n", scene->name);
	printf("-------------------------------------------------\n");
	for (i = 0; i < scene->num_tests; i++) {
		test = &scene->tests[i];

		/* initialize test parameters */
		if (test->init)
			test->init(scene, test);

		/* run the test */
		if (test->run)
			test->run(scene, test, req_msg);
		else
			test_run(scene, test, req_msg);

		if (scene->process)
			scene->process(scene);
		else
			scenario_process(scene);

		/* wait for test to finish */
		if (test->wait)
			test->wait(scene, test, resp_msg);
		else
			test_wait(scene, test, resp_msg);

		/* verify and report */
		if (test->verify)
			test->verify(scene, test, resp_msg);
		else
			test_verify(scene, test, resp_msg);

		/* cleanup if needed */
		if (test->cleanup)
			test->cleanup(scene, test);
	}

	rpmi_env_free(req_msg);
	rpmi_env_free(resp_msg);
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
					  scene->base.vendor_id, scene->base.vendor_sub_id,
					  scene->base.hw_info_len, scene->base.hw_info);
	if (!scene->xport) {
		printf("%s: failed to create test rpmi_context\n ", __func__);
		rpmi_transport_shmem_destroy(scene->xport);
		scene->xport = NULL;
		rpmi_shmem_destroy(scene->shmem);
		scene->shmem = NULL;
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
		return RPMI_ERR_FAILED;
	}

	return 0;
}

int test_scenario_default_cleanup(struct rpmi_test_scenario *scene)
{
	if (!scene) {
		printf("Invalid test scenario\n");
		return RPMI_ERR_INVAL;
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
		return RPMI_ERR_INVAL;
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
