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

struct rpmi_hsm_hart {
	/** Lock to protect this structure and perform platform operations */
	void *lock;

	/** Current HSM hart state */
	enum rpmi_hsm_hart_state state;

	/** Current hart start parameter */
	rpmi_uint64_t start_addr;

	/** Current hart suspend parameter */
	const struct rpmi_hsm_suspend_type *suspend_type;
	rpmi_uint64_t resume_addr;
};

struct rpmi_hsm {
	/** Number of harts */
	rpmi_uint32_t hart_count;

	/** Array of hart IDs */
	const rpmi_uint32_t *hart_ids;

	/** Array of harts */
	struct rpmi_hsm_hart *harts;

	/** Number of suspend types */
	rpmi_uint32_t suspend_type_count;

	/** Array of suspend types */
	const struct rpmi_hsm_suspend_type *suspend_types;

	/**
	 * Platform HSM operations
	 *
	 * Note: These operations are called with harts[i]->lock held
	 */
	const struct rpmi_hsm_platform_ops *ops;

	/** Private data of platform HSM operations */
	void *ops_priv;
};

rpmi_uint32_t rpmi_hsm_hart_count(struct rpmi_hsm *hsm)
{
	return hsm ? hsm->hart_count : 0;
}

rpmi_uint32_t rpmi_hsm_hart_index2id(struct rpmi_hsm *hsm, rpmi_uint32_t hart_index)
{
	if (!hsm || hsm->hart_count <= hart_index)
		return LIBRPMI_HSM_INVALID_HART_ID;

	return hsm->hart_ids[hart_index];
}

rpmi_uint32_t rpmi_hsm_hart_id2index(struct rpmi_hsm *hsm, rpmi_uint32_t hart_id)
{
	rpmi_uint32_t i;

	if (!hsm)
		return LIBRPMI_HSM_INVALID_HART_INDEX;

	for (i = 0; i < hsm->hart_count; i++) {
		if (hsm->hart_ids[i] == hart_id)
			return i;
	}

	return LIBRPMI_HSM_INVALID_HART_INDEX;
}

rpmi_uint32_t rpmi_hsm_get_suspend_type_count(struct rpmi_hsm *hsm)
{
	return hsm ? hsm->suspend_type_count : 0;
}

const struct rpmi_hsm_suspend_type *rpmi_hsm_get_suspend_type(struct rpmi_hsm *hsm,
							rpmi_uint32_t suspend_type_index)
{
	if (!hsm || hsm->suspend_type_count <= suspend_type_index)
		return NULL;

	return &hsm->suspend_types[suspend_type_index];
}

const struct rpmi_hsm_suspend_type *rpmi_hsm_find_suspend_type(struct rpmi_hsm *hsm,
							       rpmi_uint32_t type)
{
	const struct rpmi_hsm_suspend_type *suspend_type;
	rpmi_uint32_t i;

	if (!hsm)
		return NULL;

	for (i = 0; i < hsm->suspend_type_count; i++) {
		suspend_type = &hsm->suspend_types[i];
		if (suspend_type->type == type)
			return suspend_type;
	}

	return NULL;
}

static void __rpmi_hsm_process_hart_state_changes(struct rpmi_hsm *hsm,
						  struct rpmi_hsm_hart *hart,
						  rpmi_uint32_t hart_index)
{
	enum rpmi_hart_hw_state hw_state;

	hw_state = hsm->ops->hart_get_hw_state(hsm->ops_priv, hart_index);
	if (hart->state < 0) {
		switch (hw_state) {
		case RPMI_HART_HW_STATE_STARTED:
			hart->state = RPMI_HSM_HART_STATE_STARTED;
			break;
		case RPMI_HART_HW_STATE_SUSPENDED:
			hart->state = RPMI_HSM_HART_STATE_SUSPENDED;
			break;
		case RPMI_HART_HW_STATE_STOPPED:
		default:
			hart->state = RPMI_HSM_HART_STATE_STOPPED;
			break;
		}
	} else {
		switch (hart->state) {
		case RPMI_HSM_HART_STATE_START_PENDING:
			if (hw_state == RPMI_HART_HW_STATE_STARTED) {
				hsm->ops->hart_start_finalize(hsm->ops_priv, hart_index,
							      hart->start_addr);
				hart->state = RPMI_HSM_HART_STATE_STARTED;
			}
			break;
		case RPMI_HSM_HART_STATE_STOP_PENDING:
			if (hw_state == RPMI_HART_HW_STATE_SUSPENDED ||
			    hw_state == RPMI_HART_HW_STATE_STOPPED) {
				hsm->ops->hart_stop_finalize(hsm->ops_priv, hart_index);
				hart->state = RPMI_HSM_HART_STATE_STOPPED;
			}
			break;
		case RPMI_HSM_HART_STATE_SUSPEND_PENDING:
			if (hw_state == RPMI_HART_HW_STATE_SUSPENDED) {
				hsm->ops->hart_suspend_finalize(hsm->ops_priv, hart_index,
						hart->suspend_type, hart->resume_addr);
				hart->state = RPMI_HSM_HART_STATE_SUSPENDED;
			}
			break;
		case RPMI_HSM_HART_STATE_SUSPENDED:
			if (hw_state == RPMI_HART_HW_STATE_STARTED)
				hart->state = RPMI_HSM_HART_STATE_STARTED;
			break;
		default:
			break;
		}
	}
}

enum rpmi_error rpmi_hsm_hart_start(struct rpmi_hsm *hsm, rpmi_uint32_t hart_id,
				    rpmi_uint64_t start_addr)
{
	struct rpmi_hsm_hart *hart;
	rpmi_uint32_t hart_index;
	enum rpmi_error ret;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVAL;
	}
	if (!hsm->ops->hart_start_prepare || !hsm->ops->hart_start_finalize) {
		DPRINTF("%s: not supported\n", __func__);
		return RPMI_ERR_NOTSUPP;
	}

	hart_index = rpmi_hsm_hart_id2index(hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		DPRINTF("%s: invalid hart_id 0x%x\n", __func__, hart_id);
		return RPMI_ERR_INVAL;
	}

	hart = &hsm->harts[hart_index];
	rpmi_env_lock(hart->lock);

	if (hart->state == RPMI_HSM_HART_STATE_STARTED) {
		DPRINTF("%s: hart_id 0x%x already started\n", __func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_ALREADY;
	}
	if (hart->state == RPMI_HSM_HART_STATE_START_PENDING) {
		DPRINTF("%s: hart_id 0x%x already in-progress\n",
			__func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_ALREADY;
	}
	if (hart->state != RPMI_HSM_HART_STATE_STOPPED) {
		DPRINTF("%s: denied due to invalid state for hart_id 0x%x\n",
			__func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_DENIED;
	}

	ret = hsm->ops->hart_start_prepare(hsm->ops_priv, hart_index, start_addr);
	if (ret) {
		DPRINTF("%s: hart_id 0x%x prepare failed\n", __func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return ret;
	}

	hart->start_addr = start_addr;
	hart->state = RPMI_HSM_HART_STATE_START_PENDING;
	__rpmi_hsm_process_hart_state_changes(hsm, hart, hart_index);

	rpmi_env_unlock(hart->lock);
	return RPMI_SUCCESS;
}

enum rpmi_error rpmi_hsm_hart_stop(struct rpmi_hsm *hsm, rpmi_uint32_t hart_id)
{
	struct rpmi_hsm_hart *hart;
	rpmi_uint32_t hart_index;
	enum rpmi_error ret;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVAL;
	}
	if (!hsm->ops->hart_stop_prepare || !hsm->ops->hart_stop_finalize) {
		DPRINTF("%s: not supported\n", __func__);
		return RPMI_ERR_NOTSUPP;
	}

	hart_index = rpmi_hsm_hart_id2index(hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		DPRINTF("%s: invalid hart_id 0x%x\n", __func__, hart_id);
		return RPMI_ERR_INVAL;
	}

	hart = &hsm->harts[hart_index];
	rpmi_env_lock(hart->lock);

	if (hart->state == RPMI_HSM_HART_STATE_STOPPED) {
		DPRINTF("%s: hart_id 0x%x already stopped\n", __func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_ALREADY;
	}
	if (hart->state == RPMI_HSM_HART_STATE_STOP_PENDING) {
		DPRINTF("%s: hart_id 0x%x already in-progress\n",
			__func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_ALREADY;
	}
	if (hart->state != RPMI_HSM_HART_STATE_STARTED) {
		DPRINTF("%s: denied due to invalid state for hart_id 0x%x\n",
			__func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_DENIED;
	}

	ret = hsm->ops->hart_stop_prepare(hsm->ops_priv, hart_index);
	if (ret) {
		DPRINTF("%s: hart_id 0x%x prepare failed\n", __func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return ret;
	}

	hart->state = RPMI_HSM_HART_STATE_STOP_PENDING;
	__rpmi_hsm_process_hart_state_changes(hsm, hart, hart_index);

	rpmi_env_unlock(hart->lock);
	return RPMI_SUCCESS;
}

enum rpmi_error rpmi_hsm_hart_suspend(struct rpmi_hsm *hsm, rpmi_uint32_t hart_id,
				const struct rpmi_hsm_suspend_type *suspend_type,
				rpmi_uint64_t resume_addr)
{
	struct rpmi_hsm_hart *hart;
	rpmi_uint32_t hart_index;
	enum rpmi_error ret;

	if (!hsm || !suspend_type) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVAL;
	}
	if (!hsm->ops->hart_suspend_prepare || !hsm->ops->hart_suspend_finalize) {
		DPRINTF("%s: not supported\n", __func__);
		return RPMI_ERR_NOTSUPP;
	}

	hart_index = rpmi_hsm_hart_id2index(hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		DPRINTF("%s: invalid hart_id 0x%x\n", __func__, hart_id);
		return RPMI_ERR_INVAL;
	}

	hart = &hsm->harts[hart_index];
	rpmi_env_lock(hart->lock);

	if (hart->state == RPMI_HSM_HART_STATE_SUSPENDED) {
		DPRINTF("%s: hart_id 0x%x already suspended\n", __func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_ALREADY;
	}
	if (hart->state == RPMI_HSM_HART_STATE_SUSPEND_PENDING) {
		DPRINTF("%s: hart_id 0x%x already in-progress\n",
			__func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_ALREADY;
	}
	if (hart->state != RPMI_HSM_HART_STATE_STARTED) {
		DPRINTF("%s: denied due to invalid state for hart_id 0x%x\n",
			__func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return RPMI_ERR_DENIED;
	}

	ret = hsm->ops->hart_suspend_prepare(hsm->ops_priv, hart_index,
					     suspend_type, resume_addr);
	if (ret) {
		DPRINTF("%s: hart_id 0x%x prepare failed\n", __func__, hart_id);
		rpmi_env_unlock(hart->lock);
		return ret;
	}

	hart->suspend_type = suspend_type;
	hart->resume_addr = resume_addr;
	hart->state = RPMI_HSM_HART_STATE_SUSPEND_PENDING;
	__rpmi_hsm_process_hart_state_changes(hsm, hart, hart_index);

	rpmi_env_unlock(hart->lock);
	return RPMI_SUCCESS;
}

int rpmi_hsm_get_hart_state(struct rpmi_hsm *hsm, rpmi_uint32_t hart_id)
{
	enum rpmi_hsm_hart_state state;
	struct rpmi_hsm_hart *hart;
	rpmi_uint32_t hart_index;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVAL;
	}

	hart_index = rpmi_hsm_hart_id2index(hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		DPRINTF("%s: invalid hart_id 0x%x\n", __func__, hart_id);
		return RPMI_ERR_INVAL;
	}

	hart = &hsm->harts[hart_index];
	rpmi_env_lock(hart->lock);
	state = hart->state;
	rpmi_env_unlock(hart->lock);

	return state;
}

void rpmi_hsm_process_state_changes(struct rpmi_hsm *hsm)
{
	struct rpmi_hsm_hart *hart;
	rpmi_uint32_t i;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	for (i = 0; i < hsm->hart_count; i++) {
		hart = &hsm->harts[i];
		rpmi_env_lock(hart->lock);
		__rpmi_hsm_process_hart_state_changes(hsm, hart, i);
		rpmi_env_unlock(hart->lock);
	}
}

struct rpmi_hsm *rpmi_hsm_create(rpmi_uint32_t hart_count,
				 const rpmi_uint32_t *hart_ids,
				 rpmi_uint32_t suspend_type_count,
				 const struct rpmi_hsm_suspend_type *suspend_types,
				 const struct rpmi_hsm_platform_ops *ops,
				 void *ops_priv)
{
	struct rpmi_hsm *hsm;
	rpmi_uint32_t i;

	/* Critical parameters should be non-zero */
	if (!hart_count || !hart_ids || !ops || !ops->hart_get_hw_state) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Suspend types array should be non-NULL only for non-zero suspend type count */
	if ((suspend_type_count && !suspend_types) ||
	    (!suspend_type_count && suspend_types)) {
		DPRINTF("%s: invalid suspend parameters\n", __func__);
		return NULL;
	}

	/* Allocate HSM */
	hsm = rpmi_env_zalloc(sizeof(*hsm));
	if (!hsm) {
		DPRINTF("%s: failed to allocate HSM instance\n", __func__);
		return NULL;
	}

	hsm->hart_count = hart_count;
	hsm->hart_ids = hart_ids;

	hsm->harts = rpmi_env_zalloc(hsm->hart_count * sizeof(*hsm->harts));
	if (!hsm->harts) {
		DPRINTF("%s: failed to allocate hart array\n", __func__);
		rpmi_env_free(hsm);
		return NULL;
	}

	for (i = 0; i < hsm->hart_count; i++) {
		hsm->harts[i].lock = rpmi_env_alloc_lock();
		hsm->harts[i].state = -1;
	}

	hsm->suspend_type_count = suspend_type_count;
	hsm->suspend_types = suspend_types;
	hsm->ops = ops;
	hsm->ops_priv = ops_priv;

	rpmi_hsm_process_state_changes(hsm);

	return hsm;
}

void rpmi_hsm_destroy(struct rpmi_hsm *hsm)
{
	rpmi_uint32_t i;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	for (i = 0; i < hsm->hart_count; i++)
		rpmi_env_free_lock(hsm->harts[i].lock);
	rpmi_env_free_lock(hsm->harts);
	rpmi_env_free(hsm);
}
