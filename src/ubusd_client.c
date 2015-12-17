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

void ubusd_client_delete(struct ubusd_client **self){
	free(*self); 
	*self = 0; 
}

static void _handle_client_disconnect(struct ubusd_client *cl){
	ubusd_socket_destroy(cl); 

	ubusd_proto_free_client(cl);
	if (cl->pending_msg_fd >= 0)
		close(cl->pending_msg_fd);
	uloop_remove_fd(&cl->uloop, &cl->sock);
	close(cl->sock.fd);
	free(cl);
}

static void _handle_message(struct ubusd_client *cl, struct ubusd_msg_buf *ub){
	ubusd_proto_receive_message(cl, ub);
}

//static void client_cb(struct uloop_fd *sock, unsigned int events); 
void ubusd_client_init(struct ubusd_client *self, int fd){
	memset(self, 0, sizeof(struct ubusd_client)); 
	INIT_LIST_HEAD(&self->objects);
	ubusd_socket_init(self, fd); 
	ubusd_socket_on_disconnect(self, _handle_client_disconnect); 
	ubusd_socket_on_message(self, _handle_message); 
	self->pending_msg_fd = -1;
	uloop_init(&self->uloop); 
}

