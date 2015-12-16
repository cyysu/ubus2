
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

/* takes the msgbuf reference */
void ubusd_msg_send(struct ubusd_client *cl, struct ubusd_msg_buf *ub, bool free)
{
	int written;

	if (!cl->tx_queue[cl->txq_cur]) {
		written = ubusd_msg_writev(cl->sock.fd, ub, 0);
		if (written >= ub->len + sizeof(ub->hdr))
			goto out;

		if (written < 0)
			written = 0;

		cl->txq_ofs = written;

		/* get an event once we can write to the socket again */
		uloop_add_fd(&uloop, &cl->sock, ULOOP_READ | ULOOP_WRITE | ULOOP_EDGE_TRIGGER);
	}
	ubusd_msg_enqueue(cl, ub);

out:
	if (free)
		ubusd_msg_free(ub);
}

static void client_cb(struct uloop_fd *sock, unsigned int events)
{
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
	while ((ub = ubusd_msg_head(cl))) {
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
	if (!ubusd_msg_head(cl) && (events & ULOOP_WRITE))
		uloop_add_fd(&uloop, sock, ULOOP_READ | ULOOP_EDGE_TRIGGER);

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
		ubusd_proto_receive_message(cl, ub);
		goto retry;
	}

out:
	if (!sock->eof || ubusd_msg_head(cl))
		return;

disconnect:
	handle_client_disconnect(cl);
}

