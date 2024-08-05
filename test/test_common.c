/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include "test_common.h"

#define VENDOR_ID 0x1337
#define VENDOR_SUB_ID 0x7331

#define NUM_SOCKETS     1

char *rpmi_shm = NULL;

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

void *get_shm_addr(unsigned int *shm_sz)
{
	if (!rpmi_shm) {
		rpmi_shm = rpmi_env_zalloc(RPMI_SHM_SZ);
	}

	if (rpmi_shm)
	{
		*shm_sz = RPMI_SHM_SZ;
	}

	return rpmi_shm;
}

struct rpmi_transport *init_test_rpmi_xport(int do_clear)
{
	char *shm = NULL;
	unsigned int shm_sz;
	struct rpmi_shmem *rpmi_shmem = NULL;
	struct rpmi_transport *rpmi_transport_shmem;

	/* initialize shm for test */
	shm = get_shm_addr(&shm_sz);
	if (!shm) {
		printf("get_shm_addr failed\n");
		return NULL;
	}

	if (do_clear)
		rpmi_env_memset((void *)shm, 0, shm_sz);

	rpmi_shmem = rpmi_shmem_create("rpmi_shmem", (rpmi_uint64_t)shm, shm_sz,
				       &rpmi_shmem_simple_ops, shm);
	if (!rpmi_shmem) {
		printf("%s: rpmi_shmem_tests_create failed\n ", __func__);
		return NULL;
	}

	rpmi_transport_shmem = rpmi_transport_shmem_create("rpmi_transport_shmem",
							   RPMI_SLOT_SIZE,
							   rpmi_shmem);
	if (!rpmi_transport_shmem) {
		printf("%s: transport_shmem_create failed, slot_sz: %d, shm: %p\n",
		       __func__, RPMI_SLOT_SIZE_MIN, rpmi_shmem);
		return NULL;
	}

	return rpmi_transport_shmem;
}

static void scenario_process(struct rpmi_test_scenario *scene)
{
	struct rpmi_context *rpmi_context = scene->rctx;

	if (rpmi_context) {
		rpmi_context_process_a2p_request(rpmi_context);
		rpmi_context_process_all_events(rpmi_context);
	} else {
		printf("%s: context not initialized!\n",
		       __func__);
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

	if (!scene)
		return;

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

static int init_scenario(struct rpmi_test_scenario *scene)
{
	return 0;
}

static void cleanup_scenario(struct rpmi_test_scenario *scene)
{
	rpmi_env_free(scene->xport->priv);
	rpmi_env_free(scene->rctx);

	/* cleanup scenario specific stuff */
	if (scene->cleanup)
		scene->cleanup(scene);

	rpmi_env_free(rpmi_shm);
}

int execute_test_scenario(struct rpmi_test_scenario *scene)
{
	int rc;

	if(scene->init)
		rc = scene->init(scene);
	else
		rc = init_scenario(scene);

	if (!rc) {
		execute_scenario(scene);
		cleanup_scenario(scene);
	} else {
		printf("Failed to get rpmi transport\n");
		return -1;
	}

	return 0;
}
