#pragma once

struct ubusd_msg_buf {
	uint32_t refcount; /* ~0: uses external data buffer */
	struct ubus_msghdr hdr;
	struct blob_attr *data;
	int fd;
	int len;
};

void ubusd_msg_init(struct ubusd_msg_buf *ub, uint8_t type, uint16_t seq, uint32_t peer); 
struct ubusd_msg_buf *ubusd_msg_ref(struct ubusd_msg_buf *ub); 
