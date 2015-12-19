/*
 * Copyright (C) 2011 Felix Fietkau <nbd@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __UBUSD_OBJ_H
#define __UBUSD_OBJ_H

#include "ubusd_id.h"

extern struct avl_tree obj_types;
extern struct avl_tree objects;
extern struct avl_tree path;

struct ubusd_client;
struct ubusd_msg_buf;

struct ubusd_object_type {
	struct ubusd_id id;
	int refcount;
	struct list_head methods;
};

struct ubusd_method {
	struct list_head list;
	char *name;
	struct blob_attr data[];
};

struct ubusd_subscription {
	struct list_head list, target_list;
	struct ubusd_object *subscriber, *target;
};

struct ubusd_object {
	struct ubusd_id id;
	struct list_head list;

	struct list_head events;

	struct list_head subscribers, target_list;

	struct ubusd_object_type *type;
	struct avl_node path;

	struct ubusd_client *client;
	int (*recv_msg)(struct ubusd_client *client, const char *method, struct blob_attr *msg);

	int event_seen;
	unsigned int invoke_seq;
};

struct ubusd_object *ubusd_create_object(struct ubusd_client *cl, struct blob_attr **attr);
struct ubusd_object *ubusd_create_object_internal(struct ubusd_object_type *type, uint32_t id);
void ubusd_free_object(struct ubusd_object *obj);

static inline struct ubusd_object *ubusd_find_object(uint32_t objid)
{
	struct ubusd_object *obj;
	struct ubusd_id *id;

	id = ubusd_find_id(&objects, objid);
	if (!id)
		return NULL;

	obj = container_of(id, struct ubusd_object, id);
	return obj;
}
#endif
