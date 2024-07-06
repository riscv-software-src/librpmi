/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Simple intrusive doubly-linked list library.
 *
 * Adapted from OpenSBI source file include/sbi/sbi_list.h
 *
 * Copyright (c) 2024 Ventana Micro Systems
 */

#ifndef __LIBRPMI_LIST_H__
#define __LIBRPMI_LIST_H__

#include <librpmi_env.h>

#define RPMI_LIST_POISON_PREV	0xDEADBEEF
#define RPMI_LIST_POISON_NEXT	0xFADEBABE

struct rpmi_dlist {
	struct rpmi_dlist *next, *prev;
};

#define RPMI_LIST_HEAD_INIT(__lname)	{ &(__lname), &(__lname) }

#define RPMI_LIST_HEAD(_lname)	\
struct rpmi_dlist _lname = RPMI_LIST_HEAD_INIT(_lname)

#define RPMI_INIT_LIST_HEAD(ptr)	\
do { \
	(ptr)->next = ptr; (ptr)->prev = ptr; \
} while (0)

static inline void __rpmi_list_add(struct rpmi_dlist *new,
				  struct rpmi_dlist *prev,
				  struct rpmi_dlist *next)
{
	new->prev = prev;
	new->next = next;
	prev->next = new;
	next->prev = new;
}

/**
 * Checks if the list is empty or not.
 * @param head List head
 *
 * Returns true if list is empty, false otherwise.
 */
static inline rpmi_bool_t rpmi_list_empty(struct rpmi_dlist *head)
{
	return head->next == head;
}

/**
 * Adds the new node after the given head.
 * @param new New node that needs to be added to list.
 * @param head List head after which the "new" node should be added.
 * Note: the new node is added after the head.
 */
static inline void rpmi_list_add(struct rpmi_dlist *new, struct rpmi_dlist *head)
{
	__rpmi_list_add(new, head, head->next);
}

/**
 * Adds a node at the tail where tnode points to tail node.
 * @param new The new node to be added before tail.
 * @param tnode The current tail node.
 * Note: the new node is added before tail node.
 */
static inline void rpmi_list_add_tail(struct rpmi_dlist *new,
				     struct rpmi_dlist *tnode)
{
	__rpmi_list_add(new, tnode->prev, tnode);
}

static inline void __rpmi_list_del(struct rpmi_dlist *prev,
				  struct rpmi_dlist *next)
{
	prev->next = next;
	next->prev = prev;
}

static inline void __rpmi_list_del_entry(struct rpmi_dlist *entry)
{
	__rpmi_list_del(entry->prev, entry->next);
}

/**
 * Deletes a given entry from list.
 * @param node Node to be deleted.
 */
static inline void rpmi_list_del(struct rpmi_dlist *entry)
{
	__rpmi_list_del(entry->prev, entry->next);
	entry->next = (void *)RPMI_LIST_POISON_NEXT;
	entry->prev = (void *)RPMI_LIST_POISON_PREV;
}

/**
 * Deletes entry from list and reinitialize it.
 * @param entry the element to delete from the list.
 */
static inline void rpmi_list_del_init(struct rpmi_dlist *entry)
{
	__rpmi_list_del_entry(entry);
	RPMI_INIT_LIST_HEAD(entry);
}

/**
 * Get the struct for this entry
 * @param ptr the &struct list_head pointer.
 * @param type the type of the struct this is embedded in.
 * @param member the name of the list_struct within the struct.
 */
#define rpmi_list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/**
 * Get the first element from a list
 * @param ptr the list head to take the element from.
 * @param type the type of the struct this is embedded in.
 * @param member the name of the list_struct within the struct.
 *
 * Note: that list is expected to be not empty.
 */
#define rpmi_list_first_entry(ptr, type, member) \
	rpmi_list_entry((ptr)->next, type, member)

/**
 * Get the last element from a list
 * @param ptr the list head to take the element from.
 * @param type the type of the struct this is embedded in.
 * @param member the name of the list_head within the struct.
 *
 * Note: that list is expected to be not empty.
 */
#define rpmi_list_last_entry(ptr, type, member) \
	rpmi_list_entry((ptr)->prev, type, member)

/**
 * Iterate over a list
 * @param pos the &struct list_head to use as a loop cursor.
 * @param head the head for your list.
 */
#define rpmi_list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * Iterate over list of given type
 * @param pos the type * to use as a loop cursor.
 * @param head the head for your list.
 * @param member the name of the list_struct within the struct.
 */
#define rpmi_list_for_each_entry(pos, head, member) \
	for (pos = rpmi_list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = rpmi_list_entry(pos->member.next, typeof(*pos), member))

#endif
