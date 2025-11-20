// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "librpmi_internal_list.h"

#ifdef LIBRPMI_DEBUG
#define PREFIX_STR  "== LIBRPMI: MM: =========> %24s: %03u: "

#define DPRINTF(msg...)							\
	{								\
		rpmi_env_printf(PREFIX_STR, __func__, __LINE__);	\
		rpmi_env_printf(msg);					\
		rpmi_env_printf("\n");					\
	}
#else
#define DPRINTF(msg...)
#endif

/* RPMI Spec MM Version : Not explicitly defined in Spec as such */
#define RPMI_MM_MAJOR_VER   (0x1UL)
#define RPMI_MM_MINOR_VER   0x0

/**
 * Note: A change is required in RISC-V EDK2 MMcommunicate.h as it defines the
 * MM_MAJOR_VER_MASK as 0xEFFF0000 => The top nibble is E and not F: why?
 */
#define MM_MAJOR_VER_MASK   0xFFFF0000
#define MM_MINOR_VER_MASK   0x0000FFFF
#define MM_MAJOR_VER_SHIFT  16

struct rpmi_mm_service_linklist {
	rpmi_uint16_t num_entries;
	struct rpmi_mm_service *srvlist;
	struct rpmi_dlist node;
};

struct rpmi_service_group_mm {
	rpmi_uint32_t mm_version;
	struct rpmi_shmem *shmem;
	struct rpmi_dlist srvlist_head;
	rpmi_uint16_t num_mm_srvunits;
	struct rpmi_service_group group;
};

rpmi_uint64_t get_mm_group_shmem_address(struct rpmi_service_group *grp);

static rpmi_uint16_t get_list_index(rpmi_uint16_t num_entries,
				    struct rpmi_mm_service *list,
				    const struct rpmi_guid_t *guid)
{
	rpmi_uint16_t i;

	for (i = 0; i < num_entries; i++) {
		if (rpmi_env_memcmp(&list[i].guid, (void *)guid, GUID_LENGTH))
			continue;

		return i;
	}

	return num_entries;
}

static struct rpmi_mm_service
*get_mm_service_unit(struct rpmi_service_group_mm *sgmm,
		     const struct rpmi_guid_t *guid)
{
	struct rpmi_mm_service_linklist *entry;
	rpmi_uint16_t j;

	rpmi_list_for_each_entry(entry, &sgmm->srvlist_head, node) {
		j = get_list_index(entry->num_entries, entry->srvlist, guid);

		if (j < entry->num_entries)
			return &entry->srvlist[j];
	}

	return NULL;
}

enum rpmi_error rpmi_mm_service_register(struct rpmi_service_group *group,
					 rpmi_uint32_t num_entries,
					 struct rpmi_mm_service *iplist)
{
	struct rpmi_service_group_mm *sgmm;
	struct rpmi_mm_service_linklist *entry;
	struct rpmi_mm_service *nlist;
	struct rpmi_mm_service *temp;
	rpmi_uint16_t i, j;

	/* Critical parameter should be non-NULL */
	if (!group || !iplist || !num_entries) {
		DPRINTF("invalid parameter: pointer is NULL / zero entries");
		return RPMI_ERR_INVALID_PARAM;
	}

	sgmm = group->priv;

	/* Allocate memory for new list to be registered */
	nlist = rpmi_env_zalloc(num_entries * sizeof(*nlist));
	if (!nlist) {
		DPRINTF("failed to allocate MM GUID list");
		return RPMI_ERR_DENIED;
	}

	/* Add the first entry to the new list straight away */
	rpmi_env_memcpy(nlist, iplist, sizeof(*nlist));

	/* Check that the input list does not have duplicate GUID values */
	for (i = 1; i < num_entries; i++) {
		j = get_list_index(i, nlist, &iplist[i].guid);

		if (i != j) {
			DPRINTF("Duplicate GUID found: ignoring given list");
			rpmi_env_free(nlist);
			return RPMI_ERR_INVALID_PARAM;
		}

		rpmi_env_memcpy(nlist + i, iplist + i, sizeof(*nlist));
	}

	DPRINTF("No conflict within given list");
	DPRINTF("Checking existing lists for non-conflict");

	/* Now check that the input list has no conflict with existing list */
	rpmi_list_for_each_entry(entry, &sgmm->srvlist_head, node) {
		DPRINTF("entry->num_entries: %d", entry->num_entries);

		for (i = 0; i < num_entries; i++) {
			temp = get_mm_service_unit(sgmm, &iplist[i].guid);

			if (temp) {
				DPRINTF("GUID conflict with existing list: "
					"ignoring given list");
				rpmi_env_free(nlist);
				return RPMI_ERR_INVALID_PARAM;
			}
		}
	}

	/* Allocate memory for appending list to the linked list */
	entry = rpmi_env_zalloc(sizeof(*entry));
	if (!entry) {
		DPRINTF("failed to allocate memory for new node");
		rpmi_env_free(nlist);
		return RPMI_ERR_DENIED;
	}

	DPRINTF("Adding current list %u entries to the group", num_entries);

	entry->num_entries = num_entries;
	entry->srvlist = nlist;
	RPMI_INIT_LIST_HEAD(&entry->node);
	rpmi_list_add_tail(&entry->node, &sgmm->srvlist_head);

	sgmm->num_mm_srvunits += num_entries;
	DPRINTF("MM group total service units = %u", sgmm->num_mm_srvunits);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_mm_get_attributes(struct rpmi_service_group *group,
					      struct rpmi_service *service,
					      struct rpmi_transport *xport,
					      rpmi_uint16_t request_datalen,
					      const rpmi_uint8_t *request_data,
					      rpmi_uint16_t *response_datalen,
					      rpmi_uint8_t *response_data)
{
	struct rpmi_service_group_mm *sgmm = group->priv;
	rpmi_uint32_t *rsp = (void *)response_data;
	rpmi_uint64_t shmem_base;
	rpmi_uint32_t shmem_size;
	enum rpmi_error status;

	if (sgmm && response_datalen) {
		shmem_base = rpmi_shmem_base(sgmm->shmem);
		shmem_size = rpmi_shmem_size(sgmm->shmem);

		rsp[1] = rpmi_to_xe32(xport->is_be, sgmm->mm_version);
		rsp[2] = rpmi_to_xe32(xport->is_be, (rpmi_uint32_t)
				      (shmem_base & 0xFFFFFFFF));
		rsp[3] = rpmi_to_xe32(xport->is_be, shmem_base >> 32);
		rsp[4] = rpmi_to_xe32(xport->is_be, shmem_size);

		*response_datalen = 5 * sizeof(rpmi_uint32_t);
		status = RPMI_SUCCESS;
	} else {
		status = RPMI_ERR_NO_DATA;
	}

	rsp[0] = rpmi_to_xe32(xport->is_be, (rpmi_int32_t)status);

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_mm_communicate(struct rpmi_service_group *group,
					   struct rpmi_service *service,
					   struct rpmi_transport *xport,
					   rpmi_uint16_t request_datalen,
					   const rpmi_uint8_t *request_data,
					   rpmi_uint16_t *response_datalen,
					   rpmi_uint8_t *response_data)
{
	struct rpmi_service_group_mm *sgmm = group->priv;
	rpmi_uint32_t *rsp = (void *)response_data;
	struct rpmi_mm_comm_req *mmc_req;
	struct rpmi_mm_service *srvunit;
	struct rpmi_guid_t guid;
	enum rpmi_error status;

	if (!request_data || !sgmm)
		return RPMI_ERR_NO_DATA;

	DPRINTF("ENTER");

	mmc_req = (struct rpmi_mm_comm_req *)request_data;
	rpmi_shmem_read(sgmm->shmem, mmc_req->idata_off, &guid, GUID_LENGTH);

	srvunit = get_mm_service_unit(sgmm, &guid);

	if (!srvunit || !srvunit->active_cbfn_p)
		return RPMI_ERR_NO_DATA;

	status = srvunit->active_cbfn_p(sgmm->shmem, request_datalen,
					request_data, response_datalen,
					response_data, srvunit->priv_data);

	rsp[0] = rpmi_to_xe32(xport->is_be, (rpmi_int32_t)status);
	rsp[1] = rpmi_to_xe32(xport->is_be, (rpmi_uint32_t)(*response_datalen));
	*response_datalen = 2 * sizeof(rpmi_uint32_t);

	DPRINTF("EXIT: response length = %u status = %d", rsp[1], status);

	return status;
}

/* Keep entry index same as service_id value */
static struct rpmi_service rpmi_mm_services[RPMI_MM_SRV_ID_MAX] = {
	[RPMI_MM_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_MM_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = NULL,
	},
	[RPMI_MM_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_MM_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_mm_get_attributes,
	},
	[RPMI_MM_SRV_COMMUNICATE] = {
		.service_id = RPMI_MM_SRV_COMMUNICATE,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_mm_communicate,
	},
};

struct rpmi_service_group *rpmi_service_group_mm_create(struct rpmi_shmem *shm)
{
	struct rpmi_service_group_mm *sgmm;
	struct rpmi_service_group *group;

	/* Critical parameter should be non-NULL */
	if (!shm) {
		DPRINTF("invalid parameter: shared memory pointer is NULL");
		return NULL;
	}

	/* Allocate MM service group */
	sgmm = rpmi_env_zalloc(sizeof(*sgmm));
	if (!sgmm) {
		DPRINTF("failed to allocate MM service group instance");
		return NULL;
	}

	sgmm->mm_version =
	    ((RPMI_MM_MAJOR_VER << MM_MAJOR_VER_SHIFT) & MM_MAJOR_VER_MASK) |
	    ((RPMI_MM_MINOR_VER & MM_MINOR_VER_MASK));
	sgmm->shmem = shm;
	RPMI_INIT_LIST_HEAD(&sgmm->srvlist_head);

	group = &sgmm->group;
	group->name = "mm";
	group->servicegroup_id = RPMI_SRVGRP_MANAGEMENT_MODE;
	group->servicegroup_version =
	    RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed only for M-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK;
	group->max_service_id = RPMI_MM_SRV_ID_MAX;
	group->services = rpmi_mm_services;
	group->process_events = NULL;
	group->lock = rpmi_env_alloc_lock();
	group->priv = sgmm;

	return group;
}

void rpmi_service_group_mm_destroy(struct rpmi_service_group *group)
{
	struct rpmi_mm_service_linklist *entry;
	struct rpmi_service_group_mm *sgmm;
	struct rpmi_mm_service *srvunit;
	rpmi_uint16_t j;

	if (!group) {
		DPRINTF("invalid parameters");
		return;
	}

	sgmm = group->priv;

	DPRINTF("Initiating cleanup ...");

	rpmi_list_for_each_entry(entry, &sgmm->srvlist_head, node) {
		for (j = 0; j < entry->num_entries; j++) {
			srvunit = &entry->srvlist[j];

			if (!srvunit || !srvunit->delete_cbfn_p)
				continue;

			srvunit->delete_cbfn_p(sgmm->shmem, 0, NULL, NULL, NULL,
					       srvunit->priv_data);
		}
	}

	rpmi_env_free(sgmm);
}
