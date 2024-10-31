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

#define RPMI_SHM_SZ		(8 * 1024)

#define RPMI_SLOT_SIZE		64

struct rpmi_test_scenario;

struct rpmi_test {
	/* ==== Public members  ===== */

	char name[64];
	struct {
		rpmi_uint16_t servicegroup_id;
		rpmi_uint8_t service_id;
		rpmi_uint8_t flags;

		const void *request_data;
		rpmi_uint16_t request_data_len;

		const void *expected_data;
		rpmi_uint16_t expected_data_len;
	} attrs;
	void *priv;

	/* test specific function handlers*/
	int (*init)(struct rpmi_test_scenario *scene, struct rpmi_test *test);
	rpmi_uint16_t (*init_request_data)(struct rpmi_test_scenario *scene,
					   struct rpmi_test *test,
					   void *data, rpmi_uint16_t max_data_len);
	rpmi_uint16_t (*init_expected_data)(struct rpmi_test_scenario *scene,
					    struct rpmi_test *test,
					    void *data, rpmi_uint16_t max_data_len);
	int (*run)(struct rpmi_test_scenario *scene, struct rpmi_test *test,
		   struct rpmi_message *msg);
	void (*wait)(struct rpmi_test_scenario *scene, struct rpmi_test *test,
		    struct rpmi_message *msg);
	void (*cleanup)(struct rpmi_test_scenario *scene, struct rpmi_test *test);
};

struct rpmi_test_scenario {
	/* ==== Private members  ===== */

	void *shm;
	struct rpmi_shmem *shmem;
	struct rpmi_transport *xport;
	struct rpmi_context *cntx;
	rpmi_uint16_t token_sequence;

	/* ==== Public members  ===== */

	char name[64];
	rpmi_uint32_t shm_size;
	rpmi_uint32_t slot_size;
	rpmi_uint32_t max_num_groups;
	struct {
		rpmi_uint32_t plat_info_len;
		const char *plat_info;
	} base;
	void *priv;

	/* Scenario specific callbacks */
	int (*init)(struct rpmi_test_scenario *scene);
	int (*process)(struct rpmi_test_scenario *scene);
	int (*cleanup)(struct rpmi_test_scenario *scene);

	int num_tests;
	struct rpmi_test tests[];
};

rpmi_uint16_t test_init_request_data_from_attrs(struct rpmi_test_scenario *scene,
						struct rpmi_test *test,
						void *data, rpmi_uint16_t max_data_len);
rpmi_uint16_t test_init_expected_data_from_attrs(struct rpmi_test_scenario *scene,
						 struct rpmi_test *test,
						 void *data, rpmi_uint16_t max_data_len);

int test_scenario_default_init(struct rpmi_test_scenario *scene);
int test_scenario_default_cleanup(struct rpmi_test_scenario *scene);

int test_scenario_execute(struct rpmi_test_scenario *scene);

#endif
