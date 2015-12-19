#include "ubusd.h"
#include <fcntl.h>
#include <unistd.h>

struct ubusd_msg_buf *ubusd_msg_ref(struct ubusd_msg_buf *ub)
{
	if (ub->refcount == ~0)
		return ubusd_msg_new(ub->data, ub->len, false);

	ub->refcount++;
	return ub;
}

struct ubusd_msg_buf *ubusd_msg_new(void *data, int len, bool shared)
{
	struct ubusd_msg_buf *ub;
	int buflen = sizeof(*ub);

	if (!shared)
		buflen += len;

	ub = calloc(1, buflen);
	if (!ub)
		return NULL;

	ub->fd = -1;

	if (shared) {
		ub->refcount = ~0;
		ub->data = data;
	} else {
		ub->refcount = 1;
		ub->data = (void *) (ub + 1);
		if (data)
			memcpy(ub + 1, data, len);
	}

	ub->len = len;
	return ub;
}

void ubusd_msg_free(struct ubusd_msg_buf *ub)
{
	switch (ub->refcount) {
	case 1:
	case ~0:
		if (ub->fd >= 0)
			close(ub->fd);

		free(ub);
		break;
	default:
		ub->refcount--;
		break;
	}
}

void ubusd_msg_init(struct ubusd_msg_buf *ub, uint8_t type, uint16_t seq, uint32_t peer){
	ub->hdr.version = 0;
	ub->hdr.type = type;
	ub->hdr.seq = seq;
	ub->hdr.peer = peer;
}

