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

static rpmi_uint32_t enable_notif_reqdata_default[] = {
	RPMI_BASE_EVENT_REQUEST_HANDLE_ERROR,
	1, /* Enable notification */
};

static rpmi_uint32_t enable_notif_expdata_default[] = {
	RPMI_SUCCESS,
	1, /* Current state: enabled */
};

static rpmi_uint32_t disable_notif_reqdata_default[] = {
	RPMI_BASE_EVENT_REQUEST_HANDLE_ERROR,
	0, /* Disable notification */
};

static rpmi_uint32_t disable_notif_expdata_default[] = {
	RPMI_SUCCESS,
	0, /* Current state: disabled */
};

static rpmi_uint32_t query_notif_reqdata_default[] = {
	RPMI_BASE_EVENT_REQUEST_HANDLE_ERROR,
	2, /* Query current state */
};

/* Query runs after disable, so current state is 0 */
static rpmi_uint32_t query_notif_expdata_default[] = {
	RPMI_SUCCESS,
	0,
};

static rpmi_uint32_t invalid_event_notif_reqdata_default[] = {
	0xFF, /* Invalid event ID */
	1,
};

static rpmi_uint32_t invalid_event_notif_expdata_default[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t invalid_state_notif_reqdata_default[] = {
	RPMI_BASE_EVENT_REQUEST_HANDLE_ERROR,
	3, /* Invalid req_state */
};

static rpmi_uint32_t invalid_state_notif_expdata_default[] = {
	RPMI_ERR_INVALID_PARAM,
};

static rpmi_uint32_t notif_event_expdata_default[] = {
	RPMI_BASE_EVENT_REQUEST_HANDLE_ERROR,
	0, /* Reserved event-specific data */
};

static int test_base_notif_run(struct rpmi_test_scenario *scene,
			       struct rpmi_test *test,
			       struct rpmi_message *msg)
{
	/* Re-enable notification before triggering (disabled by prior test) */
	rpmi_context_base_request_handle_error(scene->cntx);
	rpmi_context_process_all_events(scene->cntx);
	return 0;
}

static void test_base_notif_wait(struct rpmi_test_scenario *scene,
				 struct rpmi_test *test,
				 struct rpmi_message *msg)
{
	int result = -1;

	rpmi_env_memset(msg, 0, scene->slot_size);

	/* Notification arrives on P2A_REQ queue, not P2A_ACK */
	while (result != RPMI_SUCCESS)
		result = rpmi_transport_dequeue(scene->xport,
						RPMI_QUEUE_P2A_REQ, msg);
}

static int test_base_notif_verify(struct rpmi_test_scenario *scene,
				  struct rpmi_test *test,
				  struct rpmi_message *msg)
{
	int failed = 0;

	if (msg->header.flags != RPMI_MSG_NOTIFICATION) {
		printf("%s: expected NOTIFICATION flags, got 0x%x\n",
		       test->name, msg->header.flags);
		failed = 1;
	}

	if (msg->header.servicegroup_id != RPMI_SRVGRP_BASE) {
		printf("%s: expected servicegroup_id %d, got %d\n",
		       test->name, RPMI_SRVGRP_BASE, msg->header.servicegroup_id);
		failed = 1;
	}

	return failed;
}

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

static rpmi_uint32_t probe_sysreset_reqdata_default[] = {
	RPMI_SRVGRP_SYSTEM_RESET,
};

static rpmi_uint32_t probe_unsupported_expdata_default[] = {
	RPMI_SUCCESS,
	0,
};

static rpmi_uint32_t probe_experimental_reqdata_default[] = {
	RPMI_SRVGRP_EXPERIMENTAL_START,
};

static rpmi_uint32_t probe_vendor_reqdata_default[] = {
	RPMI_SRVGRP_VENDOR_START,
};

static rpmi_uint32_t invalid_service_expdata_default[] = {
	RPMI_ERR_NOTSUPP,
};

static rpmi_uint32_t attribs_expdata_default[] = {
	RPMI_SUCCESS,
	RPMI_BASE_FLAGS_F0_PRIVILEGE | RPMI_BASE_FLAGS_F0_EV_NOTIFY,
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

	.num_tests = 17,
	.tests = {
		{
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = enable_notif_reqdata_default,
				.request_data_len = sizeof(enable_notif_reqdata_default),
				.expected_data = enable_notif_expdata_default,
				.expected_data_len = sizeof(enable_notif_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
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
			.name = "RPMI_BASE_SRV_PROBE_SERVICE_GROUP unsupported",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_PROBE_SERVICE_GROUP,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_sysreset_reqdata_default,
				.request_data_len = sizeof(probe_sysreset_reqdata_default),
				.expected_data = probe_unsupported_expdata_default,
				.expected_data_len = sizeof(probe_unsupported_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_PROBE_SERVICE_GROUP experimental",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_PROBE_SERVICE_GROUP,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_experimental_reqdata_default,
				.request_data_len = sizeof(probe_experimental_reqdata_default),
				.expected_data = probe_unsupported_expdata_default,
				.expected_data_len = sizeof(probe_unsupported_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_PROBE_SERVICE_GROUP vendor",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_PROBE_SERVICE_GROUP,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = probe_vendor_reqdata_default,
				.request_data_len = sizeof(probe_vendor_reqdata_default),
				.expected_data = probe_unsupported_expdata_default,
				.expected_data_len = sizeof(probe_unsupported_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE invalid service",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ID_MAX,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = invalid_service_expdata_default,
				.expected_data_len = sizeof(invalid_service_expdata_default),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION (disable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = disable_notif_reqdata_default,
				.request_data_len = sizeof(disable_notif_reqdata_default),
				.expected_data = disable_notif_expdata_default,
				.expected_data_len = sizeof(disable_notif_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION (query state)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = query_notif_reqdata_default,
				.request_data_len = sizeof(query_notif_reqdata_default),
				.expected_data = query_notif_expdata_default,
				.expected_data_len = sizeof(query_notif_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION (invalid event ID)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_event_notif_reqdata_default,
				.request_data_len = sizeof(invalid_event_notif_reqdata_default),
				.expected_data = invalid_event_notif_expdata_default,
				.expected_data_len = sizeof(invalid_event_notif_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION (invalid req_state)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = invalid_state_notif_reqdata_default,
				.request_data_len = sizeof(invalid_state_notif_reqdata_default),
				.expected_data = invalid_state_notif_expdata_default,
				.expected_data_len = sizeof(invalid_state_notif_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			/* Re-enable so the notification event test can fire */
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION (re-enable)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = enable_notif_reqdata_default,
				.request_data_len = sizeof(enable_notif_reqdata_default),
				.expected_data = enable_notif_expdata_default,
				.expected_data_len = sizeof(enable_notif_expdata_default),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE REQUEST_HANDLE_ERROR notification event",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = 0,
				.flags = RPMI_MSG_NOTIFICATION,
				.expected_data = notif_event_expdata_default,
				.expected_data_len = sizeof(notif_event_expdata_default),
			},
			.run = test_base_notif_run,
			.wait = test_base_notif_wait,
			.verify = test_base_notif_verify,
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
