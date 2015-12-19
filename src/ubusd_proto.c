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

#include <arpa/inet.h>
#include <unistd.h>

#include "ubusd.h"

//static struct ubusd_msg_buf *retmsg;
//static int *retmsg_data;

/*
static const struct blob_attr_policy ubusd_policy[UBUS_ATTR_MAX] = {
	[UBUS_ATTR_SIGNATURE] = { .type = BLOB_ATTR_ARRAY },
	[UBUS_ATTR_OBJTYPE] = { .type = BLOB_ATTR_INT32 },
	[UBUS_ATTR_OBJPATH] = { .type = BLOB_ATTR_STRING },
	[UBUS_ATTR_OBJID] = { .type = BLOB_ATTR_INT32 },
	[UBUS_ATTR_STATUS] = { .type = BLOB_ATTR_INT32 },
	[UBUS_ATTR_METHOD] = { .type = BLOB_ATTR_STRING },
};

*/
/*
static void ubusd_msg_close_fd(struct ubusd_msg_buf *ub)
{
	if (ub->fd < 0)
		return;

	close(ub->fd);
	ub->fd = -1;
}
*/
/*
void ubusd_proto_init(void)
{

	blob_buf_reset(&b);
	blob_buf_put_i32(&b, 0);

	//retmsg = ubusd_msg_from_blob(false);
	if (!retmsg)
		exit(1);

	//retmsg->hdr.type = UBUS_MSG_STATUS;
	retmsg_data = blob_attr_data(blob_attr_data(retmsg->data));
}
*/
