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

#include <arpa/inet.h>
#include "ubusd.h"

static struct avl_tree patterns;
static struct ubusd_object *event_obj;
static int event_seq = 0;
static int obj_event_seq = 1;

struct event_source {
	struct list_head list;
	struct ubusd_object *obj;
	struct avl_node avl;
	bool partial;
};

static void ubusd_delete_event_source(struct event_source *evs)
{
	list_del(&evs->list);
	avl_delete(&patterns, &evs->avl);
	free(evs);
}

void ubusd_event_cleanup_object(struct ubusd_object *obj)
{
	struct event_source *ev;

	while (!list_empty(&obj->events)) {
		ev = list_first_entry(&obj->events, struct event_source, list);
		ubusd_delete_event_source(ev);
	}
}

enum {
	EVREG_PATTERN,
	EVREG_OBJECT,
	EVREG_LAST,
};

static struct blob_attr_policy evr_policy[] = {
	[EVREG_PATTERN] = { .name = "pattern", .type = BLOB_ATTR_STRING },
	[EVREG_OBJECT] = { .name = "object", .type = BLOB_ATTR_INT32 },
};

static int ubusd_alloc_event_pattern(struct ubusd_client *cl, struct blob_attr *msg)
{
	struct event_source *ev;
	struct ubusd_object *obj;
	struct blob_attr *attr[EVREG_LAST];
	char *pattern, *name;
	uint32_t id;
	bool partial = false;
	int len;

	blob_attr_parse(msg, attr, evr_policy, EVREG_LAST); 
	//evr_policy, EVREG_LAST, attr, blob_attr_data(msg), blob_attr_len(msg));
	if (!attr[EVREG_OBJECT] || !attr[EVREG_PATTERN])
		return UBUS_STATUS_INVALID_ARGUMENT;

	id = blob_attr_get_u32(attr[EVREG_OBJECT]);
	if (id < UBUS_SYSTEM_OBJECT_MAX)
		return UBUS_STATUS_PERMISSION_DENIED;

	obj = ubusd_find_object(id);
	if (!obj)
		return UBUS_STATUS_NOT_FOUND;

	if (obj->client != cl)
		return UBUS_STATUS_PERMISSION_DENIED;

	pattern = blob_attr_data(attr[EVREG_PATTERN]);

	len = strlen(pattern);
	if (pattern[len - 1] == '*') {
		partial = true;
		pattern[len - 1] = 0;
		len--;
	}

	ev = calloc(1, sizeof(*ev) + len + 1);
	if (!ev)
		return UBUS_STATUS_NO_DATA;

	list_add(&ev->list, &obj->events);
	ev->obj = obj;
	ev->partial = partial;
	name = (char *) (ev + 1);
	strcpy(name, pattern);
	ev->avl.key = name;
	avl_insert(&patterns, &ev->avl);

	return 0;
}

typedef struct ubusd_msg_buf *(*event_fill_cb)(void *priv, const char *id);

static void ubusd_send_event_msg(struct ubusd_msg_buf **ub, struct ubusd_client *cl,
				 struct ubusd_object *obj, const char *id,
				 event_fill_cb fill_cb, void *cb_priv)
{
	uint32_t *objid_ptr;

	/* do not loop back events */
	if (obj->client == cl)
	    return;

	/* do not send duplicate events */
	if (obj->event_seen == obj_event_seq)
		return;

	obj->event_seen = obj_event_seq;

	if (!*ub) {
		*ub = fill_cb(cb_priv, id);
		(*ub)->hdr.type = UBUS_MSG_METHOD_CALL;
		(*ub)->hdr.peer = 0;
	}

	objid_ptr = blob_attr_data(blob_attr_data((*ub)->data));
	*objid_ptr = htonl(obj->id.id);

	(*ub)->hdr.seq = ++event_seq;
	ubusd_msg_send(obj->client, *ub, false);
}

static bool strmatch_len(const char *s1, const char *s2, int *len)
{
	for (*len = 0; s1[*len] == s2[*len]; (*len)++)
		if (!s1[*len])
			return true;

	return false;
}

static int ubusd_send_event(struct ubusd_client *cl, const char *id,
			    event_fill_cb fill_cb, void *cb_priv)
{
	struct ubusd_msg_buf *ub = NULL;
	struct event_source *ev;
	int match_len = 0;

	obj_event_seq++;

	/*
	 * Since this tree is sorted alphabetically, we can only expect to find
	 * matching entries as long as the number of matching characters
	 * between the pattern string and our string is monotonically increasing.
	 */
	avl_for_each_element(&patterns, ev, avl) {
		const char *key = ev->avl.key;
		int cur_match_len;
		bool full_match;

		full_match = strmatch_len(id, key, &cur_match_len);
		if (cur_match_len < match_len)
			break;

		match_len = cur_match_len;

		if (!full_match) {
			if (!ev->partial)
				continue;

			if (match_len != strlen(key))
				continue;
		}

		ubusd_send_event_msg(&ub, cl, ev->obj, id, fill_cb, cb_priv);
	}

	if (ub)
		ubusd_msg_free(ub);

	return 0;
}

enum {
	EVMSG_ID,
	EVMSG_DATA,
	EVMSG_LAST,
};

static struct blob_attr_policy ev_policy[] = {
	[EVMSG_ID] = { .name = "id", .type = BLOB_ATTR_STRING },
	[EVMSG_DATA] = { .name = "data", .type = BLOB_ATTR_TABLE },
};

static struct ubusd_msg_buf *
ubusd_create_event_from_msg(void *priv, const char *id)
{
	struct blob_attr *msg = priv;

	blob_buf_reset(&b);
	blob_buf_put_i32(&b, 0);
	blob_buf_put_string(&b, id);
	blob_buf_put_attr(&b, msg); 

	return ubusd_msg_new(blob_buf_head(&b), blob_buf_size(&b), true);
}

static int ubusd_forward_event(struct ubusd_client *cl, struct blob_attr *msg)
{
	struct blob_attr *data;
	struct blob_attr *attr[EVMSG_LAST];
	const char *id;

	//blobmsg_parse(ev_policy, EVMSG_LAST, attr, blob_data(msg), blob_len(msg));
	blob_attr_parse(msg, attr, ev_policy, EVMSG_LAST); 
	if (!attr[EVMSG_ID] || !attr[EVMSG_DATA])
		return UBUS_STATUS_INVALID_ARGUMENT;

	id = blob_attr_data(attr[EVMSG_ID]);
	data = attr[EVMSG_DATA];

	if (!strncmp(id, "ubus.", 5))
		return UBUS_STATUS_PERMISSION_DENIED;

	return ubusd_send_event(cl, id, ubusd_create_event_from_msg, data);
}

static int ubusd_event_recv(struct ubusd_client *cl, const char *method, struct blob_attr *msg)
{
	if (!strcmp(method, "register"))
		return ubusd_alloc_event_pattern(cl, msg);

	if (!strcmp(method, "send"))
		return ubusd_forward_event(cl, msg);

	return UBUS_STATUS_INVALID_COMMAND;
}

static struct ubusd_msg_buf *
ubusd_create_object_event_msg(void *priv, const char *id)
{
	struct ubusd_object *obj = priv;
	void *s;

	blob_buf_reset(&b);
	blob_buf_put_i32(&b, 0); // object id
	blob_buf_put_string(&b, id);
	s = blob_buf_open_table(&b);
	blob_buf_put_string(&b, "id"); 
	blob_buf_put_u32(&b, obj->id.id);
	blob_buf_put_string(&b, "path"); 
	blob_buf_put_string(&b, obj->path.key);
	blob_buf_close_table(&b, s);

	return ubusd_msg_new(blob_buf_head(&b), blob_buf_size(&b), true);
}

void ubusd_send_obj_event(struct ubusd_object *obj, bool add)
{
	const char *id = add ? "ubus.object.add" : "ubus.object.remove";

	ubusd_send_event(NULL, id, ubusd_create_object_event_msg, obj);
}

void ubusd_event_init(void)
{
	ubusd_init_string_tree(&patterns, true);
	event_obj = ubusd_create_object_internal(NULL, UBUS_SYSTEM_OBJECT_EVENT);
	if (event_obj != NULL)
		event_obj->recv_msg = ubusd_event_recv;
}

