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
 * +------------------+--------------+-----------+
 * |  SERVICEGROUP_ID |  SERVICE_ID  |   FLAGS   |
 * +------------------+--+-----------+-----------+
 * |        TOKEN        |     DATA LENGTH       |
 * +---------------------+-----------------------+
 * |                 DATA/PAYLOAD                |
 * +---------------------------------------------+
 */

/** Message Header Offset */
#define RPMI_MSG_HDR_OFFSET			(0x0)
#define RPMI_MSG_HDR_SIZE			(8)

/** Flags field */
#define RPMI_MSG_FLAGS_OFFSET			(0x0)
#define RPMI_MSG_FLAGS_SIZE			(1)

#define RPMI_MSG_FLAGS_TYPE_POS			(0U)
#define RPMI_MSG_FLAGS_TYPE_MASK		0x3
#define RPMI_MSG_FLAGS_TYPE			\
	((0x3) << RPMI_MSG_FLAGS_TYPE_POS)

#define RPMI_MSG_FLAGS_DOORBELL_POS		(3U)
#define RPMI_MSG_FLAGS_DOORBELL_MASK		0x1
#define RPMI_MSG_FLAGS_DOORBELL			\
	((0x1) << RPMI_MSG_FLAGS_DOORBELL_POS)

/** Service ID field */
#define RPMI_MSG_SERVICE_ID_OFFSET		(0x1)
#define RPMI_MSG_SERVICE_ID_SIZE		(1)

/** ServiceGroup ID field */
#define RPMI_MSG_SERVICEGROUP_ID_OFFSET		(0x2)
#define RPMI_MSG_SERVICEGROUP_ID_SIZE		(2)

/** Data length field */
#define RPMI_MSG_DATALEN_OFFSET			(0x4)
#define RPMI_MSG_DATALEN_SIZE			(2)

/** Token is unique message identifier in the system */
#define RPMI_MSG_TOKEN_OFFSET			(0xa)
#define RPMI_MSG_TOKEN_SIZE			(2)

/** Data field */
#define RPMI_MSG_DATA_OFFSET			(RPMI_MSG_HDR_SIZE)
#define RPMI_MSG_DATA_SIZE(__slot_size)		((__slot_size) - RPMI_MSG_HDR_SIZE)

/** Minimum slot size */
#define RPMI_SLOT_SIZE_MIN			(64)

/** RPMI Messages Types */
enum rpmi_message_type {
	/* Normal request backed with ack */
	RPMI_MSG_NORMAL_REQUEST = 0x0,
	/* Request without any ack */
	RPMI_MSG_POSTED_REQUEST = 0x1,
	/* Acknowledgment for normal request message */
	RPMI_MSG_ACKNOWLDGEMENT = 0x2,
	/* Notification message */
	RPMI_MSG_NOTIFICATION = 0x3,
};

/** RPMI Message Header */
struct rpmi_message_header {
	rpmi_uint8_t flags;
	rpmi_uint8_t service_id;
	rpmi_uint16_t servicegroup_id;
	rpmi_uint16_t datalen;
	rpmi_uint16_t token;
};

/** RPMI Message */
struct rpmi_message {
	struct rpmi_message_header header;
	rpmi_uint8_t data[0];
};

/** RPMI Error Types */
enum rpmi_error {
	/* Success */
	RPMI_SUCCESS		= 0,
	/* General fail */
	RPMI_ERR_FAILED		= -1,
	/* Service/feature not supported */
	RPMI_ERR_NOTSUPP	= -2,
	/* Invalid Parameter  */
	RPMI_ERR_INVAL		= -3,
	/* Insufficier permissions  */
	RPMI_ERR_DENIED		= -4,
	/* Requested resource not found */
	RPMI_ERR_NOTFOUND	= -5,
	/* Requested resource out of range */
	RPMI_ERR_OUTOFRANGE	= -6,
	/* Resource limit reached */
	RPMI_ERR_OUTOFRES	= -7,
	/* Operation failed due to hardware issues  */
	RPMI_ERR_HWFAULT	= -8,
	/* System currently busy, retry later */
	RPMI_ERR_BUSY		= -9,
	/* Operation timed out*/
	RPMI_ERR_TIMEOUT	= -10,
	/* Error in communication, retry later */
	RPMI_ERR_COMMS		= -11,
	/*
	 * Operation failed as it was already in progress or the state has changed
	 * already for which the operation was carried out.
	 */
	RPMI_ERR_ALREADY	= -12,
	/* Error in implementsation which violates the specification version */
	RPMI_ERR_IMPL		= -13,
	RPMI_ERR_RESERVED_START	= -14,
	RPMI_ERR_RESERVED_END	= -127,
	RPMI_ERR_VENDOR_START	= -128,
};

/** RPMI Queue Types */
enum rpmi_queue_type {
	RPMI_QUEUE_A2P_REQ = 0,
	RPMI_QUEUE_P2A_ACK,
	RPMI_QUEUE_P2A_REQ,
	RPMI_QUEUE_A2P_ACK,
	RPMI_QUEUE_MAX,
};

/** RPMI ServiceGroups IDs */
enum rpmi_servicegroup_id {
	RPMI_SRVGRP_ID_MIN = 0,
	RPMI_SRVGRP_BASE = 0x00001,
	RPMI_SRVGRP_SYSTEM_RESET = 0x00002,
	RPMI_SRVGRP_ID_MAX_COUNT,
};

/** RPMI Base ServiceGroup Service IDs */
enum rpmi_base_service_id {
	RPMI_BASE_SRV_ENABLE_NOTIFICATION = 0x01,
	RPMI_BASE_SRV_GET_IMPLEMENTATION_VERSION = 0x02,
	RPMI_BASE_SRV_GET_IMPLEMENTATION_IDN = 0x03,
	RPMI_BASE_SRV_GET_SPEC_VERSION = 0x04,
	RPMI_BASE_SRV_GET_HW_INFO = 0x05,
	RPMI_BASE_SRV_PROBE_SERVICE_GROUP = 0x06,
	RPMI_BASE_SRV_GET_ATTRIBUTES = 0x07,
	RPMI_BASE_SRV_SET_MSI = 0x08,
	RPMI_BASE_SRV_MAX = 0x09,
};

#define RPMI_BASE_VERSION_MINOR_POS		0
#define RPMI_BASE_VERSION_MINOR_MASK		0xffff

#define RPMI_BASE_VERSION_MAJOR_POS		16
#define RPMI_BASE_VERSION_MAJOR_MASK		0xffff

#define RPMI_BASE_VERSION(__major, __minor)	\
((((__major) & RPMI_BASE_VERSION_MAJOR_MASK) << RPMI_BASE_VERSION_MAJOR_POS) | \
 (((__minor) & RPMI_BASE_VERSION_MINOR_MASK) << RPMI_BASE_VERSION_MINOR_POS))

#define RPMI_BASE_VENDOR_ID(__id, __subid)	\
((((__subid) & 0xffff) << 16) | ((__id) & 0xffff))

#define RPMI_BASE_FLAGS_F0_EV_NOTIFY		(1U << 31)
#define RPMI_BASE_FLAGS_F0_MSI_EN		(1U << 30)

/** RPMI System Reset ServiceGroup Service IDs */
enum rpmi_sysreset_service_id {
	RPMI_SYSRST_SRV_ENABLE_NOTIFICATION = 0x01,
	RPMI_SYSRST_SRV_GET_SYSTEM_RESET_ATTRIBUTES = 0x02,
	RPMI_SYSRST_SRV_SYSTEM_RESET = 0x03,
	RPMI_SYSRST_SRV_MAX = 0x04,
};

#define RPMI_SYSRST_TYPE_SHUTDOWN		0U
#define RPMI_SYSRST_TYPE_COLD_REBOOT		1U
#define RPMI_SYSRST_TYPE_WARM_REBOOT		2U

#define RPMI_SYSRST_ATTRIBUTES_FLAGS_SUPPORTED	(1U << 31)

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_COMMON_INTERFACE RPMI common library.interface
 * @brief Common defines and structures for the RPMI library.
 * @{
 */

#define LIBRPMI_IMPL_ID					0

#define LIBRPMI_IMPL_VERSION_MAJOR			0
#define LIBRPMI_IMPL_VERSION_MINOR			1


#define LIBRPMI_TRANSPORT_SHMEM_MIN_SLOTS		16
#define LIBRPMI_TRANSPORT_SHMEM_MIN_SIZE(__slot_size)	\
	((__slot_size) * LIBRPMI_TRANSPORT_SHMEM_MIN_SLOTS * RPMI_QUEUE_MAX)

/** RPMI shared memory instance */
struct rpmi_shmem {
	/** Name of the shared memory */
	const char *name;

	/** Size of the shared memory */
	rpmi_uint32_t size;

	/** Read from a shared memory */
	enum rpmi_error (*read)(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				void *in, rpmi_uint32_t len);

	/** Write to a shared memory */
	enum rpmi_error (*write)(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				 const void *out, rpmi_uint32_t len);

	/** Fill a shared memory with a fixed character */
	enum rpmi_error (*fill)(struct rpmi_shmem *shmem, rpmi_uint32_t offset,
				char ch, rpmi_uint32_t len);

	/** Private data of the shared memory implementation */
	void *priv;
};

/**
 * @brief Get size of a shared memory
 *
 * @param[in] shmem		pointer to shared memory instance
 * @return size of shared memory in bytes
 */
static inline rpmi_uint32_t rpmi_shmem_size(struct rpmi_shmem *shmem)
{
	return (shmem) ? shmem->size : 0;
}

/**
 * @brief Read a buffer from shared memory
 *
 * @param[in] shmem		pointer to shared memory instance
 * @param[in] offset		source offset within shared memory
 * @param[in] in		destinetion buffer pointer
 * @param[in] len		number of bytes to read
 * @return enum rpmi_error
 */
static inline enum rpmi_error rpmi_shmem_read(struct rpmi_shmem *shmem,
					      rpmi_uint32_t offset,
					      void *in, rpmi_uint32_t len)
{
	if (!shmem || !shmem->read || !in)
		return RPMI_ERR_INVAL;
	if ((offset + len) > shmem->size)
		return RPMI_ERR_OUTOFRANGE;
	return shmem->read(shmem, offset, in, len);
}

/**
 * @brief Write a buffer to shared memory
 *
 * @param[in] shmem		pointer to shared memory instance
 * @param[in] offset		destinetion offset within shared memory
 * @param[in] out		source buffer pointer
 * @param[in] len		number of bytes to write
 * @return enum rpmi_error
 */
static inline enum rpmi_error rpmi_shmem_write(struct rpmi_shmem *shmem,
					       rpmi_uint32_t offset,
					       const void *out, rpmi_uint32_t len)
{
	if (!shmem || !shmem->write || !out)
		return RPMI_ERR_INVAL;
	if ((offset + len) > shmem->size)
		return RPMI_ERR_OUTOFRANGE;
	return shmem->write(shmem, offset, out, len);
}

/**
 * @brief Fill a part of shared memory with fixed character
 *
 * @param[in] shmem		pointer to shared memory instance
 * @param[in] offset		offset within shared memory
 * @param[in] ch		character value to fill
 * @param[in] len		number of characters to fill
 * @return enum rpmi_error
 */
static inline enum rpmi_error rpmi_shmem_fill(struct rpmi_shmem *shmem,
					      rpmi_uint32_t offset,
					      char ch, rpmi_uint32_t len)
{
	if (!shmem || !shmem->fill)
		return RPMI_ERR_INVAL;
	if ((offset + len) > shmem->size)
		return RPMI_ERR_OUTOFRANGE;
	return shmem->fill(shmem, offset, ch, len);
}

/**
 * @brief Create a simple shared memory instance
 *
 * @param[in] name		name of the shared memory instance
 * @param[in] base		base address of the shared memory
 * @param[in] size		size of the shared memory
 * @return pointer to RPMI shared memory instance upon success and NULL upon failure
 */
struct rpmi_shmem *rpmi_shmem_simple_create(const char *name,
					    rpmi_uint64_t base,
					    rpmi_uint32_t size);

/**
 * @brief Destroy (of free) a shared memory instance
 *
 * @param[in] shmem		pointer to shared memory instance
 */
void rpmi_shmem_simple_destroy(struct rpmi_shmem *shmem);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_TRANSPORT_INTERFACE RPMI transport library interface
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
	const char *name;

	/** Endianness of the messages transferred through this transport */
	rpmi_bool_t is_be;

	/** Slot (or max message) size in transport queues */
	rpmi_size_t slot_size;

	/**
	 * Callback to check if a RPMI queue type is empty
	 *
	 * Note: This function must be called with transport lock held.
	 */
	rpmi_bool_t (*is_empty)(struct rpmi_transport *trans,
				enum rpmi_queue_type qtype);

	/**
	 * Callback to check if a RPMI queue type is full
	 *
	 * Note: This function must be called with transport lock held.
	 */
	rpmi_bool_t (*is_full)(struct rpmi_transport *trans,
			       enum rpmi_queue_type qtype);

	/**
	 * Callback to enqueue a RPMI message to a specified RPMI queue type
	 *
	 * Note: This function must be called with transport lock held.
	 */
	enum rpmi_error (*enqueue)(struct rpmi_transport *trans,
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
	void *lock;

	/** Private data of the transport implementation */
	void *priv;
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
rpmi_bool_t rpmi_tranport_is_full(struct rpmi_transport *trans,
				  enum rpmi_queue_type qtype);

/**
 * @brief Enqueue a RPMI message to a specified RPMI queue type of a RPMI transport
 *
 * @param[in] trans		pointer to RPMI transport instance
 * @param[in] qtype		type of the RPMI queue
 * @param[in] msg		pointer to the enqueued RPMI message
 * @return enum rpmi_error
 */
enum rpmi_error rpmi_tranport_enqueue(struct rpmi_transport *trans,
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
 * @param[in] shmem		pointer to a RPMI shared memory instance
 * @return pointer to RPMI transport upon success and NULL upon failure
 */
struct rpmi_transport *rpmi_transport_shmem_create(const char *name,
						   rpmi_uint32_t slot_size,
						   struct rpmi_shmem *shmem);

/**
 * @brief Destroy (of free) a shared memory transport instance
 *
 * @param[in] trans		pointer to RPMI transport instance
 */
void rpmi_transport_shmem_destroy(struct rpmi_transport *trans);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_SERVICEGROUP_INTERFACE RPMI service group library interface
 * @brief Global functions and data structures implemented by the RPMI library
 * for managing RPMI service groups.
 * @{
 */

struct rpmi_service_group;

/** RPMI service instance */
struct rpmi_service {
	/** ID of the service */
	rpmi_uint8_t service_id;

	/** Minimum data length for handling request */
	rpmi_uint16_t min_a2p_request_datalen;

	/**
	 * Callback to process a2p request
	 *
	 * Note: This function must be called with service group lock held.
	 */
	enum rpmi_error (*process_a2p_request)(struct rpmi_service_group *group,
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
	const char *name;

	/** ID of the service group */
	rpmi_uint16_t servicegroup_id;

	/** Maximum service ID of the service group */
	rpmi_uint8_t max_service_id;

	/** Array of services indexed by service ID */
	struct rpmi_service *services;

	/**
	 * Callback to process events for a service group. This events can be:
	 *
	 * 1) Fast-channel requests from application processors
	 * 2) Pending HW interrupts relevant to a service group
	 * 3) HW state changes relevant to a service group
	 *
	 * Note: This function must be called with service group lock held.
	 */
	enum rpmi_error (*process_events)(struct rpmi_service_group *group);

	/** Lock to synchronize service group access (optional) */
	void *lock;

	/** Private data of the service group implementation */
	void *priv;
};

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
 * @brief Destroy (of free) a system reset service group instance
 *
 * @param[in] group		pointer to RPMI service group instance
 */
void rpmi_service_group_sysreset_destroy(struct rpmi_service_group *group);

/** @} */

/******************************************************************************/

/**
 * \defgroup LIBRPMI_CONTEXT_INTERFACE RPMI context library interface
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
 * @param[in] vendor_id		vendor ID of HW
 * @param[in] vendor_sub_id	vendor SUB-ID of HW
 * @param[in] hw_info_len	length of the HW info string
 * @param[in] hw_info		pointer to the HW info string
 * @return pointer to RPMI context upon success and NULL upon failure
 */
struct rpmi_context *rpmi_context_create(const char *name,
					 struct rpmi_transport *trans,
					 rpmi_uint32_t max_num_groups,
					 rpmi_uint16_t vendor_id,
					 rpmi_uint16_t vendor_sub_id,
					 rpmi_uint32_t hw_info_len,
					 const rpmi_uint8_t *hw_info);

/**
 * @brief Destroy (of free) a RPMI context
 *
 * @param[in] cntx		pointer to RPMI context instance
 */
void rpmi_context_destroy(struct rpmi_context *cntx);

/** @} */

#endif  /* __LIBRPMI_H__ */
