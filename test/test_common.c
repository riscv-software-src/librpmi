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

struct rpmi_message *get_rpmi_msg(struct rpmi_transport *rpmi_xport)
{
	return rpmi_env_zalloc(rpmi_xport->slot_size);
}

void free_rpmi_msg(struct rpmi_message *msg)
{
	if (msg)
		rpmi_env_free(msg);
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

void group_process(struct rpmi_test_group *group)
{
	struct rpmi_context *rpmi_context = group->rctx;

	if (rpmi_context) {
		rpmi_context_process_a2p_request(rpmi_context);
		rpmi_context_process_all_events(rpmi_context);
	} else {
		printf("%s: context not initialized!\n",
		       __func__);
	}
}


int test_run(struct rpmi_test *rpmi_test)
{
	int rc = 0;
	struct rpmi_message *req_msg = get_rpmi_msg(rpmi_test->rpmi_xport);

	req_msg->header.servicegroup_id = rpmi_test->attrs.svc_grp_id;
	req_msg->header.service_id = rpmi_test->attrs.svc_id;
	req_msg->header.datalen = rpmi_test->query_data_len;
	rpmi_env_memcpy(req_msg->data, rpmi_test->query_data, rpmi_test->query_data_len);

	do {
		rc = rpmi_transport_enqueue(rpmi_test->rpmi_xport,
					    RPMI_QUEUE_A2P_REQ, req_msg);
	} while (rc == RPMI_ERR_OUTOFRES);

	if (rc) {
		printf("%s: failed (error %d)\n",
		       __func__,  rc);
	}

	free_rpmi_msg(req_msg);
	return rc;
}

void test_wait(struct rpmi_test *rpmi_test, struct rpmi_message *msg)
{
	int result = -1;

	rpmi_env_memset(msg, 0, sizeof(*msg));

	/* wait for response */
	if (rpmi_test->attrs.svc_type != RPMI_MSG_POSTED_REQUEST) {

		while (result != RPMI_SUCCESS)
			result = rpmi_transport_dequeue(rpmi_test->rpmi_xport,
						    RPMI_QUEUE_P2A_ACK,
						    msg);
	}
}

void test_verify(struct rpmi_test *rpmi_test, struct rpmi_message *msg)
{
	int failed = 0;

	if (msg->header.datalen != rpmi_test->resp_data_len) {
		/* expected data error */
		printf("%s: datalen mismatch: expected: %d, got: %d\n",
		       rpmi_test->name,
		       rpmi_test->resp_data_len, msg->header.datalen);
		failed = 1;
	} else {
		if (rpmi_env_memcmp(&rpmi_test->resp_data, &msg->data,
			   rpmi_test->resp_data_len)) {
			printf("%s: datalen: %d, expected data mismatch\n",
			       rpmi_test->name,
			       rpmi_test->resp_data_len);
			hexdump("expected",
				(unsigned int *)&rpmi_test->resp_data,
				rpmi_test->resp_data_len);
			hexdump("received",
				(unsigned int *)msg->data,
				rpmi_test->resp_data_len);
			failed = 1;
		}
	}
	printf("%-50s \t : %s!\n", rpmi_test->name, failed?"Failed":"Succeeded");
}

void execute_group(struct rpmi_test_group *group)
{
	int test;
	struct rpmi_test *rpmi_test;
	struct rpmi_transport *rpmi_xport = group->xport;
	struct rpmi_message *msg = get_rpmi_msg(rpmi_xport);

	if (!group)
		return;

	printf("\nExecuting %s service group tests :\n", group->name);
	printf("-------------------------------------------------\n");
	for (test = 0; test < group->num_tests; test++) {

		rpmi_test = &group->tests[test];
		rpmi_test->rpmi_xport = rpmi_xport;

		/* initialize test parameters */
		if (rpmi_test->init)
			rpmi_test->init(rpmi_test);

		/* run the test */
		if (rpmi_test->run)
			rpmi_test->run(rpmi_test);
		else
			test_run(rpmi_test);

		if (group->process)
			group->process(group);
		else
			group_process(group);

		/* wait for test to finish */
		if (rpmi_test->wait)
			rpmi_test->wait(rpmi_test, msg);
		else
			test_wait(rpmi_test, msg);

		/* verify and report */
		if (rpmi_test->verify)
			rpmi_test->verify(rpmi_test, msg);
		else
			test_verify(rpmi_test, msg);

		/* cleanup if needed */
		if (rpmi_test->cleanup)
			rpmi_test->cleanup(rpmi_test);
	}

	free_rpmi_msg(msg);
}

int init_group(struct rpmi_test_group *group)
{
	return 0;
}

void cleanup_group(struct rpmi_test_group *group)
{
	rpmi_env_free(group->xport->priv);
	rpmi_env_free(group->rctx);

	/* cleanup group specific stuff */
	if (group->cleanup)
		group->cleanup(group);

	rpmi_env_free(rpmi_shm);
}

int execute_test_group(struct rpmi_test_group *group)
{
	int rc;

	if(group->init)
		rc = group->init(group);
	else
		rc = init_group(group);

	if (!rc) {
		execute_group(group);
		cleanup_group(group);
	} else {
		printf("Failed to get rpmi transport\n");
		return -1;
	}

	return 0;
}
