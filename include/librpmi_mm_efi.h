/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2025 Ventana Micro Systems Inc.
 */

#ifndef __LIBRPMI_MM_EFI_H__
#define __LIBRPMI_MM_EFI_H__

#include <librpmi.h>

enum mm_efi_header_guid {
	MM_EFI_VAR_PROTOCOL_GUID,
	MM_EFI_VAR_POLICY_GUID,
	MM_EFI_END_OF_DXE_GUID,
	MM_EFI_READY_TO_BOOT_GUID,
	MM_EFI_EXIT_BOOT_SVC_GUID,
	MM_EFI_HDR_GUID_UNSUPPORTED,
};

#define MM_EFI_VAR_PROTOCOL_GUID_DATA	\
	{ 0xed32d533, 0x99e6, 0x4209,	\
	  { 0x9c, 0xc0, 0x2d, 0x72, 0xcd, 0xd9, 0x98, 0xa7 } }

#define MM_EFI_VAR_POLICY_GUID_DATA	\
	{ 0xda1b0d11, 0xd1a7, 0x46c4,	\
	  { 0x9d, 0xc9, 0xf3, 0x71, 0x48, 0x75, 0xc6, 0xeb } }

#define MM_EFI_END_OF_DXE_GUID_DATA	\
	{ 0x2ce967a, 0xdd7e, 0x4ffc, 	\
	  { 0x9e, 0xe7, 0x81, 0x0c, 0xf0, 0x47, 0x08, 0x80 } }

#define MM_EFI_READY_TO_BOOT_GUID_DATA	\
	{ 0x7ce88fb3, 0x4bd7, 0x4679,	\
	  { 0x87, 0xa8, 0xa8, 0xd8, 0xde, 0xe5, 0x0d, 0x2b } }

#define MM_EFI_EXIT_BOOT_SVC_GUID_DATA	\
	{ 0x27abf055, 0xb1b8, 0x4c26,	\
	  { 0x80, 0x48, 0x74, 0x8f, 0x37, 0xba, 0xa2, 0xdf } }

/** Basic EFI error defines */
#define MAX_BIT                 (0x8000000000000000ULL)

#define ENCODE_ERROR(code)      ((rpmi_uint64_t)(MAX_BIT | code))
#define RETURN_ERROR(code)      (((rpmi_int64_t)(rpmi_uint64_t)(code)) < 0)

#define EFI_SUCCESS             ((rpmi_uint64_t)0)
#define EFI_INVALID_PARAMETER   ENCODE_ERROR(2)
#define EFI_UNSUPPORTED         ENCODE_ERROR(3)
#define EFI_BUFFER_TOO_SMALL    ENCODE_ERROR(5)
#define EFI_OUT_OF_RESOURCES    ENCODE_ERROR(9)
#define EFI_NOT_FOUND           ENCODE_ERROR(14)
#define EFI_ACCESS_DENIED       ENCODE_ERROR(15)

#define EFI_ERROR(n)            RETURN_ERROR(n)

/**
 * struct mm_efi_comm_header - Header used for MM EFI communication
 *
 * @hdr_guid: guid used for disambiguation of message format
 * @msg_len:  size of data (in bytes) - excluding size of the header
 * @data:     an array of bytes that is msg_len in size
 *
 * To avoid confusion in interpreting frames, the communication buffer should
 * always begin with struct mm_efi_comm_header.
 */
struct mm_efi_comm_header {
	struct rpmi_guid_t hdr_guid;
	rpmi_uint64_t msg_len;
	rpmi_uint8_t data[];
};

struct efi_var_policy_comm_header {
	rpmi_uint32_t signature;
	rpmi_uint32_t revision;
	rpmi_uint32_t command;
	rpmi_uint64_t result;
};

/** The payload for this function is struct mm_var_comm_access_variable */
#define EFI_VAR_FN_GET_VARIABLE                         1

/** The payload for this function is struct mm_var_comm_get_next_var_name */
#define EFI_VAR_FN_GET_NEXT_VARIABLE_NAME               2

/** The payload for this function is struct mm_var_comm_access_variable */
#define EFI_VAR_FN_SET_VARIABLE                         3

#define EFI_VAR_FN_QUERY_VARIABLE_INFO                  4
#define EFI_VAR_FN_READY_TO_BOOT                        5
#define EFI_VAR_FN_EXIT_BOOT_SERVICE                    6
#define EFI_VAR_FN_GET_STATISTICS                       7
#define EFI_VAR_FN_LOCK_VARIABLE                        8
#define EFI_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_SET      9
#define EFI_VAR_FN_VAR_CHECK_VARIABLE_PROPERTY_GET      10

/** The payload for this function is struct efi_var_comm_get_payload_size */
#define EFI_VAR_FN_GET_PAYLOAD_SIZE                     11

#define EFI_VAR_FN_INIT_RUNTIME_VARIABLE_CACHE_CONTEXT  12
#define EFI_VAR_FN_SYNC_RUNTIME_CACHE                   13
#define EFI_VAR_FN_GET_RUNTIME_CACHE_INFO               14

/** Size of MM EFI communicate header, without including the payload */
#define MM_EFI_COMM_HEADER_SIZE   (sizeof(struct mm_efi_comm_header) - 1)

/** Size of EFI variable communicate header, without including the payload */
#define EFI_VAR_COMM_HEADER_SIZE  (sizeof(struct efi_var_comm_header) - 1)

/* Max information size per MM variable: 1 KB (including header) */
#define MAX_VARINFO_SIZE  1024
#define MAX_PAYLOAD_SIZE  (MAX_VARINFO_SIZE - EFI_VAR_COMM_HEADER_SIZE)

/**
 * This structure is used for MM variable. The communication buffer should be:
 *      struct mm_efi_comm_header + struct efi_var_comm_header + payload
 */
struct efi_var_comm_header {
	rpmi_uint64_t function;
	rpmi_uint64_t return_status;
	rpmi_uint8_t data[];
};

struct efi_var_get_payload_size {
	rpmi_uint64_t var_payload_size;
};

/** Structure used to communicate with MM EFI via SetVariable/ GetVariable */
struct efi_var_access_variable {
	struct rpmi_guid_t guid;
	rpmi_uint64_t datasize;
	rpmi_uint64_t namesize;
	rpmi_uint32_t attr;
	rpmi_uint16_t name[];
};

/** Structure used to communicate with MM EFI via GetNextVariableName */
struct efi_var_get_next_var_name {
	struct rpmi_guid_t guid;
	rpmi_uint64_t namesize;
	rpmi_uint16_t name[];
};

/** MM EFI specific platform operations */
struct rpmi_mm_efi_platform_ops {
	rpmi_uint64_t (*get_variable)(void *priv, const rpmi_uint8_t *data,
				      rpmi_uint32_t datasize);

	rpmi_uint64_t (*get_next_variable_name)(void *priv,
						const rpmi_uint8_t *data,
						rpmi_uint32_t datasize);

	rpmi_uint64_t (*set_variable)(void *priv, const rpmi_uint8_t *data,
				      rpmi_uint32_t datasize);
};

/** Details required for MM EFI platform operations */
struct rpmi_mm_efi {
	/** MM EFI platform operations */
	const struct rpmi_mm_efi_platform_ops *ops;

	/** Private data of MM EFI platform operations */
	void *ops_priv;
};

/**
 * Function to register MM EFI service units and the platform operations that
 * are to be attached with the MM EFI helper
 */
enum rpmi_error rpmi_mm_efi_register_service(struct rpmi_service_group *group,
					     struct rpmi_mm_efi *efi);

#endif /* __LIBRPMI_MM_EFI_H__ */
