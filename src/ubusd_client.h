#pragma once

struct ubusd_client; 
struct ubusd_client {
	struct ubusd_id id;
	struct uloop_fd sock;

	struct list_head objects;
	
	struct blob_buf buf; 

	struct ubusd_msg_buf *tx_queue[UBUSD_CLIENT_BACKLOG];
	unsigned int txq_cur, txq_tail, txq_ofs;

	struct ubusd_msg_buf *pending_msg;
	int pending_msg_offset;
	int pending_msg_fd;
	struct {
		struct ubus_msghdr hdr;
		struct blob_attr data;
	} hdrbuf;
	struct uloop uloop; 
	void (*on_disconnected)(struct ubusd_client *self);
	void (*on_message)(struct ubusd_client *cl, struct ubusd_msg_buf *ub); 
};

struct ubusd_client *ubusd_client_new(int fd); 
void ubusd_client_delete(struct ubusd_client **self); 

void ubusd_client_init(struct ubusd_client *self, int fd); 
void ubusd_client_destroy(struct ubusd_client *self); 

void ubusd_client_process_events(struct ubusd_client *self); 
void ubusd_client_on_disconnect(struct ubusd_client *self, void (*cb)(struct ubusd_client *self)); 
void ubusd_client_on_message(struct ubusd_client *self, void (*cb)(struct ubusd_client *self, struct ubusd_msg_buf *ub)); 
