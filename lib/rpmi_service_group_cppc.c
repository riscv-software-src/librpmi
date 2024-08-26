
/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/** Convert bytes to bits */
#define BITS(bytes)     (bytes * 8)

/** Convert VAL(HI_32bit, LO_32bit) -> VAL(U64) */
#define VAL_U64(lo, hi)			((rpmi_uint64_t)hi << 32 | lo)
/** Get HI_32bit from Val(U64) */
#define VAL_U64TOHI32(r)		((rpmi_uint32_t)(r >> 32))
/** Get LO_32bit from Val(U64) */
#define VAL_U64TOLO32(r)		((rpmi_uint32_t)r)

/**
 * RPMI CPPC fast channel per hart
 */
union rpmi_cppc_hart_fastchan {
	/** CPPC passive(default) mode */
	struct  {
		rpmi_uint32_t desired_perf;
		rpmi_uint32_t __reserved;
	} passive;

	/** CPPC Active(autonomous) mode */
	struct {
		rpmi_uint32_t min_perf;
		rpmi_uint32_t max_perf;
	} active;
};

/**
 * RPMI CPPC fast channel context
 */
struct rpmi_cppc_fastchan {
	/** shared memory backing the fast channels */
	struct rpmi_shmem *shmem;

	/** array of fastchannel entries for all harts
	 * managed by cppc service group */
	union rpmi_cppc_hart_fastchan *hart_fastchan;
};

struct rpmi_cppc_group {
	/** hart count managed by CPPC service group */
	rpmi_uint32_t hart_count;

	/** CPPC mode of operation */
	enum rpmi_cppc_mode cppc_mode;

	/** hsm context representing all harts managed by CPPC service group */
	struct rpmi_hsm *hsm;

	/** pointer to cppc register platform data */
	const struct rpmi_cppc_regs *regs;

	/** CPPC fast channel */
	struct rpmi_cppc_fastchan *fastchan;

	/** Private data of platform cppc operations */
	const struct rpmi_cppc_platform_ops *ops;
	void *ops_priv;

	struct rpmi_service_group group;
};

/**
 * Check if a register is valid.
 * A register even if unimplemented but defined in the
 * regiser namespace(enum rpmi_cppc_reg_id) is a valid
 * register.
 */
static inline rpmi_bool_t __rpmi_cppc_reg_valid(rpmi_uint32_t reg_id)
{
	return ((reg_id < RPMI_CPPC_ACPI_REG_MAX_IDX) ||
	(reg_id >= RPMI_CPPC_TRANSITION_LATENCY &&
	reg_id < RPMI_CPPC_NON_ACPI_REG_MAX_IDX))? true : false;
}

/**
 * Get the hart fastchannel offset
 */
static inline rpmi_uint64_t __rpmi_cppc_hart_fc_offset(rpmi_uint32_t hart_index)
{
	return (hart_index * sizeof(union rpmi_cppc_hart_fastchan));
}

/**
 * Get the desired_perf value from fastchannel of a hart
 */
static inline rpmi_uint32_t
__rpmi_cppc_get_fc_desired_perf(struct rpmi_cppc_group *cppcgrp,
			     rpmi_uint32_t hart_index)
{
	int rc;
	rpmi_uint32_t val, offset;

	offset = __rpmi_cppc_hart_fc_offset(hart_index);

	rc = rpmi_shmem_read(cppcgrp->fastchan->shmem, offset, &val,
			     sizeof(rpmi_uint32_t));
	if (rc)
		return 0;

	return val;
}

/**
 * This function maintains the list of implemented
 * CPPC registers in librpmi.
 * DO NOT FORGET to change the below switch-case in
 * case of changes in the implementation status of any
 * register.
 * The register sizes are as per the ACPI CPPC specification.
 */
static enum rpmi_error __rpmi_cppc_probe_reg(struct rpmi_cppc_group *cppcgrp,
					     rpmi_uint32_t reg_id,
					     rpmi_uint32_t *reg_len)
{
	enum rpmi_error status = RPMI_SUCCESS;
	rpmi_uint32_t len;

	switch(reg_id) {
	case RPMI_CPPC_HIGHEST_PERF:
	case RPMI_CPPC_NOMINAL_PERF:
	case RPMI_CPPC_LOWEST_NON_LINEAR_PERF:
	case RPMI_CPPC_LOWEST_PERF:
	case RPMI_CPPC_DESIRED_PERF:
	case RPMI_CPPC_PERF_LIMITED:
	case RPMI_CPPC_REFERENCE_PERF:
	case RPMI_CPPC_LOWEST_FREQ:
	case RPMI_CPPC_NOMINAL_FREQ:
	case RPMI_CPPC_TRANSITION_LATENCY:
		status = RPMI_SUCCESS;
		len = BITS(sizeof(rpmi_uint32_t));
		break;
	case RPMI_CPPC_REFERENCE_PERF_COUNTER:
	case RPMI_CPPC_DELIVERED_PERF_COUNTER:
		status = RPMI_SUCCESS;
		len = BITS(sizeof(rpmi_uint64_t));
		break;
	/** Not implemented registers (will be in future) */
	case RPMI_CPPC_MAX_PERF:
	case RPMI_CPPC_MIN_PERF:
	case RPMI_CPPC_GUARANTEED_PERF:
	case RPMI_CPPC_TIME_WINDOW:
	case RPMI_CPPC_PERF_REDUCTION_TOLERANCE:
	case RPMI_CPPC_AUTONOMOUS_SELECTION_ENABLE:
	case RPMI_CPPC_CPPC_ENABLE:
	case RPMI_CPPC_COUNTER_WRAPAROUND_TIME:
	case RPMI_CPPC_AUTONOMOUS_ACTIVITY_WINDOW:
	case RPMI_CPPC_ENERGY_PERF_PREFERENCE:
	default:
		DPRINTF("%s: CPPC reg-%u not implemented\n", __func__, reg_id);
		status = RPMI_ERR_NOTSUPP;
		len = 0;
		break;
	};

	if (reg_len)
		*reg_len = len;

	return status;
}

static enum rpmi_error __rpmi_cppc_read_reg(struct rpmi_cppc_group *cppcgrp,
					    rpmi_uint32_t reg_id,
					    rpmi_uint32_t hart_index,
					    rpmi_uint64_t *reg_val)
{
	enum rpmi_error status = RPMI_SUCCESS;
	rpmi_uint64_t val = 0;

	/** More frequently read registers must be placed
	 * in the beginning of switch case */
	switch (reg_id) {
	case RPMI_CPPC_DELIVERED_PERF_COUNTER:
		status = cppcgrp->ops->cppc_get_reg(cppcgrp->ops_priv,
				RPMI_CPPC_DELIVERED_PERF_COUNTER,
				hart_index,
				&val);
		break;
	case RPMI_CPPC_REFERENCE_PERF_COUNTER:
		status = cppcgrp->ops->cppc_get_reg(cppcgrp->ops_priv,
				RPMI_CPPC_REFERENCE_PERF_COUNTER,
				hart_index,
				&val);
		break;
	case RPMI_CPPC_HIGHEST_PERF:
		val = cppcgrp->regs->highest_perf;
		break;
	case RPMI_CPPC_NOMINAL_PERF:
		val = cppcgrp->regs->nominal_perf;
		break;
	case RPMI_CPPC_LOWEST_NON_LINEAR_PERF:
		val = cppcgrp->regs->lowest_nonlinear_perf;
		break;
	case RPMI_CPPC_LOWEST_PERF:
		val = cppcgrp->regs->lowest_perf;
		break;
	case RPMI_CPPC_REFERENCE_PERF:
		val = cppcgrp->regs->reference_perf;
		break;
	case RPMI_CPPC_DESIRED_PERF:
		val = __rpmi_cppc_get_fc_desired_perf(cppcgrp, hart_index);
		break;
	case RPMI_CPPC_PERF_LIMITED:
		status = cppcgrp->ops->cppc_get_reg(cppcgrp->ops_priv,
				RPMI_CPPC_PERF_LIMITED,
				hart_index,
				&val);
		break;
	case RPMI_CPPC_LOWEST_FREQ:
		val = cppcgrp->regs->lowest_freq;
		break;
	case RPMI_CPPC_NOMINAL_FREQ:
		val = cppcgrp->regs->nominal_freq;
		break;
	case RPMI_CPPC_TRANSITION_LATENCY:
		val = cppcgrp->regs->transition_latency;
		break;
	default:
		/* No read permission */
		status = RPMI_ERR_DENIED;
		val = 0;
		break;
	};

	*reg_val = val;
	return status;
}

static enum rpmi_error __rpmi_cppc_write_reg(struct rpmi_cppc_group *cppcgrp,
					     rpmi_uint32_t reg_id,
					     rpmi_uint32_t hart_index,
					     rpmi_uint64_t reg_val)
{
	enum rpmi_error status = RPMI_SUCCESS;

	switch (reg_id) {
	case RPMI_CPPC_DESIRED_PERF:
	/* If fastchannel is supported then platform expect desired
	 * performance value in fastchannel from supervisor software
	 * Any write here will not take affect and denied. */
		if (cppcgrp->fastchan) {
			status = RPMI_ERR_DENIED;
			break;
		}
		status = cppcgrp->ops->cppc_set_reg(cppcgrp->ops_priv,
						    reg_id,
						    hart_index,
						    reg_val);
		break;
	case RPMI_CPPC_DELIVERED_PERF_COUNTER:
	case RPMI_CPPC_REFERENCE_PERF_COUNTER:
	case RPMI_CPPC_HIGHEST_PERF:
	case RPMI_CPPC_NOMINAL_PERF:
	case RPMI_CPPC_LOWEST_NON_LINEAR_PERF:
	case RPMI_CPPC_LOWEST_PERF:
	case RPMI_CPPC_REFERENCE_PERF:
	case RPMI_CPPC_PERF_LIMITED:
	case RPMI_CPPC_LOWEST_FREQ:
	case RPMI_CPPC_NOMINAL_FREQ:
	case RPMI_CPPC_TRANSITION_LATENCY:
	default:
		/* No write permission */
		status = RPMI_ERR_DENIED;
		break;
	};

	return status;
}

static enum rpmi_error
rpmi_cppc_sg_probe_reg(struct rpmi_service_group *group,
		       struct rpmi_service *service,
		       struct rpmi_transport *trans,
		       rpmi_uint16_t request_datalen,
		       const rpmi_uint8_t *request_data,
		       rpmi_uint16_t *response_datalen,
		       rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t hart_id, cppc_reg_id, hart_index, resp_dlen, reg_len = 0;
	struct rpmi_cppc_group *cppcgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	hart_id = rpmi_to_xe32(trans->is_be,
			((const rpmi_uint32_t *)request_data)[0]);
	cppc_reg_id = rpmi_to_xe32(trans->is_be,
			    ((const rpmi_uint32_t *)request_data)[1]);

	/** valid cppc register */
	if (!__rpmi_cppc_reg_valid(cppc_reg_id)) {
		status = RPMI_ERR_INVAL;
		resp_dlen = sizeof(*resp);
		goto done;
	}

	/** valid hart id */
	hart_index = rpmi_hsm_hart_id2index(cppcgrp->hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		status = RPMI_ERR_NOTFOUND;
		resp_dlen = sizeof(*resp);
		goto done;
	}

	status = __rpmi_cppc_probe_reg(cppcgrp, cppc_reg_id, &reg_len);

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)reg_len);
	resp_dlen = 2 * sizeof(*resp);

done:
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_cppc_sg_read_reg(struct rpmi_service_group *group,
		      struct rpmi_service *service,
		      struct rpmi_transport *trans,
		      rpmi_uint16_t request_datalen,
		      const rpmi_uint8_t *request_data,
		      rpmi_uint16_t *response_datalen,
		      rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t hart_id, cppc_reg_id, resp_dlen, hart_index, len;
	rpmi_uint32_t data_lo, data_hi;
	rpmi_uint64_t reg_val;
	struct rpmi_cppc_group *cppcgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	hart_id = rpmi_to_xe32(trans->is_be,
			((const rpmi_uint32_t *)request_data)[0]);
	cppc_reg_id = rpmi_to_xe32(trans->is_be,
			    ((const rpmi_uint32_t *)request_data)[1]);

	data_lo = 0;
	data_hi = 0;

	/** valid cppc register */
	if (!__rpmi_cppc_reg_valid(cppc_reg_id)) {
		status = RPMI_ERR_INVAL;
		resp_dlen = sizeof(*resp);
		goto done;
	}

	/** valid hart id */
	hart_index = rpmi_hsm_hart_id2index(cppcgrp->hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		status = RPMI_ERR_NOTFOUND;
		resp_dlen = sizeof(*resp);
		goto done;
	}

	status = __rpmi_cppc_probe_reg(cppcgrp, cppc_reg_id, &len);
	if (status) {
		resp_dlen = sizeof(*resp);
		goto done;
	}

	status = __rpmi_cppc_read_reg(cppcgrp, cppc_reg_id, hart_index, &reg_val);
	if (status) {
		resp_dlen = sizeof(*resp);
		goto done;
	}

	if (len == BITS(sizeof(rpmi_uint64_t))) {
		data_lo = VAL_U64TOLO32(reg_val);
		data_hi = VAL_U64TOHI32(reg_val);
	} else if (len == BITS(sizeof(rpmi_uint32_t))) {
		data_lo = VAL_U64TOLO32(reg_val);
		data_hi = 0;
	}

	status = RPMI_SUCCESS;
	resp[1] = rpmi_to_xe32(trans->is_be, data_lo);
	resp[2] = rpmi_to_xe32(trans->is_be, data_hi);
	resp_dlen = 3 * sizeof(*resp);

done:
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_cppc_sg_write_reg(struct rpmi_service_group *group,
		       struct rpmi_service *service,
		       struct rpmi_transport *trans,
		       rpmi_uint16_t request_datalen,
		       const rpmi_uint8_t *request_data,
		       rpmi_uint16_t *response_datalen,
		       rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t hart_id, cppc_reg_id, hart_index, len;
	rpmi_uint32_t data_lo, data_hi;
	rpmi_uint64_t reg_val;
	struct rpmi_cppc_group *cppcgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	hart_id = rpmi_to_xe32(trans->is_be,
			((const rpmi_uint32_t *)request_data)[0]);
	cppc_reg_id = rpmi_to_xe32(trans->is_be,
			    ((const rpmi_uint32_t *)request_data)[1]);

	data_lo = rpmi_to_xe32(trans->is_be,
			((const rpmi_uint32_t *)request_data)[2]);

	data_hi = rpmi_to_xe32(trans->is_be,
			((const rpmi_uint32_t *)request_data)[3]);

	reg_val = VAL_U64(data_lo, data_hi);

	/** valid cppc register */
	if (!__rpmi_cppc_reg_valid(cppc_reg_id)) {
		status = RPMI_ERR_INVAL;
		goto done;
	}

	/** valid hart id */
	hart_index = rpmi_hsm_hart_id2index(cppcgrp->hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		status = RPMI_ERR_NOTFOUND;
		goto done;
	}

	status = __rpmi_cppc_probe_reg(cppcgrp, cppc_reg_id, &len);
	if (status) {
		goto done;
	}

	status = __rpmi_cppc_write_reg(cppcgrp, cppc_reg_id, hart_index, reg_val);

done:
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	*response_datalen = sizeof(*resp);
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_cppc_sg_get_fast_channel_region(struct rpmi_service_group *group,
				     struct rpmi_service *service,
				     struct rpmi_transport *trans,
				     rpmi_uint16_t request_datalen,
				     const rpmi_uint8_t *request_data,
				     rpmi_uint16_t *response_datalen,
				     rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t resp_dlen, flags;
	rpmi_uint64_t fastchan_region_base, fastchan_region_size;
	struct rpmi_cppc_group *cppcgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	if (!cppcgrp->fastchan) {
		status = RPMI_ERR_NOTSUPP;
		resp_dlen = sizeof(*resp);
		goto done;
	}

	fastchan_region_base = rpmi_shmem_base(cppcgrp->fastchan->shmem);
	fastchan_region_size = rpmi_shmem_size(cppcgrp->fastchan->shmem);

	/* No doorbell, and mode is passive */
	flags = 0;

	status = RPMI_SUCCESS;
	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)flags);
	/* fast channel region address low */
	resp[2] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)fastchan_region_base);
	/* fast channel region address low */
	resp[3] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)(fastchan_region_base >> 32));
	/* fast channel region size low */
	resp[4] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)fastchan_region_size);;
	/* fast channel region size high */
	resp[5] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)(fastchan_region_size >> 32));
	/* doorbell addr low */
	resp[6] = 0;
	/* doorbell addr high */
	resp[7] = 0;
	/* doorbell set mask low */
	resp[8] = 0;
	/* doorbell set mask high */
	resp[9] = 0;
	/* doorbell preserve mask low */
	resp[10] = 0;
	/* doorbell preserve mask high */
	resp[11] = 0;

	resp_dlen = 12 * sizeof(*resp);

done:
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_cppc_sg_get_fast_channel_offset(struct rpmi_service_group *group,
				     struct rpmi_service *service,
				     struct rpmi_transport *trans,
				     rpmi_uint16_t request_datalen,
				     const rpmi_uint8_t *request_data,
				     rpmi_uint16_t *response_datalen,
				     rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t hart_id, hart_index, resp_dlen;
	rpmi_uint64_t fastchan_hart_offset;
	struct rpmi_cppc_group *cppcgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	hart_id = rpmi_to_xe32(trans->is_be,
			((const rpmi_uint32_t *)request_data)[0]);

	/** valid hart id */
	hart_index = rpmi_hsm_hart_id2index(cppcgrp->hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		status = RPMI_ERR_NOTFOUND;
		resp_dlen = sizeof(*resp);
		goto done;
	}

	fastchan_hart_offset = __rpmi_cppc_hart_fc_offset(hart_index);

	status = RPMI_SUCCESS;
	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)fastchan_hart_offset);
	resp[2] = rpmi_to_xe32(trans->is_be,
			(rpmi_uint32_t)(fastchan_hart_offset >> 32));
	resp_dlen = 3 * sizeof(*resp);

done:
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_cppc_sg_get_hart_list(struct rpmi_service_group *group,
			   struct rpmi_service *service,
			   struct rpmi_transport *trans,
			   rpmi_uint16_t request_datalen,
			   const rpmi_uint8_t *request_data,
			   rpmi_uint16_t *response_datalen,
			   rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t start_index, max_entries, hart_count, hart_id;
	struct rpmi_cppc_group *cppcgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;
	rpmi_uint32_t returned, remaining, i;

	hart_count = rpmi_hsm_hart_count(cppcgrp->hsm);
	max_entries = RPMI_MSG_DATA_SIZE(trans->slot_size) - (3 * sizeof(*resp));
	max_entries = rpmi_env_div32(max_entries, sizeof(*resp));

	start_index = rpmi_to_xe32(trans->is_be, ((const rpmi_uint32_t *)request_data)[0]);

	if (start_index <= hart_count) {
		returned = max_entries < (hart_count - start_index) ?
			max_entries : (hart_count - start_index);
		for (i = 0; i < returned; i++) {
			hart_id = rpmi_hsm_hart_index2id(cppcgrp->hsm, start_index + i);
			resp[3 + i] = rpmi_to_xe32(trans->is_be, hart_id);
		}
		remaining = hart_count - (start_index + returned);
		status = RPMI_SUCCESS;
	} else {
		returned = 0;
		remaining = hart_count;
		status = RPMI_ERR_INVAL;
	}

	*response_datalen = (returned + 3) * sizeof(*resp);

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
	resp[1] = rpmi_to_xe32(trans->is_be, remaining);
	resp[2] = rpmi_to_xe32(trans->is_be, returned);

	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_cppc_services[RPMI_CPPC_SRV_ID_MAX] = {
	[RPMI_CLK_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_CLK_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_CPPC_SRV_PROBE_REG] = {
		.service_id = RPMI_CPPC_SRV_PROBE_REG,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_cppc_sg_probe_reg,
	},
	[RPMI_CPPC_SRV_READ_REG] = {
		.service_id = RPMI_CPPC_SRV_READ_REG,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_cppc_sg_read_reg,
	},
	[RPMI_CPPC_SRV_WRITE_REG] = {
		.service_id = RPMI_CPPC_SRV_WRITE_REG,
		.min_a2p_request_datalen = 16,
		.process_a2p_request = rpmi_cppc_sg_write_reg,
	},
	[RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION] = {
		.service_id = RPMI_CPPC_SRV_GET_FAST_CHANNEL_REGION,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_cppc_sg_get_fast_channel_region,
	},
	[RPMI_CPPC_SRV_GET_FAST_CHANNEL_OFFSET] = {
		.service_id = RPMI_CPPC_SRV_GET_FAST_CHANNEL_OFFSET,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_cppc_sg_get_fast_channel_offset,
	},
	[RPMI_CPPC_SRV_GET_HART_LIST] = {
		.service_id = RPMI_CPPC_SRV_GET_HART_LIST,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_cppc_sg_get_hart_list,
	},
};

static enum rpmi_error rpmi_cppc_process_events(struct rpmi_service_group *group)
{

	enum rpmi_error status = RPMI_SUCCESS;
	rpmi_uint32_t hart_idx, desired_perf;
	union rpmi_cppc_hart_fastchan *fc_hart;
	struct rpmi_cppc_group *cppcgrp = group->priv;

	for (hart_idx = 0; hart_idx < cppcgrp->hart_count; hart_idx++) {
		fc_hart = &cppcgrp->fastchan->hart_fastchan[hart_idx];
		desired_perf = __rpmi_cppc_get_fc_desired_perf(cppcgrp, hart_idx);

		if (fc_hart->passive.desired_perf != desired_perf) {
			fc_hart->passive.desired_perf = desired_perf;
			status = cppcgrp->ops->cppc_update_perf(cppcgrp->ops_priv,
					   desired_perf,
					   hart_idx);
		}
	}

	return status;
}

static struct rpmi_cppc_fastchan *
rpmi_cppc_fastchan_create(rpmi_uint32_t hart_count,
			  struct rpmi_shmem *shmem_fastchan)
{
	struct rpmi_cppc_fastchan *cppc_fastchan;
	union rpmi_cppc_hart_fastchan *hart_fastchan_array;
	rpmi_uint32_t size = rpmi_shmem_size(shmem_fastchan);

	/** size if not 0 and its not power of 2 */
	if (!size && (size & (size - 1))) {
		DPRINTF("%s: CPPC fastchan shmem size not power-of-2\n",
		__func__);
		return NULL;
	}

	/** size must accomodate fast channel entries for all harts */
	if (size < (hart_count * sizeof(union rpmi_cppc_hart_fastchan))) {
		DPRINTF("%s: CPPC fastchan shmem size less than required\n",
		__func__);
		return NULL;
	}

	/** CPPC fast channel shared memory aligned to a hart fast channel entry */
	if (size & (sizeof(union rpmi_cppc_hart_fastchan) - 1))
		return NULL;

	if (rpmi_shmem_fill(shmem_fastchan, 0, 0, size))
		return NULL;

	cppc_fastchan = rpmi_env_zalloc(sizeof(*cppc_fastchan));
	if (!cppc_fastchan) {
		DPRINTF("%s: failed to allocate cppc fastchannel instance\n",
		__func__);
		return NULL;
	}

	hart_fastchan_array =
		rpmi_env_zalloc(hart_count * sizeof(*hart_fastchan_array));
	if (!hart_fastchan_array) {
		DPRINTF("%s: failed to allocate fastchannel array for harts\n",
		__func__);
		rpmi_env_free(cppc_fastchan);
		return NULL;
	}

	cppc_fastchan->hart_fastchan = hart_fastchan_array;
	cppc_fastchan->shmem = shmem_fastchan;

	return cppc_fastchan;
}

struct rpmi_service_group *
rpmi_service_group_cppc_create(struct rpmi_hsm *hsm,
			       const struct rpmi_cppc_regs *cppc_regs,
			       enum rpmi_cppc_mode mode,
			       struct rpmi_shmem *shmem_fastchan,
			       const struct rpmi_cppc_platform_ops *ops,
			       void *ops_priv)
{
	struct rpmi_cppc_group *cppcgrp;
	struct rpmi_service_group *group;
	struct rpmi_cppc_fastchan *cppc_fastchan;
	rpmi_uint32_t hart_count;

	if (!hsm || !cppc_regs || !shmem_fastchan) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	if (mode != RPMI_CPPC_PASSIVE_MODE) {
		DPRINTF("%s: cppc mode not supported\n", __func__);
		return NULL;
	}

	/** allocate cppc group instance memory */
	cppcgrp = rpmi_env_zalloc(sizeof(*cppcgrp));
	if (!cppcgrp) {
		DPRINTF("%s: failed to allocate cppc service group instance\n",
		__func__);
		return NULL;
	}

	hart_count = rpmi_hsm_hart_count(hsm);
	if (!hart_count) {
		DPRINTF("%s: hart count is 0, failed to create cppc group\n",
		__func__);
		rpmi_env_free(cppcgrp);
		return NULL;
	}

	cppc_fastchan = rpmi_cppc_fastchan_create(hart_count, shmem_fastchan);
	if (!cppc_fastchan) {
		DPRINTF("%s: failed to create cppc fastchannel\n", __func__);
		rpmi_env_free(cppcgrp);
		return NULL;
	}

	/**
	 * cppc_regs_pdata is the set of CPPC registers value which
	 * are supported by the platform. Values are static or
	 * going to change in runtime by the platform.
	 * All harts share the same CPPC register values which
	 * includes the same CPPC performance level scale too.
	 * All harts can be in independent performance domain or
	 * groups of harts(cluster level) can be in separate domains
	 * depending on the performance controls which is platform
	 * specific.
	 *
	 * Domains are supposed to be managed by the platform.
	 * CPPC performance level to internal hardware operating point
	 * conversion is done by platform only.
	 */
	cppcgrp->regs = cppc_regs;
	cppcgrp->fastchan = cppc_fastchan;
	cppcgrp->hart_count = hart_count;
	cppcgrp->hsm = hsm;
	cppcgrp->ops = ops;
	cppcgrp->ops_priv = ops_priv;

	/** initialize rpmi service group instance */
	group = &cppcgrp->group;
	group->name = "cppc";
	group->servicegroup_id = RPMI_SRVGRP_CPPC;
	group->max_service_id = RPMI_CPPC_SRV_ID_MAX;
	group->services = rpmi_cppc_services;
	group->process_events = rpmi_cppc_process_events;
	group->lock = rpmi_env_alloc_lock();
	group->priv = cppcgrp;

	return group;
}

void rpmi_service_group_cppc_destroy(struct rpmi_service_group *group)
{
	struct rpmi_cppc_group *cppcgrp;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	cppcgrp = group->priv;
	rpmi_env_free(cppcgrp->fastchan->hart_fastchan);
	rpmi_env_free(cppcgrp->fastchan);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}
