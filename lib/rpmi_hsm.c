/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>

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
	/** Whether HSM instance is non-leaf (or hierarchical) instance */
	rpmi_bool_t is_non_leaf;

	union {
		/** Details required by leaf instance */
		struct {
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
		} leaf;

		/** Details required by non-leaf instance */
		struct {
			/** Number of child instances */
			rpmi_uint32_t child_count;

			/** Array of child instance pointers */
			struct rpmi_hsm **child_array;
		} nonleaf;
	};
};

rpmi_uint32_t rpmi_hsm_hart_count(struct rpmi_hsm *hsm)
{
	struct rpmi_hsm *child_hsm;
	rpmi_uint32_t i, ret;

	if (!hsm)
		return 0;

	if (!hsm->is_non_leaf)
		return hsm->leaf.hart_count;

	ret = 0;
	for (i = 0; i < hsm->nonleaf.child_count; i++) {
		child_hsm = hsm->nonleaf.child_array[i];
		ret += rpmi_hsm_hart_count(child_hsm);
	}

	return ret;
}

static struct rpmi_hsm *rpmi_hsm_hart_index2child(struct rpmi_hsm *hsm,
						  rpmi_uint32_t hart_index,
						  rpmi_uint32_t *child_rel_index_ptr)
{
	rpmi_uint32_t i, count, child_rel_index;
	struct rpmi_hsm *child_hsm;

	if (!hsm->is_non_leaf)
		return NULL;

	child_rel_index = 0;
	for (i = 0; i < hsm->nonleaf.child_count; i++) {
		child_hsm = hsm->nonleaf.child_array[i];

		count = rpmi_hsm_hart_count(child_hsm);
		if (child_rel_index <= hart_index &&
		    hart_index < (child_rel_index + count)) {
			if (child_rel_index_ptr)
				*child_rel_index_ptr = child_rel_index;
			return child_hsm;
		}

		child_rel_index += count;
	}

	return NULL;
}

rpmi_uint32_t rpmi_hsm_hart_index2id(struct rpmi_hsm *hsm, rpmi_uint32_t hart_index)
{
	rpmi_uint32_t child_rel_index;
	struct rpmi_hsm *child_hsm;

	if (!hsm || rpmi_hsm_hart_count(hsm) <= hart_index)
		return LIBRPMI_HSM_INVALID_HART_ID;

	if (!hsm->is_non_leaf)
		return hsm->leaf.hart_ids[hart_index];

	child_hsm = rpmi_hsm_hart_index2child(hsm, hart_index, &child_rel_index);
	if (child_hsm)
		return rpmi_hsm_hart_index2id(child_hsm, hart_index - child_rel_index);

	return LIBRPMI_HSM_INVALID_HART_ID;
}

rpmi_uint32_t rpmi_hsm_hart_id2index(struct rpmi_hsm *hsm, rpmi_uint32_t hart_id)
{
	rpmi_uint32_t i, hart_count;

	if (!hsm)
		return LIBRPMI_HSM_INVALID_HART_INDEX;

	hart_count = rpmi_hsm_hart_count(hsm);
	for (i = 0; i < hart_count; i++) {
		if (rpmi_hsm_hart_index2id(hsm, i) == hart_id)
			return i;
	}

	return LIBRPMI_HSM_INVALID_HART_INDEX;
}

rpmi_uint32_t rpmi_hsm_get_suspend_type_count(struct rpmi_hsm *hsm)
{
	if (!hsm)
		return 0;

	if (!hsm->is_non_leaf)
		return hsm->leaf.suspend_type_count;

	/* For non-leaf, return the suspend type count of first child */
	return rpmi_hsm_get_suspend_type_count(hsm->nonleaf.child_array[0]);
}

const struct rpmi_hsm_suspend_type *rpmi_hsm_get_suspend_type(struct rpmi_hsm *hsm,
							rpmi_uint32_t suspend_type_index)
{
	if (!hsm || rpmi_hsm_get_suspend_type_count(hsm) <= suspend_type_index)
		return NULL;

	if (!hsm->is_non_leaf)
		return &hsm->leaf.suspend_types[suspend_type_index];

	/* For non-leaf, return the suspend type of first child */
	return rpmi_hsm_get_suspend_type(hsm->nonleaf.child_array[0], suspend_type_index);
}

const struct rpmi_hsm_suspend_type *rpmi_hsm_find_suspend_type(struct rpmi_hsm *hsm,
							       rpmi_uint32_t type)
{
	const struct rpmi_hsm_suspend_type *suspend_type;
	rpmi_uint32_t i, suspend_type_count;

	if (!hsm)
		return NULL;

	suspend_type_count = rpmi_hsm_get_suspend_type_count(hsm);
	for (i = 0; i < suspend_type_count; i++) {
		suspend_type = rpmi_hsm_get_suspend_type(hsm, i);
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

	if (hsm->is_non_leaf)
		return;

	hw_state = hsm->leaf.ops->hart_get_hw_state(hsm->leaf.ops_priv, hart_index);
	if ((rpmi_int32_t)hart->state < 0) {
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
				hsm->leaf.ops->hart_start_finalize(hsm->leaf.ops_priv,
								   hart_index,
								   hart->start_addr);
				hart->state = RPMI_HSM_HART_STATE_STARTED;
			}
			break;
		case RPMI_HSM_HART_STATE_STOP_PENDING:
			if (hw_state == RPMI_HART_HW_STATE_SUSPENDED ||
			    hw_state == RPMI_HART_HW_STATE_STOPPED) {
				hsm->leaf.ops->hart_stop_finalize(hsm->leaf.ops_priv,
								  hart_index);
				hart->state = RPMI_HSM_HART_STATE_STOPPED;
			}
			break;
		case RPMI_HSM_HART_STATE_SUSPEND_PENDING:
			if (hw_state == RPMI_HART_HW_STATE_SUSPENDED) {
				hsm->leaf.ops->hart_suspend_finalize(hsm->leaf.ops_priv,
								     hart_index,
								     hart->suspend_type,
								     hart->resume_addr);
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
	struct rpmi_hsm *child_hsm;
	rpmi_uint32_t hart_index;
	enum rpmi_error ret;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	hart_index = rpmi_hsm_hart_id2index(hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		DPRINTF("%s: invalid hart_id 0x%x\n", __func__, hart_id);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (hsm->is_non_leaf) {
		child_hsm = rpmi_hsm_hart_index2child(hsm, hart_index, NULL);
		return rpmi_hsm_hart_start(child_hsm, hart_id, start_addr);
	}

	if (!hsm->leaf.ops->hart_start_prepare || !hsm->leaf.ops->hart_start_finalize) {
		DPRINTF("%s: not supported\n", __func__);
		return RPMI_ERR_NOTSUPP;
	}

	hart = &hsm->leaf.harts[hart_index];
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

	ret = hsm->leaf.ops->hart_start_prepare(hsm->leaf.ops_priv, hart_index, start_addr);
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
	struct rpmi_hsm *child_hsm;
	rpmi_uint32_t hart_index;
	enum rpmi_error ret;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	hart_index = rpmi_hsm_hart_id2index(hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		DPRINTF("%s: invalid hart_id 0x%x\n", __func__, hart_id);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (hsm->is_non_leaf) {
		child_hsm = rpmi_hsm_hart_index2child(hsm, hart_index, NULL);
		return rpmi_hsm_hart_stop(child_hsm, hart_id);
	}

	if (!hsm->leaf.ops->hart_stop_prepare || !hsm->leaf.ops->hart_stop_finalize) {
		DPRINTF("%s: not supported\n", __func__);
		return RPMI_ERR_NOTSUPP;
	}

	hart = &hsm->leaf.harts[hart_index];
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

	ret = hsm->leaf.ops->hart_stop_prepare(hsm->leaf.ops_priv, hart_index);
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
	struct rpmi_hsm *child_hsm;
	rpmi_uint32_t hart_index;
	enum rpmi_error ret;

	if (!hsm || !suspend_type) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	hart_index = rpmi_hsm_hart_id2index(hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		DPRINTF("%s: invalid hart_id 0x%x\n", __func__, hart_id);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (hsm->is_non_leaf) {
		child_hsm = rpmi_hsm_hart_index2child(hsm, hart_index, NULL);
		return rpmi_hsm_hart_suspend(child_hsm, hart_id, suspend_type,
					     resume_addr);
	}

	if (!hsm->leaf.ops->hart_suspend_prepare || !hsm->leaf.ops->hart_suspend_finalize) {
		DPRINTF("%s: not supported\n", __func__);
		return RPMI_ERR_NOTSUPP;
	}

	hart = &hsm->leaf.harts[hart_index];
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

	ret = hsm->leaf.ops->hart_suspend_prepare(hsm->leaf.ops_priv, hart_index,
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
	struct rpmi_hsm *child_hsm;
	struct rpmi_hsm_hart *hart;
	rpmi_uint32_t hart_index;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	hart_index = rpmi_hsm_hart_id2index(hsm, hart_id);
	if (hart_index == LIBRPMI_HSM_INVALID_HART_INDEX) {
		DPRINTF("%s: invalid hart_id 0x%x\n", __func__, hart_id);
		return RPMI_ERR_INVALID_PARAM;
	}

	if (hsm->is_non_leaf) {
		child_hsm = rpmi_hsm_hart_index2child(hsm, hart_index, NULL);
		return rpmi_hsm_get_hart_state(child_hsm, hart_id);
	}

	hart = &hsm->leaf.harts[hart_index];
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

	if (hsm->is_non_leaf) {
		for (i = 0; i < hsm->nonleaf.child_count; i++)
			rpmi_hsm_process_state_changes(hsm->nonleaf.child_array[i]);
	} else {
		for (i = 0; i < hsm->leaf.hart_count; i++) {
			hart = &hsm->leaf.harts[i];
			rpmi_env_lock(hart->lock);
			__rpmi_hsm_process_hart_state_changes(hsm, hart, i);
			rpmi_env_unlock(hart->lock);
		}
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

	hsm->leaf.hart_count = hart_count;
	hsm->leaf.hart_ids = hart_ids;

	hsm->leaf.harts = rpmi_env_zalloc(hsm->leaf.hart_count * sizeof(*hsm->leaf.harts));
	if (!hsm->leaf.harts) {
		DPRINTF("%s: failed to allocate hart array\n", __func__);
		rpmi_env_free(hsm);
		return NULL;
	}

	for (i = 0; i < hsm->leaf.hart_count; i++) {
		hsm->leaf.harts[i].lock = rpmi_env_alloc_lock();
		hsm->leaf.harts[i].state = -1;
	}

	hsm->leaf.suspend_type_count = suspend_type_count;
	hsm->leaf.suspend_types = suspend_types;
	hsm->leaf.ops = ops;
	hsm->leaf.ops_priv = ops_priv;

	rpmi_hsm_process_state_changes(hsm);

	return hsm;
}

struct rpmi_hsm *rpmi_hsm_nonleaf_create(rpmi_uint32_t child_count,
					 struct rpmi_hsm **child_array)
{
	const struct rpmi_hsm_suspend_type *suspend_type, *ref_suspend_type;
	rpmi_uint32_t i, j, suspend_type_count = 0;
	struct rpmi_hsm *hsm, *child_hsm;

	/* Critical parameters should be non-zero */
	if (!child_count || !child_array) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Sanity check on child instances */
	for (i = 0; i < child_count; i++) {
		child_hsm = child_array[i];
		if (!child_hsm) {
			DPRINTF("%s: child%d is NULL\n", __func__, i);
			return NULL;
		}

		if (!i) {
			suspend_type_count = rpmi_hsm_get_suspend_type_count(child_hsm);
			continue;
		}

		if (rpmi_hsm_get_suspend_type_count(child_hsm) != suspend_type_count) {
			DPRINTF("%s: suspend type count of child%d does not match child0\n",
				__func__, i);
			return NULL;
		}

		for (j = 0; j < suspend_type_count; j++) {
			ref_suspend_type = rpmi_hsm_get_suspend_type(child_array[0], j);
			suspend_type = rpmi_hsm_get_suspend_type(child_hsm, j);
			if (!ref_suspend_type || !suspend_type) {
				DPRINTF("%s: suspend type %d of child%d or child0 is NULL\n",
					__func__, j, i);
				return NULL;
			}

			if (ref_suspend_type->type != suspend_type->type) {
				DPRINTF("%s: type value of child%d suspend type %d"
					" does not match child0\n", __func__, i, j);
				return NULL;
			}

			if (ref_suspend_type->info.flags != suspend_type->info.flags) {
				DPRINTF("%s: flags of child%d suspend type %d"
					" does not match child0\n", __func__, i, j);
				return NULL;
			}

			if (ref_suspend_type->info.entry_latency_us !=
			    suspend_type->info.entry_latency_us) {
				DPRINTF("%s: entry_latency_us of child%d suspend type %d"
					" does not match child0\n", __func__, i, j);
				return NULL;
			}

			if (ref_suspend_type->info.exit_latency_us !=
			    suspend_type->info.exit_latency_us) {
				DPRINTF("%s: exit_latency_us of child%d suspend type %d"
					" does not match child0\n", __func__, i, j);
				return NULL;
			}

			if (ref_suspend_type->info.wakeup_latency_us !=
			    suspend_type->info.wakeup_latency_us) {
				DPRINTF("%s: wakeup_latency_us of child%d suspend type %d"
					" does not match child0\n", __func__, i, j);
				return NULL;
			}

			if (ref_suspend_type->info.min_residency_us !=
			    suspend_type->info.min_residency_us) {
				DPRINTF("%s: min_residency_us of child%d suspend type %d"
					" does not match child0\n", __func__, i, j);
				return NULL;
			}
		}
	}

	/* Allocate HSM */
	hsm = rpmi_env_zalloc(sizeof(*hsm));
	if (!hsm) {
		DPRINTF("%s: failed to allocate HSM instance\n", __func__);
		return NULL;
	}

	hsm->is_non_leaf = true;
	hsm->nonleaf.child_count = child_count;
	hsm->nonleaf.child_array = child_array;

	return hsm;
}

void rpmi_hsm_destroy(struct rpmi_hsm *hsm)
{
	rpmi_uint32_t i;

	if (!hsm) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	if (!hsm->is_non_leaf) {
		for (i = 0; i < hsm->leaf.hart_count; i++)
			rpmi_env_free_lock(hsm->leaf.harts[i].lock);
		rpmi_env_free(hsm->leaf.harts);
	}

	rpmi_env_free(hsm);
}
