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

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#ifdef FreeBSD
#include <sys/param.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <blobpack/blobpack.h>
#include <libusys/uloop.h>
#include <libusys/usock.h>
#include <libutype/list.h>
#include <libubus2/libubus2.h>

#include "ubusd_id.h"
#include "ubusd_obj.h"

#define UBUSD_CLIENT_BACKLOG	32
#define UBUS_OBJ_HASH_BITS	4

extern struct blob_buf b;

struct ubusd_path {
	struct list_head list;
	const char name[];
};

#include "ubusd_context.h"
#include "ubusd_client.h"
#include "ubusd_msg.h"

struct ubusd_msg_buf *ubusd_msg_new(void *data, int len, bool shared);
void ubusd_msg_send(struct ubusd_client *cl, struct ubusd_msg_buf *ub, bool free);
void ubusd_msg_free(struct ubusd_msg_buf *ub);


void ubusd_proto_receive_message(struct ubusd_client *cl, struct ubusd_msg_buf *ub);

void ubusd_event_init(void);
void ubusd_event_cleanup_object(struct ubusd_object *obj);
void ubusd_send_obj_event(struct ubusd_object *obj, bool add);


#endif
