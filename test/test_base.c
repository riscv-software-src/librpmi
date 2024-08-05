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

static rpmi_uint32_t enable_notif_expdata_default[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t impl_ver_expdata_default[] = {
	RPMI_SUCCESS,
	RPMI_BASE_VERSION(LIBRPMI_IMPL_VERSION_MAJOR, LIBRPMI_IMPL_VERSION_MINOR),
};

static rpmi_uint32_t impl_idn_expdata_default[] = {
	RPMI_SUCCESS,
	LIBRPMI_IMPL_ID,
};

static rpmi_uint32_t spec_ver_expdata_default[] = {
	RPMI_SUCCESS,
	RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR),
};

static rpmi_uint32_t probe_reqdata_default[] = {
	RPMI_SRVGRP_BASE,
};

static rpmi_uint32_t probe_expdata_default[] = {
	RPMI_SUCCESS,
	1,
};

static rpmi_uint32_t attribs_expdata_default[] = {
	RPMI_SUCCESS,
	RPMI_BASE_FLAGS_F0_MSI_EN,
	0,
	0,
	0,
};

static rpmi_uint32_t set_msi_reqdata_default[] = {
	0, /* MSI addr_lo */
	0, /* MSI addr_hi */
	0, /* MSI data */
};

static rpmi_uint32_t set_msi_expdata_default[] = {
	RPMI_SUCCESS,
};

static struct rpmi_test_scenario scenario_base_default = {
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
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = enable_notif_expdata_default,
				.expected_data_len = sizeof(enable_notif_expdata_default),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = impl_ver_expdata_default,
				.expected_data_len = sizeof(impl_ver_expdata_default),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = impl_idn_expdata_default,
				.expected_data_len = sizeof(impl_idn_expdata_default),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_GET_SPEC_VERSION",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_GET_SPEC_VERSION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = spec_ver_expdata_default,
				.expected_data_len = sizeof(spec_ver_expdata_default),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_GET_HW_INFO",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_GET_HW_INFO,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = &hw_info_val,
				.expected_data_len = sizeof(hw_info_val),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_PROBE_SERVICE_GROUP",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_PROBE_SERVICE_GROUP,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_reqdata_default,
				.request_data_len = sizeof(probe_reqdata_default),
				.expected_data = probe_expdata_default,
				.expected_data_len = sizeof(probe_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_GET_ATTRIBUTES",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = attribs_expdata_default,
				.expected_data_len = sizeof(attribs_expdata_default),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_SET_MSI",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_SET_MSI,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = set_msi_reqdata_default,
				.request_data_len = sizeof(set_msi_reqdata_default),
				.expected_data = set_msi_expdata_default,
				.expected_data_len = sizeof(set_msi_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	printf("Test Base Service Group\n");

	/* Execute default scenario */
	return test_scenario_execute(&scenario_base_default);
}
