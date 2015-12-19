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

#include "ubusd.h"

struct ubusd_client *ubusd_client_new(int fd){
	struct ubusd_client *self = malloc(sizeof(struct ubusd_client)); 
	ubusd_client_init(self, fd); 
	return self; 
}
static bool _ubusd_client_send_hello(struct ubusd_client *cl){
	struct ubusd_msg_buf *ub;

	blob_buf_reset(&cl->buf);
	ub = ubusd_msg_new(blob_buf_head(&cl->buf), blob_buf_size(&cl->buf), true);
	if (!ub)
		return false;

	ubusd_msg_init(ub, UBUS_MSG_HELLO, 0, cl->id.id);
	ubusd_msg_send(cl, ub, true);
	return true;
}

static void _socket_cb(struct uloop_fd *sock, unsigned int events); 
void ubusd_client_init(struct ubusd_client *self, int fd){
	memset(self, 0, sizeof(struct ubusd_client)); 
	INIT_LIST_HEAD(&self->objects);
	blob_buf_init(&self->buf, 0, 0); 

	self->sock.fd = fd; 
	self->sock.cb = _socket_cb;

	self->pending_msg_fd = -1;
	_ubusd_client_send_hello(self); 
	uloop_init(&self->uloop); 
}

static struct ubusd_msg_buf *_ubusd_client_msg_head(struct ubusd_client *cl){
	return cl->tx_queue[cl->txq_cur];
}

static void ubusd_msg_dequeue(struct ubusd_client *cl)
{
	struct ubusd_msg_buf *ub = _ubusd_client_msg_head(cl);

	if (!ub)
		return;

	ubusd_msg_free(ub);
	cl->txq_ofs = 0;
	cl->tx_queue[cl->txq_cur] = NULL;
	cl->txq_cur = (cl->txq_cur + 1) % ARRAY_SIZE(cl->tx_queue);
}

void ubusd_client_destroy(struct ubusd_client *self){
	while (_ubusd_client_msg_head(self))
		ubusd_msg_dequeue(self);
	if (self->pending_msg_fd >= 0)
		close(self->pending_msg_fd);
	uloop_remove_fd(&self->uloop, &self->sock);
	close(self->sock.fd);
}

void ubusd_client_delete(struct ubusd_client **self){
	ubusd_client_destroy(*self); 
	free(*self); 
	*self = 0; 
}


static int ubusd_msg_writev(int fd, struct ubusd_msg_buf *ub, int offset)
{
	static struct iovec iov[2];
	static struct {
		struct cmsghdr h;
		int fd;
	} fd_buf = {
		.h = {
			.cmsg_len = sizeof(fd_buf),
			.cmsg_level = SOL_SOCKET,
			.cmsg_type = SCM_RIGHTS,
		},
	};
	struct msghdr msghdr = {
		.msg_iov = iov,
		.msg_iovlen = ARRAY_SIZE(iov),
		.msg_control = &fd_buf,
		.msg_controllen = sizeof(fd_buf),
	};

	fd_buf.fd = ub->fd;
	if (ub->fd < 0) {
		msghdr.msg_control = NULL;
		msghdr.msg_controllen = 0;
	}

	if (offset < sizeof(ub->hdr)) {
		iov[0].iov_base = ((char *) &ub->hdr) + offset;
		iov[0].iov_len = sizeof(ub->hdr) - offset;
		iov[1].iov_base = (char *) ub->data;
		iov[1].iov_len = ub->len;

		return sendmsg(fd, &msghdr, 0);
	} else {
		offset -= sizeof(ub->hdr);
		return write(fd, ((char *) ub->data) + offset, ub->len - offset);
	}
}

static void ubusd_msg_enqueue(struct ubusd_client *cl, struct ubusd_msg_buf *ub)
{
	if (cl->tx_queue[cl->txq_tail])
		return;

	cl->tx_queue[cl->txq_tail] = ubusd_msg_ref(ub);
	cl->txq_tail = (cl->txq_tail + 1) % ARRAY_SIZE(cl->tx_queue);
}

/* takes the msgbuf reference */
void ubusd_msg_send(struct ubusd_client *cl, struct ubusd_msg_buf *ub, bool free){
	int written;

	printf("OUT %s seq=%d peer=%08x: ", ubus_message_types[ub->hdr.type], ub->hdr.seq, ub->hdr.peer);
	blob_attr_dump_json(ub->data); 

	if (!cl->tx_queue[cl->txq_cur]) {
		written = ubusd_msg_writev(cl->sock.fd, ub, 0);
		if (written >= ub->len + sizeof(ub->hdr))
			goto out;

		if (written < 0)
			written = 0;

		cl->txq_ofs = written;

		/* get an event once we can write to the socket again */
		uloop_add_fd(&cl->uloop, &cl->sock, ULOOP_READ | ULOOP_WRITE | ULOOP_EDGE_TRIGGER);
	}
	ubusd_msg_enqueue(cl, ub);

out:
	if (free)
		ubusd_msg_free(ub);
}

static void _socket_cb(struct uloop_fd *sock, unsigned int events){
	struct ubusd_client *cl = container_of(sock, struct ubusd_client, sock);
	struct ubusd_msg_buf *ub;
	static struct iovec iov; 
	static struct {
		struct cmsghdr h;
		int fd;
	} fd_buf = {
		.h = {
			.cmsg_type = SCM_RIGHTS,
			.cmsg_level = SOL_SOCKET,
			.cmsg_len = sizeof(fd_buf),
		}
	};
	struct msghdr msghdr = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};

	/* first try to tx more pending data */
	while ((ub = _ubusd_client_msg_head(cl))) {
		int written;

		written = ubusd_msg_writev(sock->fd, ub, cl->txq_ofs);
		if (written < 0) {
			switch(errno) {
			case EINTR:
			case EAGAIN:
				break;
			default:
				goto disconnect;
			}
			break;
		}

		cl->txq_ofs += written;
		if (cl->txq_ofs < ub->len + sizeof(ub->hdr))
			break;

		ubusd_msg_dequeue(cl);
	}

	/* prevent further ULOOP_WRITE events if we don't have data
	 * to send anymore */
	if (!_ubusd_client_msg_head(cl) && (events & ULOOP_WRITE))
		uloop_add_fd(&cl->uloop, sock, ULOOP_READ | ULOOP_EDGE_TRIGGER);

retry:
	if (!sock->eof && cl->pending_msg_offset < sizeof(cl->hdrbuf)) {
		int offset = cl->pending_msg_offset;
		int bytes;

		fd_buf.fd = -1;

		iov.iov_base = &cl->hdrbuf + offset;
		iov.iov_len = sizeof(cl->hdrbuf) - offset;

		if (cl->pending_msg_fd < 0) {
			msghdr.msg_control = &fd_buf;
			msghdr.msg_controllen = sizeof(fd_buf);
		} else {
			msghdr.msg_control = NULL;
			msghdr.msg_controllen = 0;
		}

		bytes = recvmsg(sock->fd, &msghdr, 0);
		if (bytes < 0)
			goto out;

		if (fd_buf.fd >= 0)
			cl->pending_msg_fd = fd_buf.fd;

		cl->pending_msg_offset += bytes;
		if (cl->pending_msg_offset < sizeof(cl->hdrbuf))
			goto out;

		if (blob_attr_pad_len(&cl->hdrbuf.data) > UBUS_MAX_MSGLEN)
			goto disconnect;

		cl->pending_msg = ubusd_msg_new(NULL, blob_attr_raw_len(&cl->hdrbuf.data), false);
		if (!cl->pending_msg)
			goto disconnect;

		memcpy(&cl->pending_msg->hdr, &cl->hdrbuf.hdr, sizeof(cl->hdrbuf.hdr));
		memcpy(cl->pending_msg->data, &cl->hdrbuf.data, sizeof(cl->hdrbuf.data));
	}

	ub = cl->pending_msg;
	if (ub) {
		int offset = cl->pending_msg_offset - sizeof(ub->hdr);
		int len = blob_attr_raw_len(ub->data) - offset;
		int bytes = 0;

		if (len > 0) {
			bytes = read(sock->fd, (char *) ub->data + offset, len);
			if (bytes <= 0)
				goto out;
		}

		if (bytes < len) {
			cl->pending_msg_offset += bytes;
			goto out;
		}

		/* accept message */
		ub->fd = cl->pending_msg_fd;
		cl->pending_msg_fd = -1;
		cl->pending_msg_offset = 0;
		cl->pending_msg = NULL;
		if(cl->on_message){
			cl->on_message(cl, ub); 
			//ubusd_proto_receive_message(cl, ub);
		}
		goto retry;
	}

out:
	if (!sock->eof || _ubusd_client_msg_head(cl))
		return;

disconnect:
	if(cl->on_disconnected){
		cl->on_disconnected(cl); 
	}
}

void ubusd_client_on_message(struct ubusd_client *self, void (*cb)(struct ubusd_client *self, struct ubusd_msg_buf *ub)){
	self->on_message = cb; 
}
void ubusd_client_on_disconnect(struct ubusd_client *self, void (*cb)(struct ubusd_client *self)){
	self->on_disconnected = cb; 
}

