/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#ifndef __RPMI_TEST_COMMON__
#define __RPMI_TEST_COMMON__

#include <stdio.h>
#include <librpmi.h>
#include <stdlib.h>

/*
 * Qemu shared memory offsets.
 * its better to decode these data from DTS file, hardcoding for now.
 */

#define RPMI_SHM_SZ (8 * 1024)

#define RPMI_SLOT_SIZE          64
#define RPMI_MSG_MAX_DATA_SIZE (RPMI_SLOT_SIZE -  RPMI_MSG_HDR_SIZE)

#define RPMI_MIN_QUEUE_SIZE             (16 * RPMI_SLOT_SIZE)
#define RPMI_MIN_TOTAL_QUEUES_SIZE      (RPMI_QUEUE_MAX * \
					 RPMI_MIN_QUEUE_SIZE)
#define RPMI_MAX_SERVICE_GROUPS 16

#define NUM_XPORTS_PER_SOC	(0)
#define NUM_XPORTS(num_socks)	(num_socks + NUM_XPORTS_PER_SOC)

struct rpmi_test_scenario;

struct rpmi_test {
	char name[64];
	int arg0;
	int resp0;
	struct {
		unsigned int svc_grp_id;
		unsigned int svc_id;
		unsigned int svc_type;
	} attrs;
	int query_data_len;
	unsigned char query_data[RPMI_MSG_MAX_DATA_SIZE];
	int resp_data_len;
	unsigned char resp_data[RPMI_MSG_MAX_DATA_SIZE];

	/* test specific function handlers*/
	int (*init)(struct rpmi_test_scenario *scene, struct rpmi_test *test);
	int (*run)(struct rpmi_test_scenario *scene, struct rpmi_test *test,
		   struct rpmi_message *msg);
	int (*wait)(struct rpmi_test_scenario *scene, struct rpmi_test *test,
		    struct rpmi_message *msg);
	int (*verify)(struct rpmi_test_scenario *scene, struct rpmi_test *test,
		      struct rpmi_message *msg);
	int (*cleanup)(struct rpmi_test_scenario *scene, struct rpmi_test *test);
};

struct rpmi_test_scenario {
	char name[64];
	struct group_config *conf;
	struct rpmi_context *rctx;
	struct rpmi_transport *xport;

	/* group specific function handlers*/
	int (*init)(struct rpmi_test_scenario *scene);
	int (*process)(struct rpmi_test_scenario *scene);
	int (*cleanup)(struct rpmi_test_scenario *scene);

	int num_tests;
	struct rpmi_test tests[];
};

struct rpmi_transport *init_test_rpmi_xport(int do_clear);
int execute_test_scenario(struct rpmi_test_scenario *scene);

#endif
