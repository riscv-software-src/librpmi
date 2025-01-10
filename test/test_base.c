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

#define PLAT_INFO	"ventana veyron-v2 plat 1.0"
#define PLAT_INFO_LEN	(sizeof(PLAT_INFO))

static struct plat_info {
	rpmi_uint32_t status;
	rpmi_uint32_t plat_id_len;
	rpmi_uint8_t plat_info[];
} plat_info_val = {
	.plat_id_len = PLAT_INFO_LEN,
	.plat_info = PLAT_INFO,
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
	RPMI_BASE_VERSION(1, 0),
};

static rpmi_uint32_t attribs_expdata_default[] = {
	RPMI_SUCCESS,
	RPMI_BASE_FLAGS_F0_PRIVILEGE,
	0,
	0,
	0,
};

static struct rpmi_test_scenario scenario_base_default = {
	.name = "Base Service Group Default",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.base.plat_info_len = PLAT_INFO_LEN,
	.base.plat_info = PLAT_INFO,
	.priv = NULL,

	.init = test_scenario_default_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 7,
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
			.name = "RPMI_BASE_SRV_GET_PLATFORM_INFO",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_GET_PLATFORM_INFO,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = &plat_info_val,
				.expected_data_len = PLAT_INFO_LEN + sizeof(rpmi_uint32_t)*2,
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
	},
};

int main(int argc, char *argv[])
{
	printf("Test Base Service Group\n");

	/* Execute default scenario */
	return test_scenario_execute(&scenario_base_default);
}
