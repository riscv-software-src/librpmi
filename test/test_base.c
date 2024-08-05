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
#define HW_ID0 0xd00dfeed
#define HW_ID1 0xc001babe

static struct hw_info {
	rpmi_uint32_t status;
	rpmi_uint32_t vendor_id;
	rpmi_uint32_t hw_id_len;
	rpmi_uint32_t hw_id[2];
} hw_info_val = {
	.vendor_id = RPMI_BASE_VENDOR_ID(VENDOR_ID, VENDOR_SUB_ID),
	.hw_id_len = sizeof(rpmi_uint32_t) * 2,
	.hw_id[0] = HW_ID0,
	.hw_id[1] = HW_ID1,
};

static rpmi_uint16_t init_enable_notification_expected_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = RPMI_ERR_NOTSUPP;
	return sizeof(rpmi_uint32_t);
}

static rpmi_uint16_t init_spec_ver_expected_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = RPMI_SUCCESS;
	((rpmi_uint32_t *)data)[1] = RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR,
							RPMI_SPEC_VERSION_MINOR);
	return sizeof(rpmi_uint32_t) * 2;
}

static rpmi_uint16_t init_impl_ver_expected_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = RPMI_SUCCESS;
	((rpmi_uint32_t *)data)[1] = RPMI_BASE_VERSION(LIBRPMI_IMPL_VERSION_MAJOR,
							LIBRPMI_IMPL_VERSION_MINOR);
	return sizeof(rpmi_uint32_t) * 2;
}

static rpmi_uint16_t init_impl_idn_expected_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = RPMI_SUCCESS;
	((rpmi_uint32_t *)data)[1] = LIBRPMI_IMPL_ID;
	return sizeof(rpmi_uint32_t) * 2;
}

static rpmi_uint16_t init_hw_info_expected_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	rpmi_env_memcpy(data, &hw_info_val, sizeof(hw_info_val));
	return sizeof(hw_info_val);
}

static rpmi_uint16_t init_probe_request_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = RPMI_SRVGRP_BASE;
	return sizeof(rpmi_uint32_t);
}

static rpmi_uint16_t init_probe_expected_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = RPMI_SUCCESS;
	((rpmi_uint32_t *)data)[1] = 1;
	return sizeof(rpmi_uint32_t) * 2;
}

static rpmi_uint16_t init_attribs_expected_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = RPMI_SUCCESS;
	((rpmi_uint32_t *)data)[1] = RPMI_BASE_FLAGS_F0_MSI_EN;
	((rpmi_uint32_t *)data)[2] = 0;
	((rpmi_uint32_t *)data)[3] = 0;
	((rpmi_uint32_t *)data)[4] = 0;
	return sizeof(rpmi_uint32_t) * 5;
}

static rpmi_uint16_t init_set_msi_request_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = 0; /* MSI addr_lo */
	((rpmi_uint32_t *)data)[1] = 0; /* MSI addr_hi */
	((rpmi_uint32_t *)data)[2] = 0; /* MSI data */
	return sizeof(rpmi_uint32_t) * 3;
}

static rpmi_uint16_t init_set_msi_expected_data(
					    struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len)
{
	((rpmi_uint32_t *)data)[0] = RPMI_SUCCESS;
	return sizeof(rpmi_uint32_t);
}

struct rpmi_test_scenario scenario_base_default = {
	.name = "Base Service Group Default",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.base.vendor_id = VENDOR_ID,
	.base.vendor_sub_id = VENDOR_SUB_ID,
	.base.hw_info_len = sizeof(hw_info_val.hw_id),
	.base.hw_info = (const rpmi_uint8_t *)&hw_info_val.hw_id,
	.priv = NULL,

	.init = test_scenario_default_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 8,
	.tests = {
		{
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION",
			.attrs = {
				.svc_grp_id = RPMI_SRVGRP_BASE,
				.svc_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.svc_type = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_enable_notification_expected_data,
		},
		{
			.name = "RPMI_BASE_SRV_PROBE_SERVICE_GROUP",
			.attrs = {
				.svc_grp_id = RPMI_SRVGRP_BASE,
				.svc_id = RPMI_BASE_SRV_PROBE_SERVICE_GROUP,
				.svc_type = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_request_data = init_probe_request_data,
			.init_expected_data = init_probe_expected_data,
		},
		{
			.name = "RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION",
			.attrs = {
				.svc_grp_id = RPMI_SRVGRP_BASE,
				.svc_id = RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION,
				.svc_type = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_impl_ver_expected_data,
		},
		{
			.name = "RPMI_BASE_SRV_GET_SPEC_VERSION",
			.attrs = {
				.svc_grp_id = RPMI_SRVGRP_BASE,
				.svc_id = RPMI_BASE_SRV_GET_SPEC_VERSION,
				.svc_type = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_spec_ver_expected_data,
		},
		{
			.name = "RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN",
			.attrs = {
				.svc_grp_id = RPMI_SRVGRP_BASE,
				.svc_id = RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN,
				.svc_type = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_impl_idn_expected_data,
		},
		{
			.name = "RPMI_BASE_SRV_GET_HW_INFO",
			.attrs = {
				.svc_grp_id = RPMI_SRVGRP_BASE,
				.svc_id = RPMI_BASE_SRV_GET_HW_INFO,
				.svc_type = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_hw_info_expected_data,
		},
		{
			.name = "RPMI_BASE_SRV_GET_ATTRIBUTES",
			.attrs = {
				.svc_grp_id = RPMI_SRVGRP_BASE,
				.svc_id = RPMI_BASE_SRV_GET_ATTRIBUTES,
				.svc_type = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_expected_data = init_attribs_expected_data,
		},
		{
			.name = "RPMI_BASE_SRV_SET_MSI",
			.attrs = {
				.svc_grp_id = RPMI_SRVGRP_BASE,
				.svc_id = RPMI_BASE_SRV_SET_MSI,
				.svc_type = RPMI_MSG_NORMAL_REQUEST,
			},
			.init_request_data = init_set_msi_request_data,
			.init_expected_data = init_set_msi_expected_data,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test Base Service Group\n");

	/* Execute default scenario */
	return test_scenario_execute(&scenario_base_default);
}
