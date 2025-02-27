/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "librpmi_internal_list.h"

#ifdef DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define RPMI_CLOCK_RATE_INVALID		(-1ULL)
/** Clock name max length including null char */
#define RPMI_CLK_NAME_MAX_LEN		16

/** Convert Rate(HI_32bit, LO_32bit) -> Rate(U64) */
#define RATE_U64(lo, hi)		((rpmi_uint64_t)hi << 32 | lo)
/** Get HI_32bit from Rate(U64) */
#define RATE_U64TOHI(r)		((rpmi_uint32_t)(r >> 32))
/** Get LO_32bit from Rate(U64) */
#define RATE_U64TOLO(r)		((rpmi_uint32_t)r)

/** Convert list node pointer to struct rpmi_clock instance pointer */
#define to_rpmi_clock(__node)	\
	container_of((__node), struct rpmi_clock, node)

/* A clock instance */
struct rpmi_clock {
	/* Clock node */
	struct rpmi_dlist node;
	/* Lock to invoke the platform operations to
	 * protect this structure */
	void *lock;
	/* Clock ID */
	rpmi_uint32_t id;
	/* Clock enable count on behalf of child clocks */
	rpmi_uint32_t enable_count;
	/* Current clock state */
	enum rpmi_clock_state current_state;
	/* Parent clock instance pointer */
	struct rpmi_clock *parent;
	/* Child clock count */
	rpmi_uint32_t child_count;
	/* Clock static attributes/data */
	const struct rpmi_clock_data *cdata;
	/* Child clock list */
	struct rpmi_dlist child_clock;
};

/** RPMI Clock Service Group instance */
struct rpmi_clock_group {
	/* Total clock count */
	rpmi_uint32_t clock_count;
	/* Pointer to clock tree */
	struct rpmi_clock *clock_tree;
	/* Common clock platform operations (called with holding the lock)*/
	const struct rpmi_clock_platform_ops *ops;
	/* Private data of platform clock operations */
	void *ops_priv;
	struct rpmi_service_group group;
};

/** Get a struct rpmi_clock instance pointer from clock id */
static inline struct rpmi_clock *
rpmi_get_clock(struct rpmi_clock_group *clock_group, rpmi_uint32_t clock_id)
{
	return &clock_group->clock_tree[clock_id];
}

static enum rpmi_error rpmi_clock_get_attrs(struct rpmi_clock_group *clkgrp,
					    rpmi_uint32_t clkid,
					    struct rpmi_clock_attrs *attrs)
{
	struct rpmi_clock *clk;

	if (!attrs) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	clk = rpmi_get_clock(clkgrp, clkid);
	if (!clk) {
		DPRINTF("%s: clock instance with clkid-%u not found\n",
			__func__, clkid);
		return RPMI_ERR_INVALID_PARAM;
	}

	attrs->name = clk->cdata->name;
	attrs->type = clk->cdata->clock_type;
	attrs->rate_count = clk->cdata->rate_count;
	attrs->transition_latency = clk->cdata->transition_latency_ms;
	attrs->rate_array = clk->cdata->clock_rate_array;

	return RPMI_SUCCESS;
}

/**
 * Take the parent node of a clock subtree and update the
 * rates of all available child clocks based on the
 * new parent clock rate.
 */
static enum rpmi_error
__rpmi_clock_update_rate_tree(struct rpmi_clock_group *clkgrp,
			      struct rpmi_clock *parent,
			      enum rpmi_clock_rate_match match,
			      rpmi_uint64_t parent_rate)
{
	enum rpmi_error ret;
	struct rpmi_dlist *pos;
	rpmi_uint64_t new_rate;

	if (!parent->child_count)
		return RPMI_SUCCESS;

	/* iterate over all child clocks recursively */
	rpmi_list_for_each(pos, &parent->child_clock) {
		struct rpmi_clock *cc = to_rpmi_clock(pos);
		ret = clkgrp->ops->set_rate_recalc(clkgrp->ops_priv, cc->id,
						parent_rate, &new_rate);
		if (ret) {
			DPRINTF("%s: failed to recalc rate for clock-%u\n",
					__func__, cc->id);
			return ret;
		}

		ret = __rpmi_clock_update_rate_tree(clkgrp, cc,
						    match, new_rate);
		if (ret) {
			DPRINTF("%s: failed to update rate for clk-%u tree\n",
					__func__, cc->id);
			return ret;
		}
	}

	return RPMI_SUCCESS;
}

static enum rpmi_error __rpmi_clock_set_rate(struct rpmi_clock_group *clkgrp,
				    struct rpmi_clock *clk,
				    enum rpmi_clock_rate_match match,
				    rpmi_uint64_t rate)
{
	enum rpmi_error ret;
	rpmi_uint64_t curr_rate;
	rpmi_bool_t rate_change_req;

	if (clk->current_state == RPMI_CLK_STATE_DISABLED)
		return RPMI_ERR_DENIED;

	rate_change_req = clkgrp->ops->rate_change_match(clkgrp->ops_priv,
							clk->id, rate);
	if (!rate_change_req)
		return RPMI_ERR_ALREADY;

	ret = clkgrp->ops->set_rate(clkgrp->ops_priv, clk->id, match,
					rate, &curr_rate);
	if (ret)
		return ret;

	if (clk->child_count) {
		ret = __rpmi_clock_update_rate_tree(clkgrp, clk,
						    match, curr_rate);
		if (ret)
			return ret;
	}

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_clock_set_rate(struct rpmi_clock_group *clkgrp,
				    rpmi_uint32_t clkid,
				    enum rpmi_clock_rate_match match,
				    rpmi_uint64_t rate)
{
	enum rpmi_error ret;

	struct rpmi_clock *clk = rpmi_get_clock(clkgrp, clkid);
	if (!clk) {
		DPRINTF("%s: clock instance with clkid-%u not found\n",
			__func__, clkid);
		return RPMI_ERR_INVALID_PARAM;
	}

	rpmi_env_lock(clk->lock);
	ret = __rpmi_clock_set_rate(clkgrp, clk, match, rate);
	rpmi_env_unlock(clk->lock);

	return ret;
}

static enum rpmi_error __rpmi_clock_set_state(struct rpmi_clock_group *clkgrp,
					      struct rpmi_clock *clk,
					      enum rpmi_clock_state state)
{
	enum rpmi_error ret = 0;
	struct rpmi_dlist *pos;

	/**
	 * To disable a clock:
	 * - Must not be disabled already if its a leaf or independent clock.
	 * - If clock is a parent then all child clocks must be in disabled
	 *   state already.
	 **/
	if (state == RPMI_CLK_STATE_DISABLED) {
		/* If clock already disabled? use the cached state */
		if (clk->current_state == RPMI_CLK_STATE_DISABLED) {
			return RPMI_ERR_ALREADY;
		}
		/* If the clock has no child or its a parent with single enable
		 * count then - disable, update cache and return */
		if (!clk->child_count || clk->enable_count == 1) {
			ret = clkgrp->ops->set_state(clkgrp->ops_priv,
							clk->id, state);
			if (ret)
				return ret;

			clk->current_state = state;
			clk->enable_count -= 1;
			goto done;
		}

		/* Check if all child clocks are disabled, otherwise deny */
		rpmi_list_for_each(pos, &clk->child_clock) {
			struct rpmi_clock *cc = to_rpmi_clock(pos);
			if (cc->current_state == RPMI_CLK_STATE_ENABLED)
				return RPMI_ERR_DENIED;
		}

		ret = clkgrp->ops->set_state(clkgrp->ops_priv, clk->id, state);
		if (ret)
			return ret;

		clk->current_state = state;
		clk->enable_count -= 1;

		/* FIXME: We are only traversing clock sub-tree of requested
		 * clock. Need to traverse the parents to check if the disable
		 * condition is met or not and disable if rules are met */
	}
	/**
	 * To enable a clock:
	 * - Must not be enabled already if its a independent clock
	 * - If a child clock(at any level in clock tree) then all its parent
	 *   must be enabled.
	 */
	else if (state == RPMI_CLK_STATE_ENABLED) {
		/* If clock is already enabled? use the cached state */
		if (clk->current_state == RPMI_CLK_STATE_ENABLED)
			return RPMI_ERR_ALREADY;

		if (!clk->parent) {
			ret = clkgrp->ops->set_state(clkgrp->ops_priv,
							clk->id, state);
			if (ret)
				return ret;

			clk->current_state = state;
			clk->enable_count += 1;
			goto done;
		}

		ret = __rpmi_clock_set_state(clkgrp, clk->parent, state);
		if (ret && ret != RPMI_ERR_ALREADY)
			return ret;

		ret = clkgrp->ops->set_state(clkgrp->ops_priv, clk->id, state);
		if (ret)
			return ret;

		clk->current_state = state;
		clk->enable_count += 1;
	}

done:
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_clock_set_state(struct rpmi_clock_group *clkgrp,
				     rpmi_uint32_t clkid,
				     enum rpmi_clock_state state)
{
	enum rpmi_error ret;
	struct rpmi_clock *clk = rpmi_get_clock(clkgrp, clkid);
	if (!clk)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(clk->lock);
	ret = __rpmi_clock_set_state(clkgrp, clk, state);
	rpmi_env_unlock(clk->lock);

	return ret;
}

static enum rpmi_error rpmi_clock_get_state(struct rpmi_clock_group *clkgrp,
				     rpmi_uint32_t clkid,
				     enum rpmi_clock_state *state)
{
	enum rpmi_error ret;
	struct rpmi_clock *clk = rpmi_get_clock(clkgrp, clkid);

	if (!clk || !state)
		return RPMI_ERR_INVALID_PARAM;

	ret = clkgrp->ops->get_state_and_rate(clkgrp->ops_priv, clk->id, state,
					      NULL);
	return ret;
}

static enum rpmi_error rpmi_clock_get_rate(struct rpmi_clock_group *clkgrp,
				    rpmi_uint32_t clkid,
				    rpmi_uint64_t *rate)
{
	enum rpmi_error ret;
	struct rpmi_clock *clk = rpmi_get_clock(clkgrp, clkid);
	if (!clk || !rate)
		return RPMI_ERR_INVALID_PARAM;

	ret = clkgrp->ops->get_state_and_rate(clkgrp->ops_priv, clk->id, NULL,
					      rate);
	return ret;
}

/**
 * Initialize the clock tree from provided
 * static platform clock data.
 *
 * This function initializes the hierarchical structures
 * to represent the clock association in the platform.
 **/
static struct rpmi_clock *
rpmi_clock_tree_init(rpmi_uint32_t clock_count,
		     const struct rpmi_clock_data *clock_tree_data,
		     const struct rpmi_clock_platform_ops *ops,
		     void *ops_priv)
{
	int ret;
	rpmi_uint32_t clkid;
	rpmi_uint64_t rate;
	enum rpmi_clock_state state;
	struct rpmi_clock *clock;

	struct rpmi_clock *clock_tree =
		rpmi_env_zalloc(sizeof(struct rpmi_clock) * clock_count);

	/* initialize all clocks instances */
	for (clkid = 0; clkid < clock_count; clkid++) {
		clock = &clock_tree[clkid];
		clock->id = clkid;
		clock->cdata = &clock_tree_data[clkid];

		RPMI_INIT_LIST_HEAD(&clock->node);
		RPMI_INIT_LIST_HEAD(&clock->child_clock);

		/**
		 * All clocks state must be deterministic at this stage
		 * which means this function must not fail with error.
		 * It should return enabled/disabled state for each
		 * clock with success.
		 */
		ret = ops->get_state_and_rate(ops_priv, clkid, &state, &rate);
		if(ret) {
			DPRINTF("%s: failed to get clk-%u state and rate\n",
							__func__, clkid);
			rpmi_env_free(clock_tree);
			return NULL;
		}

		clock->current_state = state;

		if (state == RPMI_CLK_STATE_ENABLED) {
			clock->enable_count += 1;
		}

		clock->lock = rpmi_env_alloc_lock();
	}

	/* Once all clocks instances initialized, link the clocks based
	 * on the clock tree hierarchy based on the provided clock tree
	 * data */
	for (clkid = 0; clkid < clock_count; clkid++) {
		clock = &clock_tree[clkid];
		if (clock->cdata->parent_id != -1U) {
			clock->parent = &clock_tree[clock->cdata->parent_id];
			rpmi_list_add_tail(&clock->node,
					   &clock->parent->child_clock);
			clock->parent->child_count += 1;
		}
		else {
			clock->parent = NULL;
		}
	}

	return clock_tree;
}

/*****************************************************************************
 * RPMI Clock Serivce Group Functions
 ****************************************************************************/
static enum rpmi_error
rpmi_clock_sg_get_num_clocks(struct rpmi_service_group *group,
			     struct rpmi_service *service,
			     struct rpmi_transport *trans,
			     rpmi_uint16_t request_datalen,
			     const rpmi_uint8_t *request_data,
			     rpmi_uint16_t *response_datalen,
			     rpmi_uint8_t *response_data)
{
	struct rpmi_clock_group *clkgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, clkgrp->clock_count);

	*response_datalen = 2 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_clock_sg_get_attributes(struct rpmi_service_group *group,
			     struct rpmi_service *service,
			     struct rpmi_transport *trans,
			     rpmi_uint16_t request_datalen,
			     const rpmi_uint8_t *request_data,
			     rpmi_uint16_t *response_datalen,
			     rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	rpmi_uint32_t flags = 0;
	struct rpmi_clock_attrs clk_attrs;
	struct rpmi_clock_group *clkgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t clkid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (clkid >= clkgrp->clock_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = rpmi_clock_get_attrs(clkgrp, clkid, &clk_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	/* encode CLOCK_FORMAT */
	if (clk_attrs.type == RPMI_CLK_TYPE_LINEAR)
		flags |= 1;

	resp[3] = rpmi_to_xe32(trans->is_be, clk_attrs.transition_latency);
	resp[2] = rpmi_to_xe32(trans->is_be, clk_attrs.rate_count);
	resp[1] = rpmi_to_xe32(trans->is_be, flags);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	if (clk_attrs.name)
		rpmi_env_strncpy((char *)&resp[4], clk_attrs.name, RPMI_CLK_NAME_MAX_LEN);

	resp_dlen = 8 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_clock_sg_get_supp_rates(struct rpmi_service_group *group,
			     struct rpmi_service *service,
			     struct rpmi_transport *trans,
			     rpmi_uint16_t request_datalen,
			     const rpmi_uint8_t *request_data,
			     rpmi_uint16_t *response_datalen,
			     rpmi_uint8_t *response_data)
{
	enum rpmi_error ret;
	rpmi_uint32_t i = 0, j = 0;
	rpmi_uint32_t rate_count;
	rpmi_uint32_t resp_dlen = 0, clk_rate_idx = 0;
	rpmi_uint32_t max_rates, remaining = 0, returned = 0;
	const rpmi_uint64_t *rate_array;
	struct rpmi_clock_attrs clk_attrs;
	struct rpmi_clock_group *clkgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t clkid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (clkid >= clkgrp->clock_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = rpmi_clock_get_attrs(clkgrp, clkid, &clk_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	rate_count = clk_attrs.rate_count;
	rate_array = clk_attrs.rate_array;
	if (!rate_count || !rate_array) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_NOTSUPP);
		goto done;
	}

	clk_rate_idx = rpmi_to_xe32(trans->is_be,
				    ((const rpmi_uint32_t *)request_data)[1]);

	if (clk_attrs.type == RPMI_CLK_TYPE_LINEAR) {
		/* max, min and step */
		for (i = 0; i < 3; i++) {
			resp[4 + 2*i] = rpmi_to_xe32(trans->is_be,
				(rpmi_uint32_t)RATE_U64TOLO(rate_array[i]));
			resp[5 + 2*i] = rpmi_to_xe32(trans->is_be,
				(rpmi_uint32_t)RATE_U64TOHI(rate_array[i]));
		}
		remaining = 0;
		returned = 3;
	}
	else if (clk_attrs.type == RPMI_CLK_TYPE_DISCRETE) {
		if (clk_rate_idx > rate_count) {
			resp[0] = rpmi_to_xe32(trans->is_be,
					       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
			resp_dlen = sizeof(*resp);
			goto done;
		}

		/* max rates a rpmi message can accommodate */
		max_rates =
		(RPMI_MSG_DATA_SIZE(trans->slot_size) - (4 * sizeof(*resp))) /
					sizeof(struct rpmi_clock_rate);
		remaining = rate_count - clk_rate_idx;
		if (remaining > max_rates)
			returned = max_rates;
		else
			returned = remaining;

		for (i = clk_rate_idx, j = 0; i <= (clk_rate_idx + returned - 1); i++, j++) {
			resp[4 + 2*j] = rpmi_to_xe32(trans->is_be,
				(rpmi_uint32_t)RATE_U64TOLO(rate_array[i]));
			resp[5 + 2*j] = rpmi_to_xe32(trans->is_be,
				(rpmi_uint32_t)RATE_U64TOHI(rate_array[i]));
		}

		remaining = rate_count - (clk_rate_idx + returned);
	}
	else {
		DPRINTF("%s: invalid rate format for clk-%u\n", __func__, clkid);
		return RPMI_ERR_FAILED;
	}

	resp[3] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)returned);
	resp[2] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)remaining);
	/* No flags currently supported */
	resp[1] = rpmi_to_xe32(trans->is_be, 0);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = (4 * sizeof(*resp)) +
				(returned * sizeof(struct rpmi_clock_rate));

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_clock_sg_set_config(struct rpmi_service_group *group,
			 struct rpmi_service *service,
			 struct rpmi_transport *trans,
			 rpmi_uint16_t request_datalen,
			 const rpmi_uint8_t *request_data,
			 rpmi_uint16_t *response_datalen,
			 rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t cfg, new_state;
	struct rpmi_clock_group *clkgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t clkid = rpmi_to_xe32(trans->is_be,
			       ((const rpmi_uint32_t *)request_data)[0]);

	if (clkid >= clkgrp->clock_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	cfg = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[1]);

	/* get command from 0th index bit in config field */
	new_state = (cfg & 0b1) ? RPMI_CLK_STATE_ENABLED : RPMI_CLK_STATE_DISABLED;

	/* change clock config synchronously */
	status = rpmi_clock_set_state(clkgrp, clkid, new_state);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

done:
	*response_datalen = sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_clock_sg_get_config(struct rpmi_service_group *group,
			 struct rpmi_service *service,
			 struct rpmi_transport *trans,
			 rpmi_uint16_t request_datalen,
			 const rpmi_uint8_t *request_data,
			 rpmi_uint16_t *response_datalen,
			 rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error status;
	enum rpmi_clock_state state;
	struct rpmi_clock_group *clkgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t clkid = rpmi_to_xe32(trans->is_be,
				     ((const rpmi_uint32_t *)request_data)[0]);

	if (clkid >= clkgrp->clock_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	status = rpmi_clock_get_state(clkgrp, clkid, &state);
	if (status) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	/** RPMI config field only return enabled or disabled state */
	state = (state == RPMI_CLK_STATE_ENABLED)? 1 : 0;

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)state);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 2 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_clock_sg_set_rate(struct rpmi_service_group *group,
		       struct rpmi_service *service,
		       struct rpmi_transport *trans,
		       rpmi_uint16_t request_datalen,
		       const rpmi_uint8_t *request_data,
		       rpmi_uint16_t *response_datalen,
		       rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	enum rpmi_clock_rate_match rate_match;
	rpmi_uint32_t flags;
	struct rpmi_clock_rate rate;
	rpmi_uint64_t rate_u64;
	struct rpmi_clock_group *clkgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t clkid = rpmi_to_xe32(trans->is_be,
				     ((const rpmi_uint32_t *)request_data)[0]);

	if (clkid >= clkgrp->clock_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	flags = rpmi_to_xe32(trans->is_be,
			      ((const rpmi_uint32_t *)request_data)[1]);

	/* get rate match mode from flags */
	rate_match = flags & 0b11;
	if (rate_match >= RPMI_CLK_RATE_MATCH_MAX_IDX) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	rate.lo = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[2]);
	rate.hi = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[3]);

	rate_u64 = RATE_U64(rate.lo, rate.hi);
	if (rate_u64 == RPMI_CLOCK_RATE_INVALID || rate_u64 == 0) {
	    resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	status = rpmi_clock_set_rate(clkgrp, clkid, rate_match, rate_u64);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

done:
	*response_datalen = sizeof(*resp);
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_clock_sg_get_rate(struct rpmi_service_group *group,
		       struct rpmi_service *service,
		       struct rpmi_transport *trans,
		       rpmi_uint16_t request_datalen,
		       const rpmi_uint8_t *request_data,
		       rpmi_uint16_t *response_datalen,
		       rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error status;
	rpmi_uint64_t rate_u64;
	struct rpmi_clock_group *clkgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t clkid = rpmi_to_xe32(trans->is_be,
				     ((const rpmi_uint32_t *)request_data)[0]);

	if (clkid >= clkgrp->clock_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	status = rpmi_clock_get_rate(clkgrp, clkid, &rate_u64);
	if(status) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	resp[2] = rpmi_to_xe32(trans->is_be,
				(rpmi_uint32_t)RATE_U64TOHI(rate_u64));
	resp[1] = rpmi_to_xe32(trans->is_be,
				(rpmi_uint32_t)RATE_U64TOLO(rate_u64));
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 3 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_clock_services[RPMI_CLK_SRV_ID_MAX] = {
	[RPMI_CLK_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_CLK_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_CLK_SRV_GET_NUM_CLOCKS] = {
		.service_id = RPMI_CLK_SRV_GET_NUM_CLOCKS,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_clock_sg_get_num_clocks,
	},
	[RPMI_CLK_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_CLK_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_clock_sg_get_attributes,
	},
	[RPMI_CLK_SRV_GET_SUPPORTED_RATES] = {
		.service_id = RPMI_CLK_SRV_GET_SUPPORTED_RATES,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_clock_sg_get_supp_rates,
	},
	[RPMI_CLK_SRV_SET_CONFIG] = {
		.service_id = RPMI_CLK_SRV_SET_CONFIG,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_clock_sg_set_config,
	},
	[RPMI_CLK_SRV_GET_CONFIG] = {
		.service_id = RPMI_CLK_SRV_GET_CONFIG,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_clock_sg_get_config,
	},
	[RPMI_CLK_SRV_SET_RATE] = {
		.service_id = RPMI_CLK_SRV_SET_RATE,
		.min_a2p_request_datalen = 16,
		.process_a2p_request = rpmi_clock_sg_set_rate,
	},
	[RPMI_CLK_SRV_GET_RATE] = {
		.service_id = RPMI_CLK_SRV_GET_RATE,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_clock_sg_get_rate,
	},
};

struct rpmi_service_group *
rpmi_service_group_clock_create(rpmi_uint32_t clock_count,
				const struct rpmi_clock_data *clock_tree_data,
				const struct rpmi_clock_platform_ops *ops,
				void *ops_priv)
{
	struct rpmi_clock_group *clkgrp;
	struct rpmi_service_group *group;

	/* All critical parameters should be non-NULL */
	if (!clock_count || !clock_tree_data || !ops) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate clock service group */
	clkgrp = rpmi_env_zalloc(sizeof(*clkgrp));
	if (!clkgrp) {
		DPRINTF("%s: failed to allocate clock service group instance\n",
			__func__);
		return NULL;
	}

	clkgrp->clock_tree = rpmi_clock_tree_init(clock_count,
						 clock_tree_data,
						 ops,
						 ops_priv);
	if (!clkgrp->clock_tree) {
		DPRINTF("%s: failed to initialize clock tree\n", __func__);
		rpmi_env_free(clkgrp);
		return NULL;
	}

	clkgrp->clock_count = clock_count;
	clkgrp->ops = ops;
	clkgrp->ops_priv = ops_priv;

	group = &clkgrp->group;
	group->name = "clk";
	group->servicegroup_id = RPMI_SRVGRP_CLOCK;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed for both M-mode and S-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK | RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_CLK_SRV_ID_MAX;
	group->services = rpmi_clock_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = clkgrp;

	return group;
}

void rpmi_service_group_clock_destroy(struct rpmi_service_group *group)
{
	rpmi_uint32_t clkid;
	struct rpmi_clock_group *clkgrp;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	clkgrp = group->priv;

	for (clkid = 0; clkid < clkgrp->clock_count; clkid++) {
		rpmi_env_free_lock(clkgrp->clock_tree[clkid].lock);
	}

	rpmi_env_free(clkgrp->clock_tree);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}
