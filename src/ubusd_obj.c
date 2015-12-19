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

#include "ubusd.h"
#include "ubusd_obj.h"

struct avl_tree obj_types;
struct avl_tree objects;
struct avl_tree path;

static void ubusd_unref_object_type(struct ubusd_object_type *type)
{
	struct ubusd_method *m;

	if (--type->refcount > 0)
		return;

	while (!list_empty(&type->methods)) {
		m = list_first_entry(&type->methods, struct ubusd_method, list);
		list_del(&m->list);
		free(m);
	}

	ubusd_free_id(&obj_types, &type->id);
	free(type);
}

static bool ubusd_create_obj_method(struct ubusd_object_type *type, struct blob_attr *attr)
{
	struct ubusd_method *m;
	int bloblen = blob_attr_raw_len(attr);

	m = calloc(1, sizeof(*m) + bloblen);
	if (!m)
		return false;

	list_add_tail(&m->list, &type->methods);
	memcpy(m->data, attr, bloblen);
	m->name = strdup(blob_attr_get_string(attr));

	return true;
}

static struct ubusd_object_type *ubusd_create_obj_type(struct blob_attr *sig)
{
	struct ubusd_object_type *type;

	type = calloc(1, sizeof(*type));
	if (!type)
		return NULL;

	type->refcount = 1;

	if (!ubusd_alloc_id(&obj_types, &type->id, 0))
		goto error_free;

	INIT_LIST_HEAD(&type->methods);

	//blob_for_each_attr(pos, sig, rem) {
	for(struct blob_attr *pos = blob_attr_first_child(sig); pos; pos = blob_attr_next_child(sig, pos)){
		//if (!blobmsg_check_attr(pos, true))
	    //		goto error_unref;

		if (!ubusd_create_obj_method(type, pos))
			goto error_unref;
	}

	return type;

error_unref:
	ubusd_unref_object_type(type);
	return NULL;

error_free:
	free(type);
	return NULL;
}

static struct ubusd_object_type *ubusd_get_obj_type(uint32_t obj_id)
{
	struct ubusd_object_type *type;
	struct ubusd_id *id;

	id = ubusd_find_id(&obj_types, obj_id);
	if (!id)
		return NULL;

	type = container_of(id, struct ubusd_object_type, id);
	type->refcount++;
	return type;
}

struct ubusd_object *ubusd_create_object_internal(struct ubusd_object_type *type, uint32_t id)
{
	struct ubusd_object *obj;

	obj = calloc(1, sizeof(*obj));
	if (!obj)
		return NULL;

	if (!ubusd_alloc_id(&objects, &obj->id, id))
		goto error_free;

	obj->type = type;
	INIT_LIST_HEAD(&obj->list);
	INIT_LIST_HEAD(&obj->events);
	INIT_LIST_HEAD(&obj->subscribers);
	INIT_LIST_HEAD(&obj->target_list);
	if (type)
		type->refcount++;

	return obj;

error_free:
	free(obj);
	return NULL;
}

struct ubusd_object *ubusd_create_object(struct ubusd_client *cl, struct blob_attr **attr)
{
	struct ubusd_object *obj;
	struct ubusd_object_type *type = NULL;

	printf("Add object: "); 
	for(int c = 0; c < UBUS_ATTR_MAX; c++){
		printf("%p ", attr[c]); 
	}
	printf("\n"); 
	blob_attr_dump_json(attr[UBUS_ATTR_SIGNATURE]); 

	if (attr[UBUS_ATTR_OBJTYPE])
		type = ubusd_get_obj_type(blob_attr_get_u32(attr[UBUS_ATTR_OBJTYPE]));
	else if (attr[UBUS_ATTR_SIGNATURE])
		type = ubusd_create_obj_type(attr[UBUS_ATTR_SIGNATURE]);

	obj = ubusd_create_object_internal(type, 0);
	if (type)
		ubusd_unref_object_type(type);

	if (!obj)
		return NULL;

	if (attr[UBUS_ATTR_OBJPATH]) {
		obj->path.key = strdup(blob_attr_data(attr[UBUS_ATTR_OBJPATH]));
		if (!obj->path.key)
			goto free;

		if (avl_insert(&path, &obj->path) != 0) {
			free((void *) obj->path.key);
			obj->path.key = NULL;
			goto free;
		}
		//ubusd_send_obj_event(obj, true);
	}

	obj->client = cl;
	list_add(&obj->list, &cl->objects);

	return obj;

free:
	ubusd_free_object(obj);
	return NULL;
}

void ubusd_free_object(struct ubusd_object *obj)
{
	//struct ubusd_subscription *s, *tmp;

	/*
	list_for_each_entry_safe(s, tmp, &obj->target_list, target_list) {
		ubusd_unsubscribe(s);
	}
	list_for_each_entry_safe(s, tmp, &obj->subscribers, list) {
		ubusd_notify_unsubscribe(s);
	}
*/
	//ubusd_event_cleanup_object(obj);
	if (obj->path.key) {
		//ubusd_send_obj_event(obj, false);
		avl_delete(&path, &obj->path);
		free((void *) obj->path.key);
	}
	if (!list_empty(&obj->list))
		list_del(&obj->list);
	ubusd_free_id(&objects, &obj->id);
	if (obj->type)
		ubusd_unref_object_type(obj->type);
	free(obj);
}

void ubusd_obj_init(void){
	}
