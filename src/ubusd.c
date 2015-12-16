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

#include "ubusd.h"

static struct uloop uloop; 
struct blob_buf b; 

static bool get_next_connection(int fd)
{
	struct ubusd_client *cl;
	int client_fd;

	client_fd = accept(fd, NULL, 0);
	if (client_fd < 0) {
		switch (errno) {
		case ECONNABORTED:
		case EINTR:
			return true;
		default:
			return false;
		}
	}

	cl = ubusd_proto_new_client(client_fd);
	if (cl)
		uloop_add_fd(&uloop, &cl->sock, ULOOP_READ | ULOOP_EDGE_TRIGGER);
	else
		close(client_fd);

	return true;
}

static void server_cb(struct uloop_fd *fd, unsigned int events)
{
	bool next;

	do {
		next = get_next_connection(fd->fd);
	} while (next);
}

static struct uloop_fd server_fd = {
	.cb = server_cb,
};

static int usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [<options>]\n"
		"Options: \n"
		"  -s <socket>:		Set the unix domain socket to listen on\n"
		"\n", progname);
	return 1;
}

void ubusd_obj_init(); 
void ubusd_proto_init(); 

int main(int argc, char **argv)
{
	const char *ubusd_socket = UBUS_UNIX_SOCKET;
	int ret = 0;
	int ch;
	
	blob_buf_init(&b, 0, 0); 

	signal(SIGPIPE, SIG_IGN);

	printf("initializing uloop\n"); 

	ubusd_obj_init(); 
	ubusd_proto_init(); 

	uloop_init(&uloop);

	while ((ch = getopt(argc, argv, "s:")) != -1) {
		switch (ch) {
		case 's':
			ubusd_socket = optarg;
			break;
		default:
			return usage(argv[0]);
		}
	}

	printf("preparing ubus sockets\n"); 

	unlink(ubusd_socket);
	umask(0177);
	server_fd.fd = usock(USOCK_UNIX | USOCK_SERVER | USOCK_NONBLOCK, ubusd_socket, NULL);
	if (server_fd.fd < 0) {
		perror("usock");
		ret = -1;
		goto out;
	}
	uloop_add_fd(&uloop, &server_fd, ULOOP_READ | ULOOP_EDGE_TRIGGER);

	uloop_run(&uloop);
	unlink(ubusd_socket);

out:
	uloop_destroy(&uloop);
	return ret;
}
