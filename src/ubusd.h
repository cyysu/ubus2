/*
 * Copyright (C) 2011-2014 Felix Fietkau <nbd@openwrt.org>
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

#ifndef __UBUSD_H
#define __UBUSD_H

#include <libubox/list.h>
#include <libubox/uloop.h>
#include <libubox/blobmsg.h>
#include <libubus2/libubus2.h>

#include "ubusd_id.h"
#include "ubusd_obj.h"

#define UBUSD_CLIENT_BACKLOG	32
#define UBUS_OBJ_HASH_BITS	4

extern struct blob_buf b;

struct ubusd_msg_buf {
	uint32_t refcount; /* ~0: uses external data buffer */
	struct ubus_msghdr hdr;
	struct blob_attr *data;
	int fd;
	int len;
};

struct ubusd_client {
	struct ubusd_id id;
	struct uloop_fd sock;

	struct list_head objects;

	struct ubusd_msg_buf *tx_queue[UBUSD_CLIENT_BACKLOG];
	unsigned int txq_cur, txq_tail, txq_ofs;

	struct ubusd_msg_buf *pending_msg;
	int pending_msg_offset;
	int pending_msg_fd;
	struct {
		struct ubus_msghdr hdr;
		struct blob_attr data;
	} hdrbuf;
};

struct ubusd_path {
	struct list_head list;
	const char name[];
};

struct ubusd_msg_buf *ubusd_msg_new(void *data, int len, bool shared);
void ubusd_msg_send(struct ubusd_client *cl, struct ubusd_msg_buf *ub, bool free);
void ubusd_msg_free(struct ubusd_msg_buf *ub);

struct ubusd_client *ubusd_proto_new_client(int fd, uloop_fd_handler cb);
void ubusd_proto_receive_message(struct ubusd_client *cl, struct ubusd_msg_buf *ub);
void ubusd_proto_free_client(struct ubusd_client *cl);

void ubusd_event_init(void);
void ubusd_event_cleanup_object(struct ubusd_object *obj);
void ubusd_send_obj_event(struct ubusd_object *obj, bool add);


#endif
