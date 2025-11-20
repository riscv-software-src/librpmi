// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include <librpmi_mm_efi.h>
#include "librpmi_internal.h"

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

static rpmi_uint8_t payload_buffer[MAX_PAYLOAD_SIZE];
static rpmi_uint8_t msg_buffer[MAX_VARINFO_SIZE];

#ifdef LIBRPMI_DEBUG

static rpmi_uint16_t efi_calls_counter = 0;

#define STRING_CASE(x)  case x: return #x

static const char *get_hdr_guid_string(enum mm_efi_header_guid guid)
{
	switch (guid) {
		STRING_CASE(MM_EFI_VAR_PROTOCOL_GUID);
		STRING_CASE(MM_EFI_VAR_POLICY_GUID);
		STRING_CASE(MM_EFI_END_OF_DXE_GUID);
		STRING_CASE(MM_EFI_READY_TO_BOOT_GUID);
		STRING_CASE(MM_EFI_EXIT_BOOT_SVC_GUID);

	default:
		STRING_CASE(MM_EFI_HDR_GUID_UNSUPPORTED);
	}
}

static const char *null_string = "NULL";

static const char *get_var_fn_string(rpmi_uint32_t function_code)
{
	switch (function_code) {
		STRING_CASE(EFI_VAR_FN_GET_VARIABLE);
		STRING_CASE(EFI_VAR_FN_GET_NEXT_VARIABLE_NAME);
		STRING_CASE(EFI_VAR_FN_SET_VARIABLE);
		STRING_CASE(EFI_VAR_FN_QUERY_VARIABLE_INFO);
		STRING_CASE(EFI_VAR_FN_READY_TO_BOOT);
		STRING_CASE(EFI_VAR_FN_EXIT_BOOT_SERVICE);
		STRING_CASE(EFI_VAR_FN_GET_STATISTICS);
		STRING_CASE(EFI_VAR_FN_LOCK_VARIABLE);
		STRING_CASE(EFI_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_SET);
		STRING_CASE(EFI_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_GET);
		STRING_CASE(EFI_VAR_FN_GET_PAYLOAD_SIZE);
		STRING_CASE(EFI_VAR_FN_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT);
		STRING_CASE(EFI_VAR_FN_SYNC_RUNTIME_CACHE);
		STRING_CASE(EFI_VAR_FN_GET_RUNTIME_CACHE_INFO);

	default:
		return null_string;
	}
}

#endif /* LIBRPMI_DEBUG */

static rpmi_uint64_t validate_input(struct efi_var_comm_header *comm_hdr,
				    rpmi_uint32_t payload_size,
				    rpmi_bool_t is_context_get_variable)
{
	struct efi_var_access_variable *var;
	rpmi_uint64_t infosize;

	if (payload_size < offsetof(struct efi_var_access_variable, name)) {
		DPRINTF("MM communication buffer size invalid !!!");
		return EFI_INVALID_PARAMETER;
	}

	/* Copy the input communicate buffer payload to local payload buffer */
	rpmi_env_memcpy(payload_buffer, comm_hdr->data, payload_size);
	var = (struct efi_var_access_variable *)payload_buffer;

	/* Prevent infosize overflow */
	if ((((rpmi_uint64_t)(~0) - var->datasize) <
	     offsetof(struct efi_var_access_variable, name))
	    ||
	    (((rpmi_uint64_t)(~0) - var->namesize) <
	     offsetof(struct efi_var_access_variable, name) + var->datasize)) {
		DPRINTF("infosize overflow !!!");
		return EFI_ACCESS_DENIED;
	}

	infosize = offsetof(struct efi_var_access_variable, name) +
	    var->datasize + var->namesize;
	if (infosize > payload_size) {
		DPRINTF("Data size exceed communication buffer size limit !!!");
		return EFI_ACCESS_DENIED;
	}

	/* Ensure Variable Name is a Null-terminated string */
	if ((var->namesize < sizeof(rpmi_uint16_t)) ||
	    (var->name[var->namesize / sizeof(rpmi_uint16_t) - 1] != L'\0')) {
		DPRINTF("Variable Name NOT Null-terminated !!!");
		return EFI_ACCESS_DENIED;
	}

	if (is_context_get_variable && (var->name[0] == 0))
		return EFI_INVALID_PARAMETER;

	return EFI_SUCCESS;
}

static rpmi_uint64_t fn_get_variable(struct rpmi_mm_efi *mmefi,
				     struct efi_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	if (!mmefi->ops || !mmefi->ops->get_variable)
		return EFI_ACCESS_DENIED;

	status = validate_input(comm_hdr, payload_size, true);
	if (status != EFI_SUCCESS)
		return status;

	return mmefi->ops->get_variable(mmefi->ops_priv, comm_hdr->data,
					payload_size);
}

static rpmi_uint64_t validate_name(struct efi_var_comm_header *comm_hdr,
				   rpmi_uint32_t payload_size)
{
	struct efi_var_get_next_var_name *var;
	rpmi_uint64_t infosize, max_len;

	if (payload_size < offsetof(struct efi_var_get_next_var_name, name)) {
		DPRINTF("MM communication buffer size invalid !!!");
		return EFI_INVALID_PARAMETER;
	}

	/* Copy the input communicate buffer payload to local payload buffer */
	rpmi_env_memcpy(payload_buffer, comm_hdr->data, payload_size);
	var = (struct efi_var_get_next_var_name *)payload_buffer;

	/*
	 * Calculate the possible maximum length of name string, including the
	 * Null-terminator.
	 */
	max_len = var->namesize / sizeof(rpmi_uint16_t);
	if (!max_len ||
	    (rpmi_env_strnlen((const char *)var->name, max_len) == max_len)) {
		/*
		 * Null-terminator is not found in the first namesize bytes of
		 * the name buffer, follow spec to return EFI_INVALID_PARAMETER.
		 */
		return EFI_INVALID_PARAMETER;
	}

	/* Prevent infosize overflow */
	if (((rpmi_uint64_t)(~0) - var->namesize) <
	    offsetof(struct efi_var_get_next_var_name, name) + var->namesize) {
		DPRINTF("infosize overflow !!!");
		return EFI_ACCESS_DENIED;
	}

	infosize =
	    offsetof(struct efi_var_access_variable, name) + var->namesize;
	if (infosize > payload_size) {
		DPRINTF("Data size exceed communication buffer size limit !!!");
		return EFI_ACCESS_DENIED;
	}

	return EFI_SUCCESS;
}

static rpmi_uint64_t fn_get_next_var_name(struct rpmi_mm_efi *mmefi,
					  struct efi_var_comm_header *comm_hdr,
					  rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	if (!mmefi->ops || !mmefi->ops->get_next_variable_name)
		return EFI_ACCESS_DENIED;

	status = validate_name(comm_hdr, payload_size);
	if (status != EFI_SUCCESS)
		return status;

	return mmefi->ops->get_next_variable_name(mmefi->ops_priv,
						  comm_hdr->data, payload_size);
}

static rpmi_uint64_t fn_set_variable(struct rpmi_mm_efi *mmefi,
				     struct efi_var_comm_header *comm_hdr,
				     rpmi_uint32_t payload_size)
{
	rpmi_uint64_t status;

	if (!mmefi->ops || !mmefi->ops->set_variable)
		return EFI_ACCESS_DENIED;

	status = validate_input(comm_hdr, payload_size, false);
	if (status != EFI_SUCCESS)
		return status;

	return mmefi->ops->set_variable(mmefi->ops_priv, comm_hdr->data,
					payload_size);
}

static inline rpmi_uint64_t fn_get_payload_size(rpmi_uint8_t *comm_hdr_data,
						rpmi_uint32_t payload_size)
{
	struct efi_var_get_payload_size *get_payload_size;

	if (payload_size < sizeof(struct efi_var_get_payload_size))
		return EFI_INVALID_PARAMETER;

	get_payload_size = (struct efi_var_get_payload_size *)comm_hdr_data;
	get_payload_size->var_payload_size = MAX_PAYLOAD_SIZE;

	return EFI_SUCCESS;
}

static enum rpmi_error efi_var_function_handler(struct rpmi_mm_efi *mmefi,
						void *comm_buf,
						rpmi_uint64_t bufsize)
{
	struct efi_var_comm_header *var_comm_hdr;
	rpmi_uint64_t status, payload_size;

	if (comm_buf == NULL) {
		DPRINTF("Nothing to do.");
		return RPMI_SUCCESS;
	}

	if (bufsize < EFI_VAR_COMM_HEADER_SIZE) {
		DPRINTF("MM comm buffer size invalid !!!");
		return RPMI_SUCCESS;
	}

	payload_size = bufsize - EFI_VAR_COMM_HEADER_SIZE;
	DPRINTF("bufsize = %ld hdrsize = %ld payload_size = %ld",
		bufsize, EFI_VAR_COMM_HEADER_SIZE, payload_size);

	if (payload_size > MAX_PAYLOAD_SIZE) {
		DPRINTF("MM comm buffer payload size invalid > %ld !!!",
			MAX_PAYLOAD_SIZE);
		return RPMI_SUCCESS;
	}

	rpmi_env_memset(payload_buffer, 0x00, MAX_PAYLOAD_SIZE);
	var_comm_hdr = (struct efi_var_comm_header *)comm_buf;

	switch (var_comm_hdr->function) {
	case EFI_VAR_FN_GET_VARIABLE:
		DPRINTF("Processing %s efi_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++efi_calls_counter);
		status = fn_get_variable(mmefi, var_comm_hdr, payload_size);
		break;

	case EFI_VAR_FN_GET_NEXT_VARIABLE_NAME:
		DPRINTF("Processing %s efi_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++efi_calls_counter);
		status =
		    fn_get_next_var_name(mmefi, var_comm_hdr, payload_size);
		break;

	case EFI_VAR_FN_SET_VARIABLE:
		DPRINTF("Processing %s efi_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++efi_calls_counter);
		status = fn_set_variable(mmefi, var_comm_hdr, payload_size);
		break;

	case EFI_VAR_FN_GET_PAYLOAD_SIZE:
		DPRINTF("Processing %s efi_calls_counter %u",
			get_var_fn_string(var_comm_hdr->function),
			++efi_calls_counter);
		status = fn_get_payload_size(var_comm_hdr->data, payload_size);
		break;

	case EFI_VAR_FN_READY_TO_BOOT:
	case EFI_VAR_FN_EXIT_BOOT_SERVICE:
		DPRINTF("Processing (dummy) %s",
			get_var_fn_string(var_comm_hdr->function));
		status = EFI_SUCCESS;
		break;

	default:
		status = EFI_UNSUPPORTED;
		DPRINTF("%s not supported",
			get_var_fn_string(var_comm_hdr->function));
		break;
	}

	var_comm_hdr->return_status = status;

	return RPMI_SUCCESS;
}

static enum rpmi_error efi_var_protocol_handler(struct rpmi_shmem *mm_shmem,
						rpmi_uint16_t req_datalen,
						const rpmi_uint8_t *req_data,
						rpmi_uint16_t *rsp_datalen,
						rpmi_uint8_t *rsp_data,
						void *priv_data)
{
	enum mm_efi_header_guid guid_name = MM_EFI_VAR_PROTOCOL_GUID;
	struct mm_efi_comm_header *mm_comm_hdr, *msg;
	struct rpmi_mm_comm_req *mmc_req;
	struct rpmi_mm_efi *mmefi;
	enum rpmi_error status;
	rpmi_uint64_t msg_len;

	if (guid_name != MM_EFI_VAR_PROTOCOL_GUID)
		return RPMI_ERR_NO_DATA;

	DPRINTF("Handling header %s", get_hdr_guid_string(guid_name));

	mmc_req = (struct rpmi_mm_comm_req *)req_data;
	rpmi_shmem_read(mm_shmem, mmc_req->idata_off, msg_buffer,
			sizeof(struct mm_efi_comm_header));

	mm_comm_hdr = (struct mm_efi_comm_header *)msg_buffer;
	msg_len =
	    offsetof(struct mm_efi_comm_header, data) + mm_comm_hdr->msg_len;
	msg = (struct mm_efi_comm_header *)msg_buffer;
	rpmi_shmem_read(mm_shmem, mmc_req->idata_off, msg, msg_len);

	mmefi = (struct rpmi_mm_efi *)priv_data;
	status = efi_var_function_handler(mmefi, &msg->data, msg_len);
	rpmi_shmem_write(mm_shmem, mmc_req->odata_off, msg, msg_len);

	return status;
}

static enum rpmi_error efi_var_protocol_cleanup(struct rpmi_shmem *mm_shmem,
						rpmi_uint16_t req_datalen,
						const rpmi_uint8_t *req_data,
						rpmi_uint16_t *rsp_datalen,
						rpmi_uint8_t *rsp_data,
						void *priv_data)
{
	enum mm_efi_header_guid guid_name = MM_EFI_VAR_PROTOCOL_GUID;
	struct rpmi_mm_efi *mmefi;

	if (guid_name != MM_EFI_VAR_PROTOCOL_GUID)
		return RPMI_ERR_NO_DATA;

	DPRINTF("Handling cleanup wrt %s", get_hdr_guid_string(guid_name));

	mmefi = (struct rpmi_mm_efi *)priv_data;
	rpmi_env_free(mmefi);

	return RPMI_SUCCESS;
}

static enum rpmi_error efi_var_policy_handler(struct rpmi_shmem *mm_shmem,
					      rpmi_uint16_t req_datalen,
					      const rpmi_uint8_t *req_data,
					      rpmi_uint16_t *rsp_datalen,
					      rpmi_uint8_t *rsp_data,
					      void *priv_data)
{
	struct efi_var_policy_comm_header *policy_hdr;
	enum mm_efi_header_guid guid_name;
	struct rpmi_mm_comm_req *mmc_req;
	struct mm_efi_comm_header *msg;
	rpmi_uint64_t msg_len;

	guid_name = (enum mm_efi_header_guid)((rpmi_uintptr_t)priv_data);

	if (guid_name != MM_EFI_VAR_POLICY_GUID)
		return RPMI_ERR_NO_DATA;

	DPRINTF("Handling header %s", get_hdr_guid_string(guid_name));

	mmc_req = (struct rpmi_mm_comm_req *)req_data;
	rpmi_shmem_read(mm_shmem, mmc_req->idata_off, msg_buffer,
			sizeof(struct mm_efi_comm_header));

	msg = (struct mm_efi_comm_header *)msg_buffer;
	rpmi_shmem_read(mm_shmem, mmc_req->idata_off, msg, sizeof(*msg));

	policy_hdr = (struct efi_var_policy_comm_header *)&msg->data;
	policy_hdr->result = 0x00;

	/* Maintain msg_len as next multiple of GUID_LENGTH/ 16 bytes */
	msg_len =
	    offsetof(struct mm_efi_comm_header, data) + sizeof(*policy_hdr);
	msg_len = msg_len + GUID_LENGTH - 1;
	msg_len = msg_len / GUID_LENGTH;
	msg_len = msg_len * GUID_LENGTH;

	rpmi_shmem_write(mm_shmem, mmc_req->odata_off, msg, msg_len);

	if (rsp_datalen)
		*rsp_datalen = msg_len;

	return RPMI_SUCCESS;
}

static enum rpmi_error efi_dummy_handler(struct rpmi_shmem *mm_shmem,
					 rpmi_uint16_t req_datalen,
					 const rpmi_uint8_t *req_data,
					 rpmi_uint16_t *rsp_datalen,
					 rpmi_uint8_t *rsp_data,
					 void *priv_data)
{
	enum mm_efi_header_guid guid_name;

	guid_name = (enum mm_efi_header_guid)((rpmi_uintptr_t)priv_data);

	if ((guid_name != MM_EFI_END_OF_DXE_GUID) &&
	    (guid_name != MM_EFI_READY_TO_BOOT_GUID) &&
	    (guid_name != MM_EFI_EXIT_BOOT_SVC_GUID))
		return RPMI_ERR_NO_DATA;

	DPRINTF("Handling (dummy) header %s", get_hdr_guid_string(guid_name));

	if (rsp_datalen)
		*rsp_datalen = 0;

	return RPMI_SUCCESS;
}

enum rpmi_error rpmi_mm_efi_register_service(struct rpmi_service_group *group,
					     struct rpmi_mm_efi *ipefi)
{
	struct rpmi_mm_efi *mmefi = NULL;
	enum rpmi_error status;

	struct rpmi_mm_service efi_srvlist[] = {
		[0] = {
			.guid = MM_EFI_VAR_PROTOCOL_GUID_DATA,
			.active_cbfn_p = efi_var_protocol_handler,
			.delete_cbfn_p = efi_var_protocol_cleanup,
			/* .priv_data field to be filled later based on input */
		},
		[1] = {
			.guid = MM_EFI_VAR_POLICY_GUID_DATA,
			.active_cbfn_p = efi_var_policy_handler,
			.delete_cbfn_p = NULL,
			.priv_data =
			    (void *)((rpmi_uintptr_t)MM_EFI_VAR_POLICY_GUID),
		},
		[2] = {
			.guid = MM_EFI_END_OF_DXE_GUID_DATA,
			.active_cbfn_p = efi_dummy_handler,
			.delete_cbfn_p = NULL,
			.priv_data =
			    (void *)((rpmi_uintptr_t)MM_EFI_END_OF_DXE_GUID),
		},
		[3] = {
			.guid = MM_EFI_READY_TO_BOOT_GUID_DATA,
			.active_cbfn_p = efi_dummy_handler,
			.delete_cbfn_p = NULL,
			.priv_data =
			    (void *)((rpmi_uintptr_t)MM_EFI_READY_TO_BOOT_GUID),
		},
		[4] = {
			.guid = MM_EFI_EXIT_BOOT_SVC_GUID_DATA,
			.active_cbfn_p = efi_dummy_handler,
			.delete_cbfn_p = NULL,
			.priv_data =
			    (void *)((rpmi_uintptr_t)MM_EFI_EXIT_BOOT_SVC_GUID),
		},
	};

	/* Critical parameters should be non-NULL */
	if (!ipefi || !ipefi->ops || !ipefi->ops->get_variable ||
	    !ipefi->ops->get_next_variable_name || !ipefi->ops->set_variable) {
		DPRINTF("invalid parameter: pointers not to be NULL");
		return RPMI_ERR_INVALID_PARAM;
	}

	/* Allocate MM EFI platform operations wrt MM_EFI_VAR_PROTOCOL_GUID */
	mmefi = rpmi_env_zalloc(sizeof(*mmefi));
	if (!mmefi) {
		DPRINTF("failed to allocate EFI platform ops struct");
		return RPMI_ERR_DENIED;
	}

	rpmi_env_memcpy(mmefi, ipefi, sizeof(*mmefi));
	efi_srvlist[0].priv_data = (void *)mmefi;

	/* All data sanity done/ filled: now do the registration */
	status = rpmi_mm_service_register(group, array_size(efi_srvlist),
					  efi_srvlist);

	if (status)
		rpmi_env_free(mmefi);

	return status;
}
