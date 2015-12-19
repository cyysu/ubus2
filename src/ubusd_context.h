#pragma once

#include <libusys/uloop.h>
#include <blobpack/blobpack.h>
#include <libutype/avl.h>
#include "ubusd_id.h"

typedef int (*ubusd_cmd_cb)(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr);

struct ubusd_context {
	struct uloop uloop; 
	struct blob_buf buf; 
	struct uloop_fd socket; 
	struct avl_tree clients;
	char *unix_socket; 
}; 

struct ubusd_context *ubusd_new(void); 
void ubusd_delete(struct ubusd_context **self); 

void ubusd_init(struct ubusd_context *self); 
void ubusd_destroy(struct ubusd_context *self); 

int ubusd_listen(struct ubusd_context *self, const char *unix_socket); 
int ubusd_process_events(struct ubusd_context *self); 
