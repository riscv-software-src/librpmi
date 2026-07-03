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

/*
 * Maximum dequeue attempts before declaring a wait failure.  The transport is
 * purely in-memory and synchronous: if process_all_events / process_a2p_request
 * has already run, any enqueued message is immediately visible.  A large bound
 * keeps the tests from hanging while still allowing for future async changes.
 */
#define TEST_DEQUEUE_MAX_RETRIES	1000

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
	int result, retries = 0;

	rpmi_env_memset(msg, 0, scene->slot_size);

	/* Notification arrives on P2A_REQ queue, not P2A_ACK */
	do {
		result = rpmi_transport_dequeue(scene->xport,
						RPMI_QUEUE_P2A_REQ, msg);
		retries++;
	} while (result != RPMI_SUCCESS && retries < TEST_DEQUEUE_MAX_RETRIES);

	if (result != RPMI_SUCCESS)
		printf("%s: timed out waiting for notification on RPMI_QUEUE_P2A_REQ "
		       "after %d retries\n", test->name, TEST_DEQUEUE_MAX_RETRIES);
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

/*
 * Callbacks for the "disable clears pending" scenario.
 *
 * The scenario processes A2P requests manually inside each run callback so
 * that process_all_events is never called implicitly between tests.  This lets
 * us verify that disabling notifications clears pending_request_handle_err:
 * if the fix is absent, a pending error latched while notifications were
 * enabled survives the disable and fires spuriously after re-enable.
 */

static int test_scenario_no_process(struct rpmi_test_scenario *scene)
{
	/* No implicit event processing; tests drive it explicitly in run. */
	return 0;
}

/* run: enqueue request and process the A2P queue only */
static int test_base_notif_clear_run_enqueue(struct rpmi_test_scenario *scene,
					     struct rpmi_test *test,
					     struct rpmi_message *msg)
{
	int rc;

	do {
		rc = rpmi_transport_enqueue(scene->xport, RPMI_QUEUE_A2P_REQ, msg);
	} while (rc == RPMI_ERR_IO);
	if (rc)
		return rc;

	rpmi_context_process_a2p_request(scene->cntx);
	return 0;
}

/*
 * run: latch pending_request_handle_err while notifications are enabled,
 * then enqueue the disable request and process it.  The fix clears the
 * pending flag during disable; without it the flag survives to re-enable.
 */
static int test_base_notif_clear_run_latch_and_disable(struct rpmi_test_scenario *scene,
							struct rpmi_test *test,
							struct rpmi_message *msg)
{
	int rc;

	rpmi_context_base_request_handle_error(scene->cntx);

	do {
		rc = rpmi_transport_enqueue(scene->xport, RPMI_QUEUE_A2P_REQ, msg);
	} while (rc == RPMI_ERR_IO);
	if (rc)
		return rc;

	rpmi_context_process_a2p_request(scene->cntx);
	return 0;
}

/*
 * run: re-enable notifications, then call process_all_events.  With the fix,
 * pending was cleared on disable so no notification fires.  Without the fix,
 * a stale notification is enqueued to P2A_REQ here.
 */
static int test_base_notif_clear_run_reenable(struct rpmi_test_scenario *scene,
					      struct rpmi_test *test,
					      struct rpmi_message *msg)
{
	int rc;

	do {
		rc = rpmi_transport_enqueue(scene->xport, RPMI_QUEUE_A2P_REQ, msg);
	} while (rc == RPMI_ERR_IO);
	if (rc)
		return rc;

	rpmi_context_process_a2p_request(scene->cntx);
	rpmi_context_process_all_events(scene->cntx);
	return 0;
}

/* wait: dequeue the acknowledgment response from P2A_ACK */
static void test_base_notif_clear_wait_ack(struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    struct rpmi_message *msg)
{
	int result, retries = 0;

	rpmi_env_memset(msg, 0, scene->slot_size);

	do {
		result = rpmi_transport_dequeue(scene->xport, RPMI_QUEUE_P2A_ACK, msg);
		retries++;
	} while (result != RPMI_SUCCESS && retries < TEST_DEQUEUE_MAX_RETRIES);

	if (result != RPMI_SUCCESS)
		printf("%s: timed out waiting for acknowledgment on RPMI_QUEUE_P2A_ACK "
		       "after %d retries\n", test->name, TEST_DEQUEUE_MAX_RETRIES);
}

/* verify: P2A_REQ must be empty — no spurious notification should have been sent */
static int test_base_notif_clear_no_spurious_verify(struct rpmi_test_scenario *scene,
						     struct rpmi_test *test,
						     struct rpmi_message *msg)
{
	struct rpmi_message *notif_msg;
	int rc, failed = 0;

	notif_msg = rpmi_env_zalloc(scene->slot_size);
	if (!notif_msg)
		return 1;

	rc = rpmi_transport_dequeue(scene->xport, RPMI_QUEUE_P2A_REQ, notif_msg);
	if (rc == RPMI_SUCCESS) {
		printf("%s: spurious REQUEST_HANDLE_ERROR notification delivered after disable\n",
		       test->name);
		failed = 1;
	}

	rpmi_env_free(notif_msg);
	return failed;
}

/*
 * run: trigger an error while notifications are disabled, then re-enable and
 * run process_all_events.  With the fix, the error is dropped at latch time
 * (notif_enabled=false) so no notification fires on re-enable.
 */
static int test_base_notif_drop_run_trigger_and_reenable(struct rpmi_test_scenario *scene,
							  struct rpmi_test *test,
							  struct rpmi_message *msg)
{
	int rc;

	/* Error occurs while notifications are still disabled */
	rpmi_context_base_request_handle_error(scene->cntx);

	do {
		rc = rpmi_transport_enqueue(scene->xport, RPMI_QUEUE_A2P_REQ, msg);
	} while (rc == RPMI_ERR_IO);
	if (rc)
		return rc;

	rpmi_context_process_a2p_request(scene->cntx);
	rpmi_context_process_all_events(scene->cntx);
	return 0;
}

static struct rpmi_test_scenario scenario_base_notif_drop_while_disabled = {
	.name = "Base Notification Drop While Disabled",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.base.plat_info_len = PLAT_INFO_LEN,
	.base.plat_info = PLAT_INFO,

	.init = test_scenario_default_init,
	.process = test_scenario_no_process,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 3,
	.tests = {
		{
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION (enable)",
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
			.run = test_base_notif_clear_run_enqueue,
			.wait = test_base_notif_clear_wait_ack,
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
			.run = test_base_notif_clear_run_enqueue,
			.wait = test_base_notif_clear_wait_ack,
		},
		{
			/*
			 * Trigger an error while disabled, then re-enable.  The fix
			 * drops errors at latch time when notif_enabled=false, so the
			 * P2A_REQ queue must remain empty after process_all_events.
			 */
			.name = "RPMI_BASE no spurious notification for error-while-disabled",
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
			.run = test_base_notif_drop_run_trigger_and_reenable,
			.wait = test_base_notif_clear_wait_ack,
			.verify = test_base_notif_clear_no_spurious_verify,
		},
	},
};

static struct rpmi_test_scenario scenario_base_notif_clear_on_disable = {
	.name = "Base Notification Clear On Disable",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.base.plat_info_len = PLAT_INFO_LEN,
	.base.plat_info = PLAT_INFO,

	.init = test_scenario_default_init,
	.process = test_scenario_no_process,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 3,
	.tests = {
		{
			/* Enable notifications so the error is latched while enabled */
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION (enable)",
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
			.run = test_base_notif_clear_run_enqueue,
			.wait = test_base_notif_clear_wait_ack,
		},
		{
			/*
			 * Latch pending_request_handle_err while notifications are
			 * enabled, then disable.  The fix must clear the pending flag
			 * here so re-enable does not trigger a spurious delivery.
			 */
			.name = "RPMI_BASE REQUEST_HANDLE_ERROR pending cleared on disable",
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
			.run = test_base_notif_clear_run_latch_and_disable,
			.wait = test_base_notif_clear_wait_ack,
		},
		{
			/*
			 * Re-enable and call process_all_events.  If the pending flag
			 * was correctly cleared on disable, P2A_REQ stays empty.
			 */
			.name = "RPMI_BASE no spurious notification after disable-clears-pending",
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
			.run = test_base_notif_clear_run_reenable,
			.wait = test_base_notif_clear_wait_ack,
			.verify = test_base_notif_clear_no_spurious_verify,
		},
	},
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

/*
 * Test scenario for a transport without a P2A channel (p2a_req_queue_size=0).
 * GET_ATTRIBUTES must not advertise EV_NOTIFY, and ENABLE_NOTIFICATION must
 * return RPMI_ERR_NOTSUPP.
 */
static int test_scenario_no_p2a_init(struct rpmi_test_scenario *scene)
{
	if (!scene || scene->shm || scene->shmem || scene->xport || scene->cntx)
		return RPMI_ERR_ALREADY;

	scene->shm = rpmi_env_zalloc(scene->shm_size);
	if (!scene->shm)
		return RPMI_ERR_FAILED;

	scene->shmem = rpmi_shmem_create("test_shmem_no_p2a",
					 (unsigned long)scene->shm, scene->shm_size,
					 &rpmi_shmem_simple_ops, NULL);
	if (!scene->shmem) {
		printf("%s: failed to create test rpmi_shmem\n", __func__);
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
		return RPMI_ERR_FAILED;
	}

	/* p2a_req_queue_size=0: A2P-only transport, is_p2a_channel=false */
	scene->xport = rpmi_transport_shmem_create("test_transport_no_p2a",
						   scene->slot_size,
						   scene->shm_size / 2,
						   0,
						   scene->shmem);
	if (!scene->xport) {
		printf("%s: failed to create test rpmi_transport\n", __func__);
		rpmi_shmem_destroy(scene->shmem);
		scene->shmem = NULL;
		rpmi_env_free(scene->shm);
		scene->shm = NULL;
		return RPMI_ERR_FAILED;
	}

	scene->cntx = rpmi_context_create("test_context_no_p2a", scene->xport,
					  scene->max_num_groups,
					  RPMI_PRIVILEGE_M_MODE,
					  scene->base.plat_info_len,
					  scene->base.plat_info);
	if (!scene->cntx) {
		printf("%s: failed to create test rpmi_context\n", __func__);
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

/* GET_ATTRIBUTES: EV_NOTIFY must be absent when there is no P2A channel */
static rpmi_uint32_t attribs_expdata_no_p2a[] = {
	RPMI_SUCCESS,
	RPMI_BASE_FLAGS_F0_PRIVILEGE, /* no EV_NOTIFY */
	0,
	0,
	0,
};

/* ENABLE_NOTIFICATION must fail with NOTSUPP when there is no P2A channel */
static rpmi_uint32_t enable_notif_expdata_no_p2a[] = {
	RPMI_ERR_NOTSUPP,
};

static struct rpmi_test_scenario scenario_base_no_p2a_channel = {
	.name = "Base Service Group No P2A Channel",
	.shm_size = RPMI_SHM_SZ,
	.slot_size = RPMI_SLOT_SIZE,
	.max_num_groups = RPMI_SRVGRP_ID_MAX_COUNT,
	.base.plat_info_len = PLAT_INFO_LEN,
	.base.plat_info = PLAT_INFO,

	.init = test_scenario_no_p2a_init,
	.cleanup = test_scenario_default_cleanup,

	.num_tests = 2,
	.tests = {
		{
			.name = "RPMI_BASE_SRV_GET_ATTRIBUTES (no P2A channel)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_GET_ATTRIBUTES,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.expected_data = attribs_expdata_no_p2a,
				.expected_data_len = sizeof(attribs_expdata_no_p2a),
			},
			.init_expected_data = test_init_expected_data_from_attrs,
		},
		{
			.name = "RPMI_BASE_SRV_ENABLE_NOTIFICATION (no P2A channel)",
			.attrs = {
				.servicegroup_id = RPMI_SRVGRP_BASE,
				.service_id = RPMI_BASE_SRV_ENABLE_NOTIFICATION,
				.flags = RPMI_MSG_NORMAL_REQUEST,
				.request_data = enable_notif_reqdata_default,
				.request_data_len = sizeof(enable_notif_reqdata_default),
				.expected_data = enable_notif_expdata_no_p2a,
				.expected_data_len = sizeof(enable_notif_expdata_no_p2a),
			},
			.init_request_data = test_init_request_data_from_attrs,
			.init_expected_data = test_init_expected_data_from_attrs,
		},
	},
};

int main(int argc, char *argv[])
{
	int rc;

	printf("Test Base Service Group\n");

	rc = test_scenario_execute(&scenario_base_default);
	if (rc)
		return rc;

	rc = test_scenario_execute(&scenario_base_notif_clear_on_disable);
	if (rc)
		return rc;

	rc = test_scenario_execute(&scenario_base_notif_drop_while_disabled);
	if (rc)
		return rc;

	return test_scenario_execute(&scenario_base_no_p2a_channel);
}
