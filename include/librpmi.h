/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * RISC-V platform management (RPMI) library for the platform firmware.
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#ifndef __LIBRPMI_H__
#define __LIBRPMI_H__

#include <librpmi_env.h>

/******************************************************************************/

/**
 * \defgroup RPMI_MSG_PROTOCOL RPMI Messaging Protocol Structures and Defines.
 * @brief Structures and defines for the RPMI messaging protocol.
 * @{
 */

/** RPMI specification version */
#define RPMI_SPEC_VERSION_MAJOR			1
#define RPMI_SPEC_VERSION_MINOR			0

/*
 * 31                                            0
 * +---------------------+-----------------------+
 * | FLAGS | SERVICE_ID  |   SERVICEGROUP_ID     |
 * +---------------------+-----------------------+
 * |        TOKEN        |     DATA LENGTH       |
 * +---------------------+-----------------------+
 * |                 DATA/PAYLOAD                |
 * +---------------------------------------------+
 */

/** Message Header byte offset */
#define RPMI_MSG_HDR_OFFSET			(0x0)
/** Message Header Size in bytes */
#define RPMI_MSG_HDR_SIZE			(8)

/** ServiceGroup ID field byte offset */
#define RPMI_MSG_SERVICEGROUP_ID_OFFSET		(0x0)
/** ServiceGroup ID field size in bytes */
#define RPMI_MSG_SERVICEGROUP_ID_SIZE		(2)

/** Service ID field byte offset */
#define RPMI_MSG_SERVICE_ID_OFFSET		(0x2)
/** Service ID field size in bytes */
#define RPMI_MSG_SERVICE_ID_SIZE		(1)

/** Flags field byte offset */
#define RPMI_MSG_FLAGS_OFFSET			(0x3)
/** Flags field size in bytes */
#define RPMI_MSG_FLAGS_SIZE			(1)

#define RPMI_MSG_FLAGS_TYPE_POS			(0U)
#define RPMI_MSG_FLAGS_TYPE_MASK		0x7
#define RPMI_MSG_FLAGS_TYPE			\
	((0x7) << RPMI_MSG_FLAGS_TYPE_POS)

#define RPMI_MSG_FLAGS_DOORBELL_POS		(3U)
#define RPMI_MSG_FLAGS_DOORBELL_MASK		0x1
#define RPMI_MSG_FLAGS_DOORBELL			\
	((0x1) << RPMI_MSG_FLAGS_DOORBELL_POS)

/** Data length field byte offset */
#define RPMI_MSG_DATALEN_OFFSET			(0x4)
/** Data length field size in bytes */
#define RPMI_MSG_DATALEN_SIZE			(2)

/** Token field byte offset */
#define RPMI_MSG_TOKEN_OFFSET			(0x6)
/** Token field size in bytes */
#define RPMI_MSG_TOKEN_SIZE			(2)

/** Data field byte offset */
#define RPMI_MSG_DATA_OFFSET			(RPMI_MSG_HDR_SIZE)
/** Data field size in bytes */
#define RPMI_MSG_DATA_SIZE(__slot_size)		((__slot_size) - RPMI_MSG_HDR_SIZE)

/** Minimum slot size in bytes */
#define RPMI_SLOT_SIZE_MIN			(64)

/** RPMI Messages Types */
enum rpmi_message_type {
	/* Normal request backed with ack */
	RPMI_MSG_NORMAL_REQUEST		= 0x0,
	/* Request without any ack */
	RPMI_MSG_POSTED_REQUEST		= 0x1,
	/* Acknowledgement for normal request message */
	RPMI_MSG_ACKNOWLEDGEMENT	= 0x2,
	/* Notification message */
	RPMI_MSG_NOTIFICATION		= 0x3,
};

/** RPMI Message Header */
struct rpmi_message_header {
	rpmi_uint16_t	servicegroup_id;
	rpmi_uint8_t	service_id;
	rpmi_uint8_t	flags;
	rpmi_uint16_t	datalen;
	rpmi_uint16_t	token;
};

/** RPMI Message */
struct rpmi_message {
	struct rpmi_message_header	header;
	rpmi_uint8_t			data[];
};

/** RPMI Error Types */
enum rpmi_error {
	/* Success */
	RPMI_SUCCESS			= 0,
	/* General failure  */
	RPMI_ERR_FAILED			= -1,
	/* Service or feature not supported */
	RPMI_ERR_NOTSUPP		= -2,
	/* Invalid Parameter  */
	RPMI_ERR_INVALID_PARAM		= -3,
	/*
	 * Denied to insufficient permissions
	 * or due to unmet prerequisite
	 */
	RPMI_ERR_DENIED			= -4,
	/* Invalid address or offset */
	RPMI_ERR_INVALID_ADDR		= -5,
	/*
	 * Operation failed as it was already in
	 * progress or the state has changed already
	 * for which the operation was carried out.
	 */
	RPMI_ERR_ALREADY		= -6,
	/*
	 * Error in implementation which violates
	 * the specification version
	 */
	RPMI_ERR_EXTENSION		= -7,
	/* Operation failed due to hardware issues */
	RPMI_ERR_HW_FAULT		= -8,
	/* System, device or resource is busy */
	RPMI_ERR_BUSY			= -9,
	/* System or device or resource in invalid state */
	RPMI_ERR_INVALID_STATE		= -10,
	/* Index, offset or address is out of range */
	RPMI_ERR_BAD_RANGE		= -11,
	/* Operation timed out */
	RPMI_ERR_TIMEOUT		= -12,
	/*
	 * Error in input or output or
	 * error in sending or receiving data
	 * through communication medium
	 */
	RPMI_ERR_IO			= -13,
	/* No data available */
	RPMI_ERR_NO_DATA		= -14,
	RPMI_ERR_RESERVED_START		= -15,
	RPMI_ERR_RESERVED_END		= -127,
	RPMI_ERR_VENDOR_START		= -128
};

/** RPMI Queue Types */
enum rpmi_queue_type {
	RPMI_QUEUE_A2P_REQ = 0,
	RPMI_QUEUE_P2A_ACK,
	RPMI_QUEUE_P2A_REQ,
	RPMI_QUEUE_A2P_ACK,
	RPMI_QUEUE_MAX
};

/**
 * RISC-V privilege levels associated with
 * RPMI context and service groups
 */
enum rpmi_privilege_level {
	RPMI_PRIVILEGE_S_MODE = 0,
	RPMI_PRIVILEGE_M_MODE = 1,
	RPMI_PRIVILEGE_LEVEL_MAX
};

#define RPMI_PRIVILEGE_S_MODE_MASK	(1U << RPMI_PRIVILEGE_S_MODE)
#define RPMI_PRIVILEGE_M_MODE_MASK	(1U << RPMI_PRIVILEGE_M_MODE)

/** RPMI ServiceGroups IDs */
enum rpmi_servicegroup_id {
	RPMI_SRVGRP_ID_MIN		= 0,
	RPMI_SRVGRP_BASE		= 0x0001,
	RPMI_SRVGRP_SYSTEM_MSI		= 0x0002,
	RPMI_SRVGRP_SYSTEM_RESET	= 0x0003,
	RPMI_SRVGRP_SYSTEM_SUSPEND	= 0x0004,
	RPMI_SRVGRP_HSM			= 0x0005,
	RPMI_SRVGRP_CPPC		= 0x0006,
	RPMI_SRVGRP_VOLTAGE		= 0x0007,
	RPMI_SRVGRP_CLOCK		= 0x0008,
	RPMI_SRVGRP_DEVICE_POWER	= 0x0009,
	RPMI_SRVGRP_PERFORMANCE		= 0x000A,
	RPMI_SRVGRP_MANAGEMENT_MODE	= 0x000B,
	RPMI_SRVGRP_RAS_AGENT		= 0x000C,
	RPMI_SRVGRP_REQUEST_FORWARD	= 0x000D,
	RPMI_SRVGRP_ID_MAX_COUNT,

	/* Reserved range for service groups */
	RPMI_SRVGRP_RESERVE_START	= RPMI_SRVGRP_ID_MAX_COUNT,
	RPMI_SRVGRP_RESERVE_END		= 0x7BFF,

	/* Experimental service groups range */
	RPMI_SRVGRP_EXPERIMENTAL_START	= 0x7C00,
	RPMI_SRVGRP_EXPERIMENTAL_END	= 0x7FFF,

	/* Vendor/Implementation-specific service groups range */
	RPMI_SRVGRP_VENDOR_START	= 0x8000,
	RPMI_SRVGRP_VENDOR_END		= 0xFFFF,
};

/** RPMI Base ServiceGroup Service IDs */
enum rpmi_base_service_id {
	RPMI_BASE_SRV_ENABLE_NOTIFICATION		= 0x01,
	RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION	= 0x02,
	RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN		= 0x03,
	RPMI_BASE_SRV_GET_SPEC_VERSION			= 0x04,
	RPMI_BASE_SRV_GET_PLATFORM_INFO			= 0x05,
	RPMI_BASE_SRV_PROBE_SERVICE_GROUP		= 0x06,
	RPMI_BASE_SRV_GET_ATTRIBUTES			= 0x07,
	RPMI_BASE_SRV_ID_MAX
};

#define RPMI_BASE_VERSION_MINOR_POS		0
#define RPMI_BASE_VERSION_MINOR_MASK		0xffff

#define RPMI_BASE_VERSION_MAJOR_POS		16
#define RPMI_BASE_VERSION_MAJOR_MASK		0xffff

#define RPMI_BASE_VERSION(__major, __minor)	\
((((__major) & RPMI_BASE_VERSION_MAJOR_MASK) << RPMI_BASE_VERSION_MAJOR_POS) | \
	(((__minor) & RPMI_BASE_VERSION_MINOR_MASK) << RPMI_BASE_VERSION_MINOR_POS))

#define RPMI_BASE_FLAGS_F0_PRIVILEGE		(1U << 1)
#define RPMI_BASE_FLAGS_F0_EV_NOTIFY		(1U << 0)

/** RPMI System MSI (SYSMSI) ServiceGroup Service IDs */
enum rpmi_sysmsi_service_id {
	RPMI_SYSMSI_SRV_ENABLE_NOTIFICATION	= 0x01,
	RPMI_SYSMSI_SRV_GET_ATTRIBUTES		= 0x2,
	RPMI_SYSMSI_SRV_GET_MSI_ATTRIBUTES	= 0x3,
	RPMI_SYSMSI_SRV_SET_MSI_STATE		= 0x4,
	RPMI_SYSMSI_SRV_GET_MSI_STATE		= 0x5,
	RPMI_SYSMSI_SRV_SET_MSI_TARGET		= 0x6,
	RPMI_SYSMSI_SRV_GET_MSI_TARGET		= 0x7,
	RPMI_SYSMSI_SRV_ID_MAX
};

/** RPMI System Reset ServiceGroup Service IDs */
enum rpmi_sysreset_service_id {
	RPMI_SYSRST_SRV_ENABLE_NOTIFICATION	= 0x01,
	RPMI_SYSRST_SRV_GET_ATTRIBUTES		= 0x02,
	RPMI_SYSRST_SRV_SYSTEM_RESET		= 0x03,
	RPMI_SYSRST_SRV_ID_MAX			= 0x04,
};

/** RPMI System Reset types */
enum rpmi_sysrst_reset_type {
	RPMI_SYSRST_TYPE_SHUTDOWN	= 0x0,
	RPMI_SYSRST_TYPE_COLD_REBOOT	= 0x1,
	RPMI_SYSRST_TYPE_WARM_REBOOT	= 0x2,
	RPMI_SYSRST_TYPE_MAX
};

#define RPMI_SYSRST_ATTRS_FLAGS_RESETTYPE	1U

/** RPMI System Suspend ServiceGroup Service IDs */
enum rpmi_system_suspend_service_id {
	RPMI_SYSSUSP_SRV_ENABLE_NOTIFICATION	= 0x01,
	RPMI_SYSSUSP_SRV_GET_ATTRIBUTES		= 0x02,
	RPMI_SYSSUSP_SRV_SYSTEM_SUSPEND		= 0x03,
	RPMI_SYSSUSP_SRV_ID_MAX			= 0x04,
};

/* RPMI Suspend Types */
enum rpmi_syssusp_suspend_type {
	RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM	= 0x0,
	RPMI_SYSSUSP_TYPE_MAX
};

#define RPMI_SYSSUSP_ATTRS_FLAGS_RESUMEADDR	(1U << 1)
#define RPMI_SYSSUSP_ATTRS_FLAGS_SUSPENDTYPE	1U

/** RPMI Hart State Management (HSM) ServiceGroup Service IDs */
enum rpmi_hsm_service_id {
	RPMI_HSM_SRV_ENABLE_NOTIFICATION	= 0x01,
	RPMI_HSM_SRV_GET_HART_STATUS		= 0x02,
	RPMI_HSM_SRV_GET_HART_LIST		= 0x03,
	RPMI_HSM_SRV_GET_SUSPEND_TYPES		= 0x04,
	RPMI_HSM_SRV_GET_SUSPEND_INFO		= 0x05,
	RPMI_HSM_SRV_HART_START			= 0x06,
	RPMI_HSM_SRV_HART_STOP			= 0x07,
	RPMI_HSM_SRV_HART_SUSPEND		= 0x08,
	RPMI_HSM_SRV_ID_MAX
};

/** RPMI Clock (CLK) ServiceGroup Service IDs */
enum rpmi_clock_service_id {
	RPMI_CLK_SRV_ENABLE_NOTIFICATION	= 0x01,
	RPMI_CLK_SRV_GET_NUM_CLOCKS		= 0x02,
	RPMI_CLK_SRV_GET_ATTRIBUTES		= 0x03,
	RPMI_CLK_SRV_GET_SUPPORTED_RATES	= 0x04,
	RPMI_CLK_SRV_SET_CONFIG			= 0x05,
	RPMI_CLK_SRV_GET_CONFIG			= 0x06,
	RPMI_CLK_SRV_SET_RATE			= 0x07,
	RPMI_CLK_SRV_GET_RATE			= 0x08,
	RPMI_CLK_SRV_ID_MAX
};

/** RPMI CPPC (CPPC) ServiceGroup Service IDs */
enum rpmi_cppc_service_id {
	RPMI_CPPC_SRV_ENABLE_NOTIFICATION	= 0x01,
	RPMI_CPPC_SRV_PROBE_REG			= 0x02,
	RPMI_CPPC_SRV_READ_REG			= 0x03,
	RPMI_CPPC_SRV_WRITE_REG			= 0x04,
	RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION	= 0x05,
	RPMI_CPPC_SRV_GET_FAST_CHANNEL_OFFSET	= 0x06,
	RPMI_CPPC_SRV_GET_HART_LIST		= 0x07,
	RPMI_CPPC_SRV_ID_MAX
};

/** RPMI Device_Power (DPWR) ServiceGroup Service IDs */
enum rpmi_dpwr_service_id {
	RPMI_DPWR_SRV_ENABLE_NOTIFICATION	= 0x01,
	RPMI_DPWR_SRV_GET_NUM_DOMAINS		= 0x02,
	RPMI_DPWR_SRV_GET_ATTRIBUTES		= 0x03,
	RPMI_DPWR_SRV_SET_DPWR_STATE		= 0x04,
	RPMI_DPWR_SRV_GET_DPWR_STATE		= 0x05,
	RPMI_DPWR_SRV_ID_MAX,
};

/** RPMI Performance (PERF) ServiceGroup Service IDs */
enum rpmi_perf_service_id {
	RPMI_PERF_SRV_ENABLE_NOTIFICATION		= 0x01,
	RPMI_PERF_SRV_GET_NUM_DOMAINS			= 0x02,
	RPMI_PERF_SRV_GET_ATTRIBUTES			= 0x03,
	RPMI_PERF_SRV_GET_SUPPORTED_LEVELS		= 0x04,
	RPMI_PERF_SRV_GET_PERF_LEVEL			= 0x05,
	RPMI_PERF_SRV_SET_PERF_LEVEL			= 0x06,
	RPMI_PERF_SRV_GET_PERF_LIMIT			= 0x07,
	RPMI_PERF_SRV_SET_PERF_LIMIT			= 0x08,
	RPMI_PERF_SRV_GET_FAST_CHANNEL_REGION		= 0x09,
	RPMI_PERF_SRV_GET_FAST_CHANNEL_ATTRIBUTES	= 0x0A,
	RPMI_PERF_SRV_ID_MAX,
};

/** RPMI Voltage (Volt) ServiceGroup Service IDs */
enum rpmi_volt_service_id {
	RPMI_VOLT_SRV_ENABLE_NOTIFICATION	= 0x01,
	RPMI_VOLT_SRV_GET_NUM_DOMAINS		= 0x02,
	RPMI_VOLT_SRV_GET_ATTRIBUTES		= 0x03,
	RPMI_VOLT_SRV_GET_SUPPORTED_LEVELS	= 0x04,
	RPMI_VOLT_SRV_SET_CONFIG		= 0x05,
	RPMI_VOLT_SRV_GET_CONFIG		= 0x06,
	RPMI_VOLT_SRV_SET_VOLT_LEVEL		= 0x07,
	RPMI_VOLT_SRV_GET_VOLT_LEVEL		= 0x08,
	RPMI_VOLT_SRV_ID_MAX,
};

/** @} */

/****************************************************************************/

/**
 * \defgroup LIBRPMI_COMMON_INTERFACE RPMI Common Library Interface
 * @brief Common defines and structures for the RPMI library.
 * @{
 */

#define LIBRPMI_IMPL_ID					0

#define LIBRPMI_IMPL_VERSION_MAJOR			0
#define LIBRPMI_IMPL_VERSION_MINOR			1

#define LIBRPMI_TRANSPORT_SHMEM_QUEUE_MIN_SLOTS		4
#define LIBRPMI_TRANSPORT_SHMEM_QUEUE_MIN_SIZE(__slot_size)	\
	((__slot_size) * LIBRPMI_TRANSPORT_SHMEM_QUEUE_MIN_SLOTS)

/** RPMI shared memory structure to access a platform shared memory */
struct rpmi_shmem;

/** Platform specific shared memory operations */
struct rpmi_shmem_platform_ops {
	/** Read from a physical address range (mandatory) */
	enum rpmi_error (*read)(void *priv, rpmi_uint64_t addr,
				void *in, rpmi_uint32_t len);

	/** Write to a physical address range (mandatory) */
	enum rpmi_error (*write)(void *priv, rpmi_uint64_t addr,
				 const void *out, rpmi_uint32_t len);

	/** Fill a physical address range (mandatory) */
	enum rpmi_error (*fill)(void *priv, rpmi_uint64_t addr,
				char ch, rpmi_uint32_t len);
};

/**
 * Simple cache-coherent shared memory operations which use environment functions.
 *
 * Note: These operations don't require any platform specific data.
 */
extern struct rpmi_shmem_platform_ops rpmi_shmem_simple_ops;

/**
 * Simple cache-non-coherent shared memory operations which use environment functions.
 *
 * Note: These operations don't require any platform specific data.
 */
extern struct rpmi_shmem_platform_ops rpmi_shmem_simple_noncoherent_ops;

/**
 * @brief Get base address of a shared memory
 *
 * @param[in] shmem		pointer to shared memory instance
 * @return base address of shared memory
 */
rpmi_uint64_t rpmi_shmem_base(struct rpmi_shmem *shmem);

/**
 * @brief Get size of a shared memory
 *
 * @param[in] shmem		pointer to shared memory instance
 * @return size of shared memory in bytes
 */
rpmi_uint32_t rpmi_shmem_size(struct rpmi_shmem *shmem);

/**
 * @brief Read a buffer from shared memory
 *
 * @param[in] shmem		pointer to shared memory instance
 * @param[in] offset		source offset within shared memory
 * @param[in] in		destination buffer pointer
 * @param[in] len		number of bytes to read
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_shmem_read(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				void *in, rpmi_uint32_t len);

/**
 * @brief Write a buffer to shared memory
 *
 * @param[in] shmem		pointer to shared memory instance
 * @param[in] offset		destination offset within shared memory
 * @param[in] out		source buffer pointer
 * @param[in] len		number of bytes to write
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_shmem_write(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				 const void *out, rpmi_uint32_t len);

/**
 * @brief Fill a part of shared memory with fixed character
 *
 * @param[in] shmem		pointer to shared memory instance
 * @param[in] offset		offset within shared memory
 * @param[in] ch		character value to fill
 * @param[in] len		number of characters to fill
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_shmem_fill(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				char ch, rpmi_uint32_t len);

/**
 * @brief Create a shared memory instance
 *
 * @param[in] name		name of the shared memory instance
 * @param[in] base		base address of the shared memory
 * @param[in] size		size of the shared memory
 * @param[in] ops		pointer to platform specific shared memory operations
 * @param[in] ops_priv		pointer to private data of platform operations
 * @return pointer to RPMI shared memory instance upon success and NULL upon
 * failure
 */
struct rpmi_shmem *rpmi_shmem_create(const char *name,
				     rpmi_uint64_t base,
				     rpmi_uint32_t size,
				     const struct rpmi_shmem_platform_ops *ops,
				     void *ops_priv);

/**
 * @brief Destroy (or free) a shared memory instance
 *
 * @param[in] shmem		pointer to shared memory instance
 */
void rpmi_shmem_destroy(struct rpmi_shmem *shmem);

/** @} */

/*****************************************************************************/

/**
 * \defgroup LIBRPMI_TRANSPORT_INTERFACE RPMI Transport Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for managing RPMI transport.
 * @{
 */

/**
 * RPMI transport instance
 *
 * @brief Structure representing a RPMI transport between the platform firmware
 * and application processors.
 */
struct rpmi_transport {
	/** Name of the transport */
	const char	*name;

	/** Endianness of the messages transferred through this transport */
	rpmi_bool_t	is_be;

	/**
	 * Is P2A channel available (in case of shmem based transport
	 * is p2a req and a2p ack queues)
	 */
	rpmi_bool_t	is_p2a_channel;

	/** Slot (or max message) size in transport queues */
	rpmi_size_t	slot_size;

	/**
	 * Callback to check if a RPMI queue type is empty
	 *
	 * Note: This function must be called with transport lock held.
	 */
	rpmi_bool_t	(*is_empty)(struct rpmi_transport *trans,
				    enum rpmi_queue_type qtype);

	/**
	 * Callback to check if a RPMI queue type is full
	 *
	 * Note: This function must be called with transport lock held.
	 */
	rpmi_bool_t	(*is_full)(struct rpmi_transport *trans,
				   enum rpmi_queue_type qtype);

	/**
	 * Callback to enqueue a RPMI message to a specified RPMI queue type
	 *
	 * Note: This function must be called with transport lock held.
	 */
	enum rpmi_error	(*enqueue)(struct rpmi_transport *trans,
				   enum rpmi_queue_type qtype,
				   const struct rpmi_message *msg);

	/**
	 * Callback to dequeue a RPMI message from a specified RPMI queue type
	 *
	 * Note: This function must be called with transport lock held.
	 */
	enum rpmi_error (*dequeue)(struct rpmi_transport *trans,
				   enum rpmi_queue_type qtype,
				   struct rpmi_message *out_msg);

	/** Lock to synchronize transport access (optional) */
	void		*lock;

	/** Private data of the transport implementation */
	void		*priv;
};

/**
 * @brief Check if a specified RPMI queue type of a RPMI transport is empty
 *
 * @param[in] trans		pointer to RPMI transport instance
 * @param[in] qtype		type of the RPMI queue
 * @return true if empty and false if not empty
 */
rpmi_bool_t rpmi_transport_is_empty(struct rpmi_transport *trans,
				    enum rpmi_queue_type qtype);

/**
 * @brief Check if a specified RPMI queue type of a RPMI transport is full
 *
 * @param[in] trans		pointer to RPMI transport instance
 * @param[in] qtype		type of the RPMI queue
 * @return true if full and false if not full
 */
rpmi_bool_t rpmi_transport_is_full(struct rpmi_transport *trans,
				   enum rpmi_queue_type qtype);

/**
 * @brief Enqueue a RPMI message to a specified RPMI queue type of a RPMI transport
 *
 * @param[in] trans		pointer to RPMI transport instance
 * @param[in] qtype		type of the RPMI queue
 * @param[in] msg		pointer to the enqueued RPMI message
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_transport_enqueue(struct rpmi_transport *trans,
				       enum rpmi_queue_type qtype,
				       struct rpmi_message *msg);

/**
 * @brief Dequeue a RPMI message from a specified RPMI queue type of a RPMI transport
 *
 * @param[in] trans		pointer to RPMI transport instance
 * @param[in] qtype		type of the RPMI queue
 * @param[out] out_msg		pointer to the dequeued RPMI message
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_transport_dequeue(struct rpmi_transport *trans,
				       enum rpmi_queue_type qtype,
				       struct rpmi_message *out_msg);

/**
 * @brief Create a shared memory transport instance
 *
 * @param[in] name		name of the shared memory transport instance
 * @param[in] slot_size		size of message slot for queues in shared memory
 * @param[in] a2p_req_queue_size	size of A2P request and P2A acknowledgement queues
 * @param[in] p2a_req_queue_size	size of P2A request and A2P acknowledgement queues
 * @param[in] shmem		pointer to a RPMI shared memory instance
 * @return pointer to RPMI transport upon success and NULL upon failure
 */
struct rpmi_transport *rpmi_transport_shmem_create(const char *name,
						   rpmi_uint32_t slot_size,
						   rpmi_uint32_t a2p_req_queue_size,
						   rpmi_uint32_t p2a_req_queue_size,
						   struct rpmi_shmem *shmem);

/**
 * @brief Destroy (or free) a shared memory transport instance
 *
 * @param[in] trans		pointer to RPMI transport instance
 */
void rpmi_transport_shmem_destroy(struct rpmi_transport *trans);

/** @} */

/*****************************************************************************/

/**
 * \defgroup LIBRPMI_CONTEXT_INTERFACE RPMI Context Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for managing RPMI context.
 * @{
 */

/**
 * @brief Opaque RPMI execution context which groups together a RPMI transport
 * instance and a set of RPMI service groups. The RPMI base service group is
 * a built-in service group of the RPMI context and is always available.
 */
struct rpmi_context;

/**
 * @brief Process requests from application processors for a RPMI context
 *
 * @param[in] cntx		pointer to the RPMI context
 */
void rpmi_context_process_a2p_request(struct rpmi_context *cntx);

/**
 * @brief Process events of RPMI service group in a RPMI context
 *
 * @param[in] cntx		pointer to the RPMI context
 * @param[in] servicegroup_id	ID of the service group
 */
void rpmi_context_process_group_events(struct rpmi_context *cntx,
				       rpmi_uint16_t servicegroup_id);

/**
 * @brief Process events of all RPMI service groups in a RPMI context
 *
 * @param[in] cntx		pointer to the RPMI context
 */
void rpmi_context_process_all_events(struct rpmi_context *cntx);

/**
 * @brief Find a RPMI service group in a RPMI context
 *
 * @param[in] cntx		pointer to the RPMI context
 * @param[in] servicegroup_id	ID of the service group
 * @return pointer to RPMI service group upon success and NULL upon failure
 */
struct rpmi_service_group *rpmi_context_find_group(struct rpmi_context *cntx,
						   rpmi_uint16_t servicegroup_id);

/**
 * @brief Add a RPMI service group to a RPMI context
 *
 * @param[in] cntx		pointer to the RPMI context
 * @param[in] group		pointer to the RPMI service group
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_context_add_group(struct rpmi_context *cntx,
				       struct rpmi_service_group *group);

/**
 * @brief Remove a RPMI service group from a RPMI context
 *
 * @param[in] cntx		pointer to the RPMI context
 * @param[in] group		pointer to the RPMI service group
 */
void rpmi_context_remove_group(struct rpmi_context *cntx,
			       struct rpmi_service_group *group);

/**
 * @brief Create a RPMI context
 *
 * @param[in] name		name of the context instance
 * @param[in] trans		pointer to RPMI transport instance
 * @param[in] max_num_groups	maximum number of service groups
 * @param[in] privilege_level	RISC-V privilege level of the RPMI context
 * @param[in] plat_info_len	length of the Platform info string
 * @param[in] plat_info		pointer to the Platform info string
 * @return pointer to RPMI context upon success and NULL upon failure
 */
struct rpmi_context *rpmi_context_create(const char *name,
					 struct rpmi_transport *trans,
					 rpmi_uint32_t max_num_groups,
					 enum rpmi_privilege_level privilege_level,
					 rpmi_uint32_t plat_info_len,
					 const char *plat_info);

/**
 * @brief Destroy (or free) a RPMI context
 *
 * @param[in] cntx		pointer to RPMI context instance
 */
void rpmi_context_destroy(struct rpmi_context *cntx);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_SERVICEGROUP_INTERFACE RPMI Service Group Library Interface
 * @brief Data structures implemented by the RPMI library for managing RPMI
 * service groups.
 * @{
 */

struct rpmi_service_group;

/** RPMI service instance */
struct rpmi_service {
	/** ID of the service */
	rpmi_uint8_t	service_id;

	/** Minimum data length for handling request */
	rpmi_uint16_t	min_a2p_request_datalen;

	/**
	 * Callback to process a2p request
	 *
	 * Note: This function must be called with service group lock held.
	 */
	enum rpmi_error	(*process_a2p_request)(struct rpmi_service_group *group,
					       struct rpmi_service *service,
					       struct rpmi_transport *trans,
					       rpmi_uint16_t request_data_len,
					       const rpmi_uint8_t *request_data,
					       rpmi_uint16_t *response_data_len,
					       rpmi_uint8_t *response_data);
};

/** RPMI service group instance */
struct rpmi_service_group {
	/** Name of the service group */
	const char		*name;

	/** ID of the service group */
	rpmi_uint16_t		servicegroup_id;

	/** Maximum service ID of the service group */
	rpmi_uint8_t		max_service_id;

	/** Service group version */
	rpmi_uint32_t		servicegroup_version;

	/**
	 * RISC-V privilege level bitmap where this group
	 * is allowed to be accessible. enum rpmi_privilege_level
	 * values represents the bit positions which if are
	 * set, the access to that privilege level is enabled
	 */
	rpmi_uint32_t		privilege_level_bitmap;

	/** Array of services indexed by service ID */
	struct rpmi_service	*services;

	/**
	 * Callback to process events for a service group. These events can be:
	 *
	 * 1) Fast-channel requests from application processors
	 * 2) Pending HW interrupts relevant to a service group
	 * 3) HW state changes relevant to a service group
	 *
	 * Note: This function must be called with service group lock held.
	 */
	enum rpmi_error		(*process_events)(struct rpmi_service_group *group);

	/** Lock to synchronize service group access (optional) */
	void			*lock;

	/** Private data of the service group implementation */
	void			*priv;
};

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_HSM_INTERFACE RPMI Hart State Management Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI hart state management. This is shared by multiple RPMI service
 * groups.
 * @{
 */

/** Hart ID considered invalid by RPMI library */
#define LIBRPMI_HSM_INVALID_HART_ID	(-1U)

/** Hart index considered invalid by RPMI library */
#define LIBRPMI_HSM_INVALID_HART_INDEX	(-1U)

/** RPMI HSM hart states (based on SBI specification) */
enum rpmi_hsm_hart_state {
	RPMI_HSM_HART_STATE_STARTED		= 0x0,
	RPMI_HSM_HART_STATE_STOPPED		= 0x1,
	RPMI_HSM_HART_STATE_START_PENDING	= 0x2,
	RPMI_HSM_HART_STATE_STOP_PENDING	= 0x3,
	RPMI_HSM_HART_STATE_SUSPENDED		= 0x4,
	RPMI_HSM_HART_STATE_SUSPEND_PENDING	= 0x5,
	RPMI_HSM_HART_STATE_RESUME_PENDING	= 0x6,
	RPMI_HSM_HART_STATE_MAX
};

/** RPMI HW hart states */
enum rpmi_hart_hw_state {
	/** Hart is stopped or inactive (i.e. not executing instructions) */
	RPMI_HART_HW_STATE_STOPPED	= 0x0,
	/** Hart is started or active (i.e. executing instructions) */
	RPMI_HART_HW_STATE_STARTED	= 0x1,
	/** Hart is suspended or idle (i.e. WFI or equivalent state) */
	RPMI_HART_HW_STATE_SUSPENDED	= 0x2,
	RPMI_HART_HW_STATE_MAX
};

#define RPMI_HSM_SUSPEND_INFO_FLAGS_TIMER_STOP		1U

/** RPMI HSM suspend type */
struct rpmi_hsm_suspend_type {
	rpmi_uint32_t		type;
	struct {
		rpmi_uint32_t	flags;
		rpmi_uint32_t	entry_latency_us;
		rpmi_uint32_t	exit_latency_us;
		rpmi_uint32_t	wakeup_latency_us;
		rpmi_uint32_t	min_residency_us;
	} info;
};

/** RPMI hart state management (HSM) structure to manage a set of RISC-V harts.*/
struct rpmi_hsm;

/** Platform specific RPMI hart state management (HSM) operations */
struct rpmi_hsm_platform_ops {
	/** Get hart HW state (mandatory) */
	enum rpmi_hart_hw_state (*hart_get_hw_state)(void *priv,
						     rpmi_uint32_t hart_index);

	/** Prepare a hart to start (optional) */
	enum rpmi_error	(*hart_start_prepare)(void *priv,
					      rpmi_uint32_t hart_index,
					      rpmi_uint64_t start_addr);

	/** Finalize hart start (optional) */
	void		(*hart_start_finalize)(void *priv,
					       rpmi_uint32_t hart_index,
					       rpmi_uint64_t start_addr);

	/** Prepare a hart to stop (optional) */
	enum rpmi_error	(*hart_stop_prepare)(void *priv,
					     rpmi_uint32_t hart_index);

	/** Finalize hart stop (optional) */
	void		(*hart_stop_finalize)(void *priv,
					      rpmi_uint32_t hart_index);

	/** Prepare a hart to suspend (optional) */
	enum rpmi_error	(*hart_suspend_prepare)(void *priv,
						rpmi_uint32_t hart_index,
						const struct rpmi_hsm_suspend_type *suspend_type,
						rpmi_uint64_t resume_addr);

	/** Finalize hart suspend (optional) */
	void		(*hart_suspend_finalize)(void *priv,
						 rpmi_uint32_t hart_index,
						 const struct rpmi_hsm_suspend_type *suspend_type,
						 rpmi_uint64_t resume_addr);
};

/**
 * @brief Get number of harts managed by HSM instance
 *
 * @param[in] hsm		pointer to HSM instance
 * @return number of harts
 */
rpmi_uint32_t rpmi_hsm_hart_count(struct rpmi_hsm *hsm);

/**
 * @brief Get hart ID from hart index for HSM instance
 *
 * @param[in] hsm		pointer to HSM instance
 * @param[in] hart_index	index of the array of hart IDs
 * @return hart ID upon success and LIBRPMI_HSM_INVALID_HART_ID upon failure
 */
rpmi_uint32_t rpmi_hsm_hart_index2id(struct rpmi_hsm *hsm,
				     rpmi_uint32_t hart_index);

/**
 * @brief Get hart index from hart ID for HSM instance
 *
 * @param[in] hsm		pointer to HSM instance
 * @param[in] hart_id		hart ID
 * @return hart index upon success and LIBRPMI_HSM_INVALID_HART_INDEX upon failure
 */
rpmi_uint32_t rpmi_hsm_hart_id2index(struct rpmi_hsm *hsm,
				     rpmi_uint32_t hart_id);

/**
 * @brief Get number of hart suspend types handled by HSM instance
 *
 * @param[in] hsm		pointer to HSM instance
 * @return number of hart suspend types
 */
rpmi_uint32_t rpmi_hsm_get_suspend_type_count(struct rpmi_hsm *hsm);

/**
 * @brief Get hart suspend type from index in HSM instance
 *
 * @param[in] hsm			pointer to HSM instance
 * @param[in] suspend_type_index	index of the array of hart suspend types
 * @return pointer to hart suspend type upon success or NULL upon failure
 */
const struct rpmi_hsm_suspend_type *
		rpmi_hsm_get_suspend_type(struct rpmi_hsm *hsm,
					  rpmi_uint32_t suspend_type_index);

/**
 * @brief Find hart suspend type based on type value in HSM instance
 *
 * @param[in] hsm			pointer to HSM instance
 * @param[in] type			type value of hart suspend type
 * @return pointer to hart suspend type upon success or NULL upon failure
 */
const struct rpmi_hsm_suspend_type *
			rpmi_hsm_find_suspend_type(struct rpmi_hsm *hsm,
						   rpmi_uint32_t type);

/**
 * @brief Start a hart in HSM instance
 *
 * @param[in] hsm			pointer to HSM instance
 * @param[in] hart_id			hart ID of the hart to start
 * @param[in] start_addr		address where the hart will start executing
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_hsm_hart_start(struct rpmi_hsm *hsm,
				    rpmi_uint32_t hart_id,
				    rpmi_uint64_t start_addr);

/**
 * @brief Stop a hart
 *
 * @param[in] hsm			pointer to HSM instance
 * @param[in] hart_id			hart ID of the hart to stop
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_hsm_hart_stop(struct rpmi_hsm *hsm,
				   rpmi_uint32_t hart_id);

/**
 * @brief Suspend a hart
 *
 * @param[in] hsm			pointer to HSM instance
 * @param[in] hart_id			hart ID of the hart to suspend
 * @param[in] suspend_type		pointer to hart suspend type
 * @param[in] resume_addr		address where the hart will resume executing
 *					for non-retentive suspend
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_hsm_hart_suspend(struct rpmi_hsm *hsm,
				      rpmi_uint32_t hart_id,
				      const struct rpmi_hsm_suspend_type *suspend_type,
				      rpmi_uint64_t resume_addr);

/**
 * @brief Get the current HSM hart state
 *
 * @param[in] hsm			pointer to HSM instance
 * @param[in] hart_id			hart ID of the hart
 * @return returns current HSM hart state (>= 0) upon success and negative
 * error (< 0) upon failure
 */
int rpmi_hsm_get_hart_state(struct rpmi_hsm *hsm, rpmi_uint32_t hart_id);

/**
 * @brief Synchronize state of each hart with HW state
 *
 * @param[in] hsm		pointer to HSM instance
 */
void rpmi_hsm_process_state_changes(struct rpmi_hsm *hsm);

/**
 * @brief Create a leaf HSM instance to manage a set of harts
 *
 * @param[in] hart_count		number of harts to manage
 * @param[in] hart_ids			array of hart IDs
 * @param[in] suspend_type_count	number of hart suspend types
 * @param[in] suspend_types		array of hart suspend types
 * @param[in] ops			pointer to platform specific HSM operations
 * @param[in] ops_priv			pointer to private data of platform operations
 * @return pointer to RPMI HSM instance upon success and NULL upon failure
 */
struct rpmi_hsm *rpmi_hsm_create(rpmi_uint32_t hart_count,
				 const rpmi_uint32_t *hart_ids,
				 rpmi_uint32_t suspend_type_count,
				 const struct rpmi_hsm_suspend_type *suspend_types,
				 const struct rpmi_hsm_platform_ops *ops,
				 void *ops_priv);

/**
 * @brief Create a non-leaf HSM instance to manage a set of HSM instances
 *
 * @param[in] child_count		number of child HSM instances to manage
 * @param[in] child_array		array of child HSM instances
 * @return pointer to RPMI HSM instance upon success and NULL upon failure
 */
struct rpmi_hsm *rpmi_hsm_nonleaf_create(rpmi_uint32_t child_count,
					 struct rpmi_hsm **child_array);

/**
 * @brief Destroy (or free) a HSM instance
 *
 * @param[in] hsm		pointer to HSM instance
 */
void rpmi_hsm_destroy(struct rpmi_hsm *hsm);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_SYSRESETSRVGRP_INTERFACE RPMI System Reset Service Group Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI system reset service group.
 * @{
 */

/** Platform specific system reset operations */
struct rpmi_sysreset_platform_ops {
	/**
	 * Do system reset
	 * Note: this function is not expected to return
	 */
	void (*do_system_reset)(void *priv, rpmi_uint32_t sysreset_type);
};

/**
 * @brief Create a system reset service group instance
 *
 * @param[in] ops		pointer to platform specific system reset operations
 * @param[in] ops_priv		pointer to private data of platform operations
 * @return pointer to RPMI service group instance upon success and NULL upon failure
 */
struct rpmi_service_group *
rpmi_service_group_sysreset_create(rpmi_uint32_t sysreset_type_count,
				   const rpmi_uint32_t *sysreset_types,
				   const struct rpmi_sysreset_platform_ops *ops,
				   void *ops_priv);

/**
 * @brief Destroy (or free) a system reset service group instance
 *
 * @param[in] group		pointer to RPMI service group instance
 */
void rpmi_service_group_sysreset_destroy(struct rpmi_service_group *group);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_SYSSUSPENDSRVGRP_INTERFACE RPMI System Suspend Service Group Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI system suspend service group.
 * @{
 */

/** RPMI system suspend type */
struct rpmi_system_suspend_type {
	rpmi_uint32_t type;
	rpmi_uint32_t attr;
};

/** Platform specific system suspend operations */
struct rpmi_syssusp_platform_ops {
	/** Prepare for system suspend */
	enum rpmi_error	(*system_suspend_prepare)(void *priv,
						  rpmi_uint32_t hart_index,
						  const struct rpmi_system_suspend_type *syssusp_type,
						  rpmi_uint64_t resume_addr);
	/**
	 * Check if the system is ready to suspend
	 * Returns TRUE if system is ready otherwise FALSE
	 */
	rpmi_bool_t	(*system_suspend_ready)(void *priv,
						rpmi_uint32_t hart_index);
	/** Finalize system suspend */
	void		(*system_suspend_finalize)(void *priv,
						   rpmi_uint32_t hart_index,
						   const struct rpmi_system_suspend_type *syssusp_type,
						   rpmi_uint64_t resume_addr);
	/**
	 * Check if the system is ready to resume
	 * Returns TRUE if system can resume otherwise FALSE
	 */
	rpmi_bool_t	(*system_suspend_can_resume)(void *priv,
						     rpmi_uint32_t hart_index);
	/** Resume from system suspend */
	enum rpmi_error (*system_suspend_resume)(void *priv,
						 rpmi_uint32_t hart_index,
						 const struct rpmi_system_suspend_type *syssusp_type,
						 rpmi_uint64_t resume_addr);
};

/**
 * @brief Create a system suspend service group instance
 *
 * @param[in] hsm			pointer to HSM instance
 * @param[in] syssusp_type_count	number of system suspend types
 * @param[in] syssusp_types		array of system suspend type values
 * @param[in] ops			pointer to platform specific system suspend operations
 * @param[in] ops_priv			pointer to private data of platform operations
 * @return pointer to RPMI service group instance upon success and NULL upon failure
 */
struct rpmi_service_group *
rpmi_service_group_syssusp_create(struct rpmi_hsm *hsm,
				  rpmi_uint32_t syssusp_type_count,
				  const struct rpmi_system_suspend_type *syssusp_types,
				  const struct rpmi_syssusp_platform_ops *ops,
				  void *ops_priv);

/**
 * @brief Destroy (or free) a system suspend service group instance
 *
 * @param[in] group		pointer to RPMI service group instance
 */
void rpmi_service_group_syssusp_destroy(struct rpmi_service_group *group);

/**
 * @brief Create a hart state management (HSM) service group instance
 *
 * @param[in] hsm		pointer to HSM instance
 * @return pointer to RPMI service group instance upon success and NULL upon failure
 */
struct rpmi_service_group *rpmi_service_group_hsm_create(struct rpmi_hsm *hsm);

/**
 * @brief Destroy (or free) a hart state management (HSM) service group instance
 *
 * @param[in] group		pointer to RPMI service group instance
 */
void rpmi_service_group_hsm_destroy(struct rpmi_service_group *group);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_CLOCKSRVGRP_INTERFACE RPMI Clock Service Group Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI clock service group.
 * @{
 */

/** Clock rate match mode */
enum rpmi_clock_rate_match {
	RPMI_CLK_RATE_MATCH_PLATFORM	= 0,
	RPMI_CLK_RATE_MATCH_ROUND_DOWN	= 1,
	RPMI_CLK_RATE_MATCH_ROUND_UP	= 2,
	RPMI_CLK_RATE_MATCH_MAX
};

/** Supported clock states */
enum rpmi_clock_state {
	RPMI_CLK_STATE_DISABLED	= 0,
	RPMI_CLK_STATE_ENABLED	= 1,
	RPMI_CLK_STATE_MAX
};

/** Clock type based on rate format */
enum rpmi_clock_type {
	RPMI_CLK_TYPE_DISCRETE	= 0,
	RPMI_CLK_TYPE_LINEAR	= 1,
	RPMI_CLK_TYPE_MAX
};

/** A clock rate representation in RPMI */
struct rpmi_clock_rate {
	rpmi_uint32_t lo;
	rpmi_uint32_t hi;
};

/**
 * Clock Data and Tree details
 *
 * This structure represents the static
 * clock data which platform has to maintain
 * and pass to create the clock service group.
 */
struct rpmi_clock_data {
	/* Parent clock ID */
	rpmi_uint32_t		parent_id;
	/* Clock transition latency(milli-seconds) */
	rpmi_uint32_t		transition_latency_ms;
	/* Number of rates supported as per the clock format type */
	rpmi_uint32_t		rate_count;
	/* Clock rate format type */
	enum rpmi_clock_type	clock_type;
	/* Clock name */
	const char		*name;
	/* Clock rate array */
	const rpmi_uint64_t	*clock_rate_array;
};

/** Clock Attributes */
struct rpmi_clock_attrs {
	/** clock transition latency in milli-seconds */
	rpmi_uint32_t		transition_latency;
	/** clock rate format type */
	enum rpmi_clock_type	type;
	/** number of supported rates */
	rpmi_uint32_t		rate_count;
	/** array of supported rates */
	const rpmi_uint64_t	*rate_array;
	/* Clock name */
	const char		*name;
};

/** Platform specific clock operations(synchronous) */
struct rpmi_clock_platform_ops {
	/** Set the clock state enable/disable/others */
	enum rpmi_error (*set_state)(void *priv,
				     rpmi_uint32_t clock_id,
				     enum rpmi_clock_state state);

	/**
	 * Get state and rate together
	 */
	enum rpmi_error (*get_state_and_rate)(void *priv,
					      rpmi_uint32_t clock_id,
					      enum rpmi_clock_state *state,
					      rpmi_uint64_t *rate);

	/**
	 * Check if the requested rate is not in the allowed margin(Hz)
	 * which require change in clock rate.
	 * Returns TRUE if rate change required otherwise FALSE
	 */
	rpmi_bool_t	(*rate_change_match)(void *priv,
					     rpmi_uint32_t clock_id,
					     rpmi_uint64_t rate);

	/**
	 * Set clock rate.
	 * Also based on the rate match mode and PLL lock frequency
	 * the actual frequency set may have +-margin with requested rate.
	 * Return the set rate in new_rate buffer
	 */
	enum rpmi_error (*set_rate)(void *priv,
				    rpmi_uint32_t clock_id,
				    enum rpmi_clock_rate_match match,
				    rpmi_uint64_t rate,
				    rpmi_uint64_t *new_rate);

	/**
	 * Recalculate and set rate.
	 * Recalculate and set the clock rate based on the new input(parent)
	 * clock and return the new rate in buffer.
	 */
	enum rpmi_error (*set_rate_recalc)(void *priv,
					   rpmi_uint32_t clock_id,
					   rpmi_uint64_t parent_rate,
					   rpmi_uint64_t *new_rate);
};

/**
 * @brief Create a clock service group instance
 *
 * @param[in] clock_mod		pointer to clock module
 * @return rpmi_service_group *	pointer to RPMI service group instance upon
 * success and NULL upon failure
 */
struct rpmi_service_group *
rpmi_service_group_clock_create(rpmi_uint32_t clock_count,
				const struct rpmi_clock_data *clock_tree_data,
				const struct rpmi_clock_platform_ops *ops,
				void *ops_priv);

/**
 * @brief Destroy (or free) a clock service group instance
 *
 * @param[in] group	pointer to RPMI service group instance
 */
void rpmi_service_group_clock_destroy(struct rpmi_service_group *group);

/** @} */

/**
 * \defgroup LIBRPMI_CPPCSRVGRP_INTERFACE RPMI CPPC Service Group Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI cppc service group.
 * @{
 */

 /** ACPI CPPC Register IDs as per the SBI CPPC extension. */
#define	RPMI_CPPC_HIGHEST_PERF			0x00000000
#define	RPMI_CPPC_NOMINAL_PERF			0x00000001
#define RPMI_CPPC_LOWEST_NON_LINEAR_PERF	0x00000002
#define RPMI_CPPC_LOWEST_PERF			0x00000003
#define RPMI_CPPC_GUARANTEED_PERF		0x00000004
#define RPMI_CPPC_DESIRED_PERF			0x00000005
#define RPMI_CPPC_MIN_PERF			0x00000006
#define RPMI_CPPC_MAX_PERF			0x00000007
#define RPMI_CPPC_PERF_REDUCTION_TOLERANCE	0x00000008
#define RPMI_CPPC_TIME_WINDOW			0x00000009
#define RPMI_CPPC_COUNTER_WRAPAROUND_TIME	0x0000000A
#define RPMI_CPPC_REFERENCE_PERF_COUNTER	0x0000000B
#define RPMI_CPPC_DELIVERED_PERF_COUNTER	0x0000000C
#define RPMI_CPPC_PERF_LIMITED			0x0000000D
#define RPMI_CPPC_CPPC_ENABLE			0x0000000E
#define RPMI_CPPC_AUTONOMOUS_SELECTION_ENABLE	0x0000000F
#define RPMI_CPPC_AUTONOMOUS_ACTIVITY_WINDOW	0x00000010
#define RPMI_CPPC_ENERGY_PERF_PREFERENCE	0x00000011
#define RPMI_CPPC_REFERENCE_PERF		0x00000012
#define RPMI_CPPC_LOWEST_FREQ			0x00000013
#define RPMI_CPPC_NOMINAL_FREQ			0x00000014
#define RPMI_CPPC_ACPI_REG_MAX_IDX		0x00000015
/* End of the ACPI CPPC registers */
#define RPMI_CPPC_TRANSITION_LATENCY		0x80000000
/* End of the register namespace */
#define RPMI_CPPC_NON_ACPI_REG_MAX_IDX		0x80000001

/**
 * CPPC Mode of Operation.
 */
enum rpmi_cppc_mode {
	/**
	 * Default CPPC mode in which supervisor software
	 * uses Desired Performance Register to request
	 * for performance control.
	 */
	RPMI_CPPC_PASSIVE_MODE,

	/**
	 * CPPC2 mode also called Autonomous mode, which
	 * uses Minimum Performance, Maximum Performance
	 * and Energy Performance Preference Register to
	 * for performance control
	 */
	RPMI_CPPC_AUTO_MODE,
};

/**
 * ACPI CPPC Registers
 */
struct rpmi_cppc_regs {
	/* highest performance (r) */
	rpmi_uint32_t highest_perf;
	/* nominal performance (r) */
	rpmi_uint32_t nominal_perf;
	/* lowest nonlinear performance (r) */
	rpmi_uint32_t lowest_nonlinear_perf;
	/* lowest performance (r) */
	rpmi_uint32_t lowest_perf;
	/* guaranteed performance register (r) */
	rpmi_uint32_t guaranteed_perf;
	/* desired performance register (rw) */
	rpmi_uint32_t desired_perf;
	/* minimum performance register (rw) */
	rpmi_uint32_t min_perf;
	/* maximum performance register (rw) */
	rpmi_uint32_t max_perf;
	/* performance reduction tolerance register (rw) */
	rpmi_uint32_t perf_reduction_tolerence;
	/* time window register (rw) */
	rpmi_uint32_t time_window;
	/* counter wrap around time (r) */
	rpmi_uint64_t counter_wraparound_time;
	/* reference performance counter (r) */
	rpmi_uint64_t reference_perf_counter;
	/* delivered performance counter (r) */
	rpmi_uint64_t delivered_perf_counter;
	/* performance limited register (rw) */
	rpmi_uint32_t perf_limited;
	/* cppc enable register (rw) */
	rpmi_uint32_t cppc_enable;
	/* autonomous selection enable register (rw) */
	rpmi_uint32_t autonomous_selection_enable;
	/* autonomous activity window register (rw) */
	rpmi_uint32_t autonomous_activity_window;
	/* energy performance preference register (rw) */
	rpmi_uint32_t energy_perf_preference;
	/* reference performance (r) */
	rpmi_uint32_t reference_perf;
	/* lowest frequency (r) */
	rpmi_uint32_t lowest_freq;
	/* nominal frequency (r) */
	rpmi_uint32_t nominal_freq;
	/* transition latency */
	rpmi_uint32_t transition_latency;
};

/* CPPC Fastchannel size of both types as per RPMI spec */
#define RPMI_CPPC_FASTCHAN_SIZE		8

/**
 * CPPC Performance Request Fastchannel
 *
 * Per-hart. The application processor will use this
 * fast-channel for performance request change either
 * by writing the desired performance level or the
 * performance limit change.
 */
union rpmi_cppc_perf_request_fastchan {
	/** CPPC passive(default) mode fastchannel */
	struct {
		rpmi_uint32_t desired_perf;
		rpmi_uint32_t __reserved;
	} passive;

	/** CPPC Active(autonomous) mode fastchannel */
	struct {
		rpmi_uint32_t min_perf;
		rpmi_uint32_t max_perf;
	} active;
};

/**
 * CPPC Performance Feedback Fast-channel
 *
 * Per-hart. The platform microcontroller will write the
 * latest current frequency after every performance change
 * in this fast-channel.
 *
 * The target frequency applied is not directly deducible
 * from the performance level since the final target frequency
 * of the application processor is the result of many
 * platform heuristics.
 *
 * The frequency is in Hertz
 */

struct rpmi_cppc_perf_feedback_fastchan {
	rpmi_uint32_t cur_freq_low;
	rpmi_uint32_t cur_freq_high;
};

struct rpmi_cppc_platform_ops {
	/**
	 * cppc get register value for a hart.
	 */
	enum rpmi_error (*cppc_get_reg)(void *priv,
					rpmi_uint32_t reg_id,
					rpmi_uint32_t hart_index,
					rpmi_uint64_t *val);

	/**
	 * cppc set register value for a hart.
	 */
	enum rpmi_error (*cppc_set_reg)(void *priv,
					rpmi_uint32_t reg_id,
					rpmi_uint32_t hart_index,
					rpmi_uint64_t val);
	/**
	 * cppc update performance level for a hart
	 */
	enum rpmi_error (*cppc_update_perf)(void *priv,
					    rpmi_uint32_t hart_index,
					    rpmi_uint32_t desired_perf);
	/**
	 * cppc get current frequency in hertz for a hart
	 */
	enum rpmi_error (*cppc_get_current_freq)(void *priv,
						 rpmi_uint32_t hart_index,
						 rpmi_uint64_t *current_freq_hz);
};

/**
 * @brief Create a cppc service group instance
 *
 * @param[in] hsm		pointer to struct rpmi_hsm instance
 * @param[in] cppc_regs		pointer to cppc register static data
 * @param[in] mode		cppc mode (passive or active)
 * @param[in] shmem_fastchan	pointer to fastchannel shared memory instance
 * @param[in] perf_request_shmem_offset	perf request fastchannel shmem region offset
 * @param[in] perf_feedback_shmem_offset	perf feedback fastchannel shmem region offset
 * @param[in] ops		pointer to platform specific cppc operations
 * @param[in] ops_priv		pointer to private data of platform operations
 * @return rpmi_service_group *	pointer to RPMI service group instance upon
 * success and NULL upon failure
 */
struct rpmi_service_group *
rpmi_service_group_cppc_create(struct rpmi_hsm *hsm,
			       const struct rpmi_cppc_regs *cppc_regs,
			       enum rpmi_cppc_mode mode,
			       struct rpmi_shmem *shmem_fastchan,
			       rpmi_uint64_t perf_request_shmem_offset,
			       rpmi_uint64_t perf_feedback_shmem_offset,
			       const struct rpmi_cppc_platform_ops *ops,
			       void *ops_priv);

/**
 * @brief Destroy (or free) a cppc service group instance
 *
 * @param[in] group	pointer to RPMI service group instance
 */
void rpmi_service_group_cppc_destroy(struct rpmi_service_group *group);

/** @} */

/**
 * \defgroup LIBRPMI_SYSMSISRVGRP_INTERFACE RPMI System MSI Service Group Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI system MSI service group.
 * @{
 */

/** Preferred privilege level bit of the MSI attributes */
#define RPMI_SYSMSI_MSI_ATTRIBUTES_FLAG0_PREF_PRIV	(1U << 0)

/** System MSI state enable bit */
#define RPMI_SYSMSI_MSI_STATE_ENABLE			(1U << 0)

/** System MSI state pending bit */
#define RPMI_SYSMSI_MSI_STATE_PENDING			(1U << 1)

struct rpmi_sysmsi_platform_ops {
	/** Check whether given MSI target address is valid or not (Mandatory) */
	rpmi_bool_t	(*validate_msi_addr)(void *priv, rpmi_uint64_t msi_addr);

	/** Check whether M-mode is the preferred for handling given system MSI (Optional) */
	rpmi_bool_t	(*mmode_preferred)(void *priv, rpmi_uint32_t msi_index);

	/** Get the name of given system MSI (Optional) */
	void		(*get_name)(void *priv, rpmi_uint32_t msi_index,
				    char *out_name, rpmi_uint32_t out_name_sz);
};

/**
 * @brief Inject a MSI to the system MSI service group instance
 *
 * @param[in] group	pointer to RPMI service group instance
 * @param[in] msi_index	system MSI index
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_service_group_sysmsi_inject(struct rpmi_service_group *group,
						 rpmi_uint32_t msi_index);

/**
 * @brief Inject P2A doorbell system MSI to the system MSI service group instance
 *
 * @param[in] group	pointer to RPMI service group instance
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_service_group_sysmsi_inject_p2a(struct rpmi_service_group *group);

/**
 * @brief Destroy (or free) a system MSI service group instance
 *
 * @param[in] group	pointer to RPMI service group instance
 */
void rpmi_service_group_sysmsi_destroy(struct rpmi_service_group *group);

/**
 * @brief Create a system MSI service group instance
 *
 * @param[in] num_msi		number of system MSIs
 * @param[in] p2a_msi_index	system MSI index of P2A doorbell
 *				(Note: A value greater than num_msi means not supported)
 * @param[in] ops		pointer to platform specific system MSI operations
 * @param[in] ops_priv		pointer to private data of platform operations
 * success and NULL upon failure
 */
struct rpmi_service_group *
rpmi_service_group_sysmsi_create(rpmi_uint32_t num_msi,
				 rpmi_uint32_t p2a_msi_index,
				 const struct rpmi_sysmsi_platform_ops *ops,
				 void *ops_priv);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_DPWRSRVGRP_INTERFACE RPMI Device Power Service Group Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI device power service group.
 * @{
 */

/** Supported dpwr states */
enum rpmi_dpwr_state {
	RPMI_DPWR_STATE_INVALID		= -1,
	RPMI_DPWR_STATE_ON		= 0,
	RPMI_DPWR_STATE_OFF		= 3,
	RPMI_DPWR_STATE_MAX,
};

/**
 * DPWR Data and Tree details
 *
 * This structure represents the static
 * dpwr data which platform has to maintain
 * and pass to create the dpwr service group.
 */
struct rpmi_dpwr_data {
	/** worst case transition latency from one power state to another */
	rpmi_uint32_t	trans_latency;
	/* dpwr name */
	const char	name[16];
};

/** Device Power Domain Attributes */
struct rpmi_dpwr_attrs {
	/** dpwr service return status */
	rpmi_uint32_t	status;
	/** worst case transition latency from one power state to another */
	rpmi_uint32_t	trans_latency;
	/** dpwr domain name */
	const char	*name;
};

/** Platform specific dpwr operations(synchronous) */
struct rpmi_dpwr_platform_ops {
	/**
	 * Get device power state
	 **/
	enum rpmi_error (*get_state)(void *priv,
				     rpmi_uint32_t dpwr_id,
				     rpmi_uint32_t *state);
	/**
	 * Set device power state
	 **/
	enum rpmi_error (*set_state)(void *priv,
				     rpmi_uint32_t dpwr_id,
				     rpmi_uint32_t state);
};

/**
 * @brief Create a device power service group instance
 *
 * @param[in] dpwr_count        number of device power domains
 * @param[in] dpwr_tree_data    pointer to device power domain data
 * @param[in] ops               pointer to platform specific device power operations
 * @param[in] ops_priv          pointer to private data of platform operations
 * @return rpmi_service_group * pointer to RPMI service group instance upon
 * success and NULL upon failure
 */
struct rpmi_service_group *
rpmi_service_group_dpwr_create(rpmi_uint32_t dpwr_count,
                               const struct rpmi_dpwr_data *dpwr_tree_data,
                               const struct rpmi_dpwr_platform_ops *ops,
                               void *ops_priv);

/**
 * @brief Destroy(free) a device power service group instance
 *
 * @param[in] group     pointer to RPMI service group instance
 */
void rpmi_service_group_dpwr_destroy(struct rpmi_service_group *group);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_PERFSRVGRP_INTERFACE RPMI Perf Service Group Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI perf service group.
 * @{
 */

/** Perf capabilities */
#define RPMI_PERF_CAPABILITY_SET_LIMIT			(1U << 2)
#define RPMI_PERF_CAPABILITY_SET_LEVEL			(1U << 1)
#define RPMI_PERF_CAPABILITY_FAST_CHANNEL_SUPPORT	(1U << 0)

/** Fastchannel flags */
#define RPMI_PERF_FST_CHN_DB_REG_08_BITS		(0U << 1)
#define RPMI_PERF_FST_CHN_DB_REG_16_BITS		(1U << 1)
#define RPMI_PERF_FST_CHN_DB_REG_32_BITS		(2U << 1)

#define RPMI_PERF_FST_CHN_DB_NOT_SUPP			(0U << 0)
#define RPMI_PERF_FST_CHN_DB_SUPP			(1U << 0)

/** Fastchannel operation types */
enum {
	/* get domain perf level using fastchannel */
	RPMI_PERF_FC_GET_LEVEL				= 0x0,
	/* set domain perf level using fastchannel */
	RPMI_PERF_FC_SET_LEVEL				= 0x1,
	/* get domain perf limit using fastchannel */
	RPMI_PERF_FC_GET_LIMIT				= 0x2,
	/* set domain perf limit using fastchannel */
	RPMI_PERF_FC_SET_LIMIT				= 0x3,
	/* maximum number of fastchannel operations */
	RPMI_PERF_FC_MAX,
};

/** A Perf level representation in RPMI */
struct rpmi_perf_level {
	rpmi_uint32_t level_index;
	rpmi_uint32_t clock_freq;
	rpmi_uint32_t power_cost;
	rpmi_uint32_t transition_latency;
};

/* Perf fast-channel shared memory info */
struct rpmi_perf_fc_memory_region {
	rpmi_uint32_t addr_low;
	rpmi_uint32_t addr_high;
	rpmi_uint32_t size_low;
	rpmi_uint32_t size_high;
};

/** Perf Domain Fast Channel Attributes */
struct rpmi_perf_fc_attrs {
	/** Fast Channel flags */
	rpmi_uint32_t flags;
	/** offset of phys addr low */
	rpmi_uint32_t offset_phys_addr_low;
	/** offset of phys addr high */
	rpmi_uint32_t offset_phys_addr_high;
	/** size */
	rpmi_uint32_t size;
	/** doorbell addr low */
	rpmi_uint32_t db_addr_low;
	/** doorbell addr high */
	rpmi_uint32_t db_addr_high;
	/** doorbell id */
	rpmi_uint32_t db_id;
};

/**
 * Perf Data and Tree details
 *
 * This structure represents the static
 * perf data which platform has to maintain
 * and pass to create the perf service group.
 */
struct rpmi_perf_data {
	/* Perf domain name */
	const char			*name;
	/* Min time required between two consecutive requests (us) */
	rpmi_uint32_t			trans_latency;
	/* Perf capabilities */
	rpmi_uint32_t			perf_capabilities;
	/* Number of levels supported */
	rpmi_uint32_t			perf_level_count;
	/* Perf level array */
	struct rpmi_perf_level		*perf_level_array;
	/* Fast Channel attributes array */
	struct rpmi_perf_fc_attrs	*fc_attrs_array;
};

/** Perf Domain Attributes */
struct rpmi_perf_attrs {
	/** perf service return status */
	rpmi_int32_t		status;
	/** perf capabilities and constraints */
	rpmi_uint32_t		capability;
	/** number of supported levels */
	rpmi_uint32_t		level_count;
	/** min time required between two consecutive requests (us) */
	rpmi_uint32_t		trans_latency;
	/** array of supported levels */
	struct rpmi_perf_level	*level_array;
	/** perf domain name */
	const char		*name;
};

/** Platform specific perf operations(synchronous) */
struct rpmi_perf_platform_ops {
	/**
	 * Get perf level
	 **/
	enum rpmi_error (*get_level)(void *priv,
				     rpmi_uint32_t perf_id,
				     rpmi_uint32_t *state);

	/**
	 * Set perf level
	 **/
	enum rpmi_error (*set_level)(void *priv,
				     rpmi_uint32_t perf_id,
				     rpmi_uint32_t perf_level);

	/**
	 * Get perf limit
	 **/
	enum rpmi_error (*get_limit)(void *priv,
				     rpmi_uint32_t perf_id,
				     rpmi_uint32_t *max_perf_limit,
				     rpmi_uint32_t *min_perf_limit);

	/**
	 * Set perf limit
	 **/
	enum rpmi_error (*set_limit)(void *priv,
				     rpmi_uint32_t perf_id,
				     rpmi_uint32_t max_perf_limit,
				     rpmi_uint32_t min_perf_limit);
};

/**
 * @brief Create a performance service group instance
 *
 * @param[in] perf_mod          pointer to performance module
 * @return rpmi_service_group * pointer to RPMI service group instance upon
 * success and NULL upon failure
 */
struct rpmi_service_group *
rpmi_service_group_perf_create(rpmi_uint32_t perf_count,
			       const struct rpmi_perf_data *perf_tree_data,
			       const struct rpmi_perf_platform_ops *ops,
			       const struct rpmi_perf_fc_memory_region *fc_mem_region,
			       void *ops_priv);

/**
 * @brief Destroy(free) a performance service group instance
 *
 * @param[in] group     pointer to RPMI service group instance
 */
void rpmi_service_group_perf_destroy(struct rpmi_service_group *group);

/** @} */

/*************************************************************************************/

/**
 * \defgroup LIBRPMI_VOLTSRVGRP_INTERFACE RPMI Voltage Service Group Library Interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for RPMI Voltage service group.
 * @{
 */

/** Supported voltage domain states */
enum rpmi_voltage_state {
	RPMI_VOLT_STATE_INVALID			= -1,
	RPMI_VOLT_STATE_DISABLED		= 0,
	RPMI_VOLT_STATE_ENABLED			= 1,
	RPMI_VOLT_STATE_ALWAYS_ON		= 2,
	RPMI_VOLT_STATE_MAX,
};

/** Voltage type based on voltage format */
enum rpmi_voltage_type {
	RPMI_VOLT_TYPE_INVALID			= -1,
	RPMI_VOLT_TYPE_DISCRETE			= 0,
	RPMI_VOLT_TYPE_LINEAR			= 2,
	RPMI_VOLT_TYPE_MAX,
};

/** voltage domain capabilities */
enum rpmi_voltage_capability {
	RPMI_VOLT_CAPABILITY_INVALID		= -1,
	RPMI_VOLT_CAPABILITY_ENABLED_DISABLED	= 0,
	RPMI_VOLT_CAPABILITY_ALWAYS_ON		= 1,
	RPMI_VOLT_CAPABILITY_MAX,
};

/** voltage domain config */
enum rpmi_voltage_config {
	RPMI_VOLT_CONFIG_NOT_SUPPORTED		= 0,
	RPMI_VOLT_CONFIG_ENABLED		= 1,
	RPMI_VOLT_CONFIG_DISABLED		= 2,
	RPMI_VOLT_CONFIG_MAX,
};

struct rpmi_voltage_discrete_range {
	rpmi_uint32_t *uvolt;
};

struct rpmi_voltage_linear_range {
	rpmi_uint32_t uvolt_min;
	rpmi_uint32_t uvolt_max;
	rpmi_uint32_t uvolt_step;
};

/**
 * Voltage Data and Tree details
 *
 * This structure represents the static
 * voltage domain data which platform has to maintain
 * and pass to create the voltage service group.
 */
struct rpmi_voltage_data {
	/* voltage domain name */
	const char				*name;
	/* voltage format */
	rpmi_uint32_t				voltage_type;
	/** regulator control */
	rpmi_uint32_t				control;
	/* regulator config */
	rpmi_uint32_t				config;
	/** number of supported voltage levels */
	rpmi_uint32_t				num_levels;
	/** transition latency */
	rpmi_uint32_t				trans_latency;
        /** discrete voltage range */
        struct rpmi_voltage_discrete_range	*discrete_range;
        /** linear voltage range */
        struct rpmi_voltage_linear_range	*linear_range;
	/** discrete voltage levels */
	rpmi_int32_t				*discrete_levels;
	/** linear voltage levels */
	rpmi_int32_t				*linear_levels;
	/** voltage level */
	rpmi_int32_t				level_uv;
};

/** Voltage Domain Attributes */
struct rpmi_voltage_attrs {
	/** voltage service return status */
	rpmi_int32_t	status;
	/* regulator capability */
	rpmi_uint32_t	capability;
	/* regulator config */
	rpmi_uint32_t	config;
	/** number of supported levels */
	rpmi_uint32_t	num_levels;
	/* transition latency */
	rpmi_uint32_t	trans_latency;
	/** array of supported levels */
	rpmi_int32_t	*level_array;
	/* voltage domain name */
	const char	*name;
};

/** Platform specific voltage operations(synchronous) */
struct rpmi_voltage_platform_ops {
	/**
	 * set config of voltage regulator
	 **/
	enum rpmi_error (*set_config)(void *priv,
				      rpmi_uint32_t volt_id,
				      rpmi_uint32_t config);

	/**
	 * get config of voltage regulator
	 **/
	enum rpmi_error (*get_config)(void *priv,
				      rpmi_uint32_t volt_id,
				      rpmi_uint32_t *config);

	/**
	 * set level of voltage regulator
	 **/
	enum rpmi_error (*set_level)(void *priv,
				     rpmi_uint32_t volt_id,
				     rpmi_int32_t *volt_level);

	/**
	 * get level of voltage regulator
	 **/
	enum rpmi_error (*get_level)(void *priv,
				     rpmi_uint32_t volt_id,
				     rpmi_int32_t *volt_level);

	/**
	 * Get perf supported levels
	 **/
	enum rpmi_error (*get_supp_levels)(void *priv,
					   rpmi_uint32_t volt_id,
					   rpmi_uint32_t max,
					   rpmi_uint32_t volt_index,
					   rpmi_uint32_t *returned_levels,
					   rpmi_int32_t *level_array);
};

/**
 * @brief Create a device voltage service group instance
 *
 * @param[in] voltage_count        number of voltage domains
 * @param[in] voltage_tree_data    pointer to voltage domain data
 * @param[in] ops                  pointer to platform specific voltage operations
 * @param[in] ops_priv             pointer to private data of platform operations
 * @return rpmi_service_group *    pointer to RPMI service group instance upon
 * success and NULL upon failure
 */
struct rpmi_service_group *
rpmi_service_group_voltage_create(rpmi_uint32_t voltage_count,
				  const struct rpmi_voltage_data *voltage_tree_data,
				  const struct rpmi_voltage_platform_ops *ops,
				  void *ops_priv);

/**
 * @brief Destroy(free) a voltage service group instance
 *
 * @param[in] group     pointer to RPMI service group instance
 */
void rpmi_service_group_voltage_destroy(struct rpmi_service_group *group);

/** @} */

#endif /* __LIBRPMI_H__ */
