// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include "test_common.h"
#include "test_log.h"

#define TEST_EVENT_ID		0x0
#define TEST_REQUEST_STATE_ENABLE	0x1
#define TEST_MM_SHMEM_SIZE	256
#define TEST_MM_IDATA_OFF	0
#define TEST_MM_ODATA_OFF	64
#define TEST_MM_PAYLOAD		0x5a5aa5a5
#define TEST_MM_RESPONSE	((rpmi_uint32_t)~TEST_MM_PAYLOAD)
#define TEST_MM_VERSION		0x00010000
#define TEST_MM_BAD_GUID_DATA1	0x87654321

static rpmi_uint8_t test_mm_mem[TEST_MM_SHMEM_SIZE];
static struct rpmi_shmem *test_mm_shmem;
static int test_mm_callback_count;
static rpmi_uint32_t test_mm_seen_payload;
static struct rpmi_mm_comm_req test_mm_seen_req;

static struct rpmi_guid_t test_mm_guid = {
	.data1 = 0x12345678,
	.data2 = 0x9abc,
	.data3 = 0xdef0,
	.data4 = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe },
};

static struct rpmi_guid_t test_mm_bad_guid = {
	.data1 = TEST_MM_BAD_GUID_DATA1,
	.data2 = 0x9abc,
	.data3 = 0xdef0,
	.data4 = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe },
};

static rpmi_uint32_t enable_notif_reqdata[] = {
	TEST_EVENT_ID,
	TEST_REQUEST_STATE_ENABLE,
};

static rpmi_uint32_t notsupp_expdata[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t communicate_reqdata[] = {
	TEST_MM_IDATA_OFF,
	GUID_LENGTH + sizeof(rpmi_uint32_t),
	TEST_MM_ODATA_OFF,
	sizeof(rpmi_uint32_t),
};

static rpmi_uint32_t communicate_expdata[] = {
	RPMI_SUCCESS,
	sizeof(rpmi_uint32_t),
};

static rpmi_uint32_t communicate_unknown_guid_expdata[] = {
	RPMI_ERR_NO_DATA,
};

static enum rpmi_error test_mm_active_cb(struct rpmi_shmem *shmem,
						rpmi_uint16_t req_datalen,
						const rpmi_uint8_t *req_data,
						rpmi_uint16_t *rsp_datalen,
						rpmi_uint8_t *rsp_data,
						void *priv_data)
{
	struct rpmi_mm_comm_req *req = (void *)req_data;
	rpmi_uint32_t response = TEST_MM_RESPONSE;

	test_mm_callback_count++;
	test_mm_seen_req = *req;
	rpmi_shmem_read(shmem, req->idata_off + GUID_LENGTH,
			&test_mm_seen_payload, sizeof(test_mm_seen_payload));
	rpmi_shmem_write(shmem, req->odata_off, &response, sizeof(response));
	*rsp_datalen = sizeof(response);

	return RPMI_SUCCESS;
}

static struct rpmi_mm_service test_mm_service = {
	.guid = {
		.data1 = 0x12345678,
		.data2 = 0x9abc,
		.data3 = 0xdef0,
		.data4 = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe },
	},
	.active_cbfn_p = test_mm_active_cb,
	.delete_cbfn_p = NULL,
	.priv_data = NULL,
};

static rpmi_uint16_t init_attrs_expected(struct rpmi_test_scenario *scene,
						 struct rpmi_test *test,
						 void *data,
						 rpmi_uint16_t max_data_len)
{
	rpmi_uint32_t *exp = data;
	rpmi_uint64_t base = (rpmi_uint64_t)(rpmi_uintptr_t)test_mm_mem;

	exp[0] = RPMI_SUCCESS;
	exp[1] = TEST_MM_VERSION;
	exp[2] = (rpmi_uint32_t)base;
	exp[3] = (rpmi_uint32_t)(base >> 32);
	exp[4] = sizeof(test_mm_mem);

	return 5 * sizeof(*exp);
}

static rpmi_uint16_t
init_communicate_request_guid(struct rpmi_test_scenario *scene,
			       struct rpmi_test *test, void *data,
			       rpmi_uint16_t max_data_len,
			       struct rpmi_guid_t *guid)
{
	rpmi_uint32_t payload = TEST_MM_PAYLOAD;

	test_mm_callback_count = 0;
	test_mm_seen_payload = 0;
	rpmi_env_memset(&test_mm_seen_req, 0, sizeof(test_mm_seen_req));
	rpmi_env_memset(test_mm_mem, 0, sizeof(test_mm_mem));
	rpmi_shmem_write(test_mm_shmem, TEST_MM_IDATA_OFF, guid, sizeof(*guid));
	rpmi_shmem_write(test_mm_shmem, TEST_MM_IDATA_OFF + GUID_LENGTH,
			&payload, sizeof(payload));

	return test_init_request_data_from_attrs(scene, test, data, max_data_len);
}

static rpmi_uint16_t
init_communicate_request(struct rpmi_test_scenario *scene,
			 struct rpmi_test *test, void *data,
			 rpmi_uint16_t max_data_len)
{
	return init_communicate_request_guid(scene, test, data, max_data_len,
					   &test_mm_guid);
}

static rpmi_uint16_t
init_communicate_unknown_guid_request(struct rpmi_test_scenario *scene,
				      struct rpmi_test *test, void *data,
				      rpmi_uint16_t max_data_len)
{
	return init_communicate_request_guid(scene, test, data, max_data_len,
					   &test_mm_bad_guid);
}

static void wait_for_optional_mm_ack(struct rpmi_test_scenario *scene,
					     struct rpmi_test *test,
					     struct rpmi_message *msg)
{
	int rc;

	rpmi_env_memset(msg, 0, sizeof(*msg));
	rc = rpmi_transport_dequeue(scene->xport, RPMI_QUEUE_P2A_ACK, msg);
	if (rc == RPMI_ERR_IO)
		printf("%s: missing acknowledgment for unknown MM GUID\n",
		       test->name);
	else if (rc)
		printf("%s: failed to check acknowledgment queue (error %d)\n",
		       test->name, rc);
}

static int verify_communicate(struct rpmi_test_scenario *scene,
				      struct rpmi_test *test,
				      struct rpmi_message *msg)
{
	rpmi_uint32_t response = 0;

	if (test_mm_callback_count != 1) {
		printf("%s: expected one MM callback, got %d\n",
		       test->name, test_mm_callback_count);
		return 1;
	}

	if (test_mm_seen_req.idata_off != TEST_MM_IDATA_OFF ||
	    test_mm_seen_req.odata_off != TEST_MM_ODATA_OFF ||
	    test_mm_seen_payload != TEST_MM_PAYLOAD) {
		printf("%s: unexpected MM request side effects\n", test->name);
		return 1;
	}

	rpmi_shmem_read(test_mm_shmem, TEST_MM_ODATA_OFF,
			&response, sizeof(response));
	if (response != TEST_MM_RESPONSE) {
		printf("%s: expected MM response 0x%x, got 0x%x\n",
		       test->name, TEST_MM_RESPONSE, response);
		return 1;
	}

	return 0;
}

static int test_mm_scenario_init(struct rpmi_test_scenario *scene)
{
	struct rpmi_service_group *grp;
	int ret;

	ret = test_scenario_default_init(scene);
	if (ret)
		return RPMI_ERR_FAILED;

	rpmi_env_memset(test_mm_mem, 0, sizeof(test_mm_mem));
	test_mm_shmem = rpmi_shmem_create("test_mm_shmem",
						 (rpmi_uint64_t)(rpmi_uintptr_t)test_mm_mem,
						 sizeof(test_mm_mem),
						 &rpmi_shmem_simple_ops, NULL);
	if (!test_mm_shmem) {
		printf("failed to create rpmi mm shmem");
		return RPMI_ERR_FAILED;
	}

	grp = rpmi_service_group_mm_create(test_mm_shmem);
	if (!grp) {
		printf("failed to create rpmi mm service group");
		return RPMI_ERR_FAILED;
	}

	ret = rpmi_mm_service_register(grp, 1, &test_mm_service);
	if (ret) {
		printf("failed to register rpmi mm service");
		return RPMI_ERR_FAILED;
	}

	rpmi_context_add_group(scene->cntx, grp);
	return 0;
}

static struct rpmi_test_scenario scenario_mm_default = {
	.name = "Management Mode Service Group",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.priv = NULL,

	.init = test_mm_scenario_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 5,
	.tests = {
		{
			.name = "ENABLE NOTIFICATION TEST (notifications not supported)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_MANAGEMENT_MODE,
				.service_id = RPMI_MM_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = enable_notif_reqdata,
				.request_data_len = sizeof(enable_notif_reqdata),
				.expected_data = notsupp_expdata,
				.expected_data_len = sizeof(notsupp_expdata),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "GET ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_MANAGEMENT_MODE,
				.service_id = RPMI_MM_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_attrs_expected,
		},
		{
			.name = "COMMUNICATE",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_MANAGEMENT_MODE,
				.service_id = RPMI_MM_SRV_COMMUNICATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = communicate_reqdata,
				.request_data_len = sizeof(communicate_reqdata),
				.expected_data = communicate_expdata,
				.expected_data_len = sizeof(communicate_expdata),
			},
			.init_request_data = init_communicate_request,
			.init_expected_data = test_init_expected_data_from_attrs,
			.verify = verify_communicate,
		},
		{
			.name = "COMMUNICATE (unknown GUID)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_MANAGEMENT_MODE,
				.service_id = RPMI_MM_SRV_COMMUNICATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = communicate_reqdata,
				.request_data_len = sizeof(communicate_reqdata),
				.expected_data = communicate_unknown_guid_expdata,
				.expected_data_len = sizeof(communicate_unknown_guid_expdata),
			},
			.init_request_data = init_communicate_unknown_guid_request,
			.init_expected_data = test_init_expected_data_from_attrs,
			.wait = wait_for_optional_mm_ack,
		},
		{
			.name = "COMMUNICATE (short request)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_MANAGEMENT_MODE,
				.service_id = RPMI_MM_SRV_COMMUNICATE,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = notsupp_expdata,
				.expected_data_len = sizeof(notsupp_expdata),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test Management Mode Service Group\n");
	return test_scenario_execute(&scenario_mm_default);
}
