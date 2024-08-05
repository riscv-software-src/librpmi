/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <stdio.h>
#include <string.h>
#include "test_common.h"
#include "test_log.h"

#define VENDOR_ID 0x1337
#define VENDOR_SUB_ID 0x7331
#define NUM_SOCKETS	1

struct hw_info {
	int status;
	int vendor_id;
	int hw_id_len;
	int hw_id[4];
} hw_info = {
	.vendor_id = VENDOR_ID,
	.hw_id_len = 0,
};

static int init_enable_notification_test(struct rpmi_test_scenario *scene,
					 struct rpmi_test *test)
{
	rpmi_uint32_t resp_data[1];
	char name[] = "RPMI_BASE_SRV_ENABLE_NOTIFICATION";

	test->attrs.svc_grp_id = RPMI_SRVGRP_BASE;
	test->attrs.svc_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION;
	test->attrs.svc_type = RPMI_MSG_NORMAL_REQUEST;

	test->query_data_len = 0;

	test->resp_data_len = sizeof(resp_data);
	memset(&resp_data, 0,  test->resp_data_len);
	resp_data[0] = RPMI_ERR_NOTSUPP;

	rpmi_env_memcpy(&test->resp_data, &resp_data, test->resp_data_len);

	rpmi_env_memcpy(&test->name, &name, sizeof(name));

	return 0;
}

static int init_impl_ver_test(struct rpmi_test_scenario *scene, struct rpmi_test *test)
{
	rpmi_uint32_t resp_data[2];
	char name[] = "RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION";

	test->attrs.svc_grp_id = RPMI_SRVGRP_BASE;
	test->attrs.svc_id = RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION;
	test->attrs.svc_type = RPMI_MSG_NORMAL_REQUEST;

	test->query_data_len = 0;

	test->resp_data_len = sizeof(resp_data);
	memset(&resp_data, 0,  test->resp_data_len);
	resp_data[0] = RPMI_SUCCESS;
	resp_data[1] = RPMI_BASE_VERSION(LIBRPMI_IMPL_VERSION_MAJOR,
						  LIBRPMI_IMPL_VERSION_MINOR);
	rpmi_env_memcpy(&test->resp_data, &resp_data, test->resp_data_len);

	rpmi_env_memcpy(&test->name, &name, sizeof(name));
	return 0;
}

static int init_spec_ver_test(struct rpmi_test_scenario *scene, struct rpmi_test *test)
{
	rpmi_uint32_t resp_data[2];
	char name[] = "RPMI_BASE_SRV_GET_SPEC_VERSION";

	test->attrs.svc_grp_id = RPMI_SRVGRP_BASE;
	test->attrs.svc_id = RPMI_BASE_SRV_GET_SPEC_VERSION;
	test->attrs.svc_type = RPMI_MSG_NORMAL_REQUEST;

	test->query_data_len = 0;

	test->resp_data_len = sizeof(resp_data);
	memset(&resp_data, 0,  test->resp_data_len);
	resp_data[0] = RPMI_SUCCESS;
	resp_data[1] = RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR,
						 RPMI_SPEC_VERSION_MINOR);

	rpmi_env_memcpy(&test->resp_data, &resp_data, test->resp_data_len);

	rpmi_env_memcpy(&test->name, &name, sizeof(name));
	return 0;
}

static int init_impl_idn_test(struct rpmi_test_scenario *scene, struct rpmi_test *test)
{
	rpmi_uint32_t resp_data[2];
	char name[] = "RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN";

	test->attrs.svc_grp_id = RPMI_SRVGRP_BASE;
	test->attrs.svc_id = RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN;
	test->attrs.svc_type = RPMI_MSG_NORMAL_REQUEST;

	test->query_data_len = 0;

	test->resp_data_len = sizeof(resp_data);
	memset(&resp_data, 0,  test->resp_data_len);
	resp_data[0] = RPMI_SUCCESS;
	resp_data[1] = LIBRPMI_IMPL_ID;
	rpmi_env_memcpy(&test->resp_data, &resp_data, test->resp_data_len);

	rpmi_env_memcpy(&test->name, &name, sizeof(name));
	return 0;
}

static int init_hw_info_test(struct rpmi_test_scenario *scene, struct rpmi_test *test)
{
	struct hw_info {
		int status;
		int vendor_id;
		int hw_id_len;
	} resp_data;

	char name[] = "RPMI_BASE_SRV_GET_HW_INFO";

	test->attrs.svc_grp_id = RPMI_SRVGRP_BASE;
	test->attrs.svc_id = RPMI_BASE_SRV_GET_HW_INFO;
	test->attrs.svc_type = RPMI_MSG_NORMAL_REQUEST;

	test->query_data_len = 0;

	test->resp_data_len = sizeof(resp_data);
	memset(&resp_data, 0,  test->resp_data_len);
	resp_data.status = RPMI_SUCCESS;
	resp_data.vendor_id  = (VENDOR_SUB_ID << 16 | VENDOR_ID);
	resp_data.hw_id_len  = 0;
	rpmi_env_memcpy(&test->resp_data, &resp_data, test->resp_data_len);

	rpmi_env_memcpy(&test->name, &name, sizeof(name));
	return 0;
}

static int init_probe_test(struct rpmi_test_scenario *scene, struct rpmi_test *test)
{
	rpmi_uint32_t resp_data[2];
	rpmi_uint32_t probe_grp = RPMI_SRVGRP_BASE;
	char name[] = "RPMI_BASE_SRV_PROBE_SERVICE_GROUP";

	test->attrs.svc_grp_id = RPMI_SRVGRP_BASE;
	test->attrs.svc_id = RPMI_BASE_SRV_PROBE_SERVICE_GROUP;
	test->attrs.svc_type = RPMI_MSG_NORMAL_REQUEST;

	test->query_data_len = sizeof(probe_grp);
	rpmi_env_memcpy(test->query_data, &probe_grp, test->query_data_len);

	test->resp_data_len = sizeof(resp_data);
	resp_data[0] = RPMI_SUCCESS;
	resp_data[1] = 1;
	rpmi_env_memcpy(&test->resp_data, &resp_data, test->resp_data_len);

	rpmi_env_memcpy(&test->name, &name, sizeof(name));
	return 0;
}

static int init_attribs_test(struct rpmi_test_scenario *scene, struct rpmi_test *test)
{
	rpmi_uint32_t resp_data[5];
	char name[] = "RPMI_BASE_SRV_GET_ATTRIBUTES";

	test->attrs.svc_grp_id = RPMI_SRVGRP_BASE;
	test->attrs.svc_id = RPMI_BASE_SRV_GET_ATTRIBUTES;
	test->attrs.svc_type = RPMI_MSG_NORMAL_REQUEST;

	test->query_data_len = 0;

	test->resp_data_len = sizeof(resp_data);
	resp_data[0] = RPMI_SUCCESS;
	resp_data[1] = RPMI_BASE_FLAGS_F0_MSI_EN;
	resp_data[2] = 0;
	resp_data[3] = 0;
	resp_data[4] = 0;
	rpmi_env_memcpy(&test->resp_data, &resp_data, test->resp_data_len);

	rpmi_env_memcpy(&test->name, &name, sizeof(name));
	return 0;
}

static int init_set_msi_test(struct rpmi_test_scenario *scene, struct rpmi_test *test)
{
	struct msi_data_s {
		rpmi_uint32_t addr_l;
		rpmi_uint32_t addr_h;
		rpmi_uint32_t data;
	} msi_data = {0};
	rpmi_uint32_t resp_data[1];
	char name[] = "RPMI_BASE_SRV_SET_MSI";

	test->attrs.svc_grp_id = RPMI_SRVGRP_BASE;
	test->attrs.svc_id = RPMI_BASE_SRV_SET_MSI;
	test->attrs.svc_type = RPMI_MSG_NORMAL_REQUEST;

	test->query_data_len = sizeof(msi_data);
	rpmi_env_memcpy(test->query_data, &msi_data, test->query_data_len);
	test->resp_data_len = sizeof(resp_data);
	resp_data[0] = RPMI_SUCCESS;
	rpmi_env_memcpy(&test->resp_data, &resp_data, test->resp_data_len);

	rpmi_env_memcpy(&test->name, &name, sizeof(name));
	return 0;
}

struct rpmi_test_scenario scenario_base_default = {
	.name = "Base Service Group Default",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.base.vendor_id = VENDOR_ID,
	.base.vendor_sub_id = VENDOR_SUB_ID,
	.base.hw_info_len = sizeof(hw_info),
	.base.hw_info = (const rpmi_uint8_t *)&hw_info,
	.priv = NULL,

	.init = test_scenario_default_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 8,
	.tests = {
		{
			.init = init_enable_notification_test,
		},
		{
			.init = init_probe_test,
		},
		{
			.init = init_impl_ver_test,
		},
		{
			.init = init_spec_ver_test,
		},
		{
			.init = init_impl_idn_test,
		},
		{
			.init = init_hw_info_test,
		},
		{
			.init = init_attribs_test,
		},
		{
			.init = init_set_msi_test,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test Base Service Group\n");

	/* Execute default scenario */
	return test_scenario_execute(&scenario_base_default);
}
