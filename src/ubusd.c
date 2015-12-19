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
#include "ubusd.h"

static int usage(const char *progname){
	fprintf(stderr, "Usage: %s [<options>]\n"
		"Options: \n"
		"  -s <socket>:		Set the unix domain socket to listen on\n"
		"\n", progname);
	return 1;
}

int main(int argc, char **argv)
{
	const char *ubusd_socket = UBUS_UNIX_SOCKET;
	int ch;

	signal(SIGPIPE, SIG_IGN);

	printf("initializing uloop\n"); 

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
	
	struct ubusd_context *ctx = ubusd_new(); 
	if(ubusd_listen(ctx, ubusd_socket) < 0){
		fprintf(stderr, "Could not listen on unix socket %s\n", ubusd_socket); 
		return 1; 
	}

	while(true){
		ubusd_process_events(ctx); 
	}

	ubusd_delete(&ctx); 

	return 0;
}
