#include "ubusd.h"
#include "ubusd_context.h"

struct ubusd_client *ubusd_alloc_client(struct ubusd_context *self, int fd);
void ubusd_delete_client(struct ubusd_context *self, struct ubusd_client **cl);

static int ubusd_handle_method_call(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr); 
static int ubusd_handle_response(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr); 
static int ubusd_handle_signal(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr); 

static const ubusd_cmd_cb handlers[__UBUS_MSG_LAST] = {
	[UBUS_MSG_METHOD_CALL] = ubusd_handle_method_call,
	[UBUS_MSG_METHOD_RETURN] = ubusd_handle_response,
	[UBUS_MSG_ERROR] = ubusd_handle_response,
	[UBUS_MSG_SIGNAL] = ubusd_handle_signal,
};
/*
static struct ubusd_msg_buf *ubusd_msg_from_blob(struct ubusd_context *self, bool shared){
	return ubusd_msg_new(blob_buf_head(&self->buf), blob_buf_size(&self->buf), shared);
}
*/
struct ubusd_context * ubusd_new(void){
	struct ubusd_context *self = calloc(1, sizeof(struct ubusd_context)); 
	ubusd_init(self); 
	return self; 
}

void ubusd_delete(struct ubusd_context **self){
	ubusd_destroy(*self); 
	free(*self); 
	*self = NULL; 
}

void ubusd_init(struct ubusd_context *self){
	uloop_init(&self->uloop);
	blob_buf_init(&self->buf, 0, 0); 
	//ubusd_init_id_tree(&self->objects);
	ubusd_init_id_tree(&self->clients);
	//ubusd_init_id_tree(&self->obj_types);
	//ubusd_init_string_tree(&path, false);
	//ubusd_event_init();
}

void ubusd_destroy(struct ubusd_context *self){
	if(self->unix_socket)
		unlink(self->unix_socket);
}

static void _handle_client_disconnect(struct ubusd_client *cl){
	ubusd_client_destroy(cl); 

		//ubusd_delete_client(&cl);
}

static void _handle_client_message(struct ubusd_client *cl, struct ubusd_msg_buf *ub){
	ubusd_cmd_cb cb = NULL;
	int ret;

	//retmsg->hdr.seq = ub->hdr.seq;
	//retmsg->hdr.peer = ub->hdr.peer;

	printf("IN %s seq=%d peer=%08x: ", ubus_message_types[ub->hdr.type], ub->hdr.seq, ub->hdr.peer);
	blob_attr_dump_json(ub->data); 

	if (ub->hdr.type < __UBUS_MSG_LAST)
		cb = handlers[ub->hdr.type];

	//if (ub->hdr.type != UBUS_MSG_STATUS)
//		ubusd_msg_close_fd(ub);

	struct blob_attr *attrbuf[UBUS_ATTR_MAX]; 
	
	ubus_message_parse(ub->hdr.type, ub->data, attrbuf); 

	if (cb)
		ret = cb(cl, ub, attrbuf);
	else
		ret = UBUS_STATUS_INVALID_COMMAND;

	if (ret == -1)
		return;

	ubusd_msg_free(ub);

	//*retmsg_data = htonl(ret);
	//ubusd_msg_send(cl, retmsg, false);
}


struct ubusd_client *ubusd_alloc_client(struct ubusd_context *self, int fd){
	struct ubusd_client *cl = ubusd_client_new(fd); 
	
	if (!cl)
		return NULL;

	if (!ubusd_alloc_id(&self->clients, &cl->id, 0))
		goto free;

	//if (!ubusd_send_hello(cl))
//		goto delete;

	ubusd_client_on_disconnect(cl, _handle_client_disconnect); 
	ubusd_client_on_message(cl, _handle_client_message); 

	return cl;

//delete:
	//ubusd_free_id(&self->clients, &cl->id);
free:
	ubusd_client_delete(&cl);
	return NULL;
}

static void _server_callback(struct uloop_fd *fd, unsigned int events){
	bool done = false; 
	struct ubusd_context *self = container_of(fd, struct ubusd_context, socket); 

	do {
		int client_fd = accept(fd->fd, NULL, 0);
		if (client_fd < 0) {
			switch (errno) {
			case ECONNABORTED:
			case EINTR:
				done = true;
			default:
				break; 
			}
		}
	
		printf("new client fd connected!\n"); 
		struct ubusd_client *cl = ubusd_alloc_client(self, client_fd); 
		if(cl)
			uloop_add_fd(&self->uloop, &cl->sock, ULOOP_READ | ULOOP_EDGE_TRIGGER); 
		else
			close(client_fd); 
	} while (!done);
}

int ubusd_listen(struct ubusd_context *self, const char *unix_socket){
	unlink(unix_socket);
	umask(0177);
	self->socket.fd = usock(USOCK_UNIX | USOCK_SERVER | USOCK_NONBLOCK, unix_socket, NULL);
	if (self->socket.fd < 0) {
		perror("usock");
		return -1; 
	}
	self->socket.cb = _server_callback; 
	uloop_add_fd(&self->uloop, &self->socket, ULOOP_READ | ULOOP_EDGE_TRIGGER);
	self->unix_socket = strdup(unix_socket); 
	return 0; 
}


void ubusd_delete_client(struct ubusd_context *self, struct ubusd_client **client){
	struct ubusd_client *cl = *client; 
	struct ubusd_object *obj;

	while (!list_empty(&cl->objects)) {
		obj = list_first_entry(&cl->objects, struct ubusd_object, list);
		ubusd_free_object(obj);
	}

	ubusd_free_id(&self->clients, &cl->id);
	ubusd_client_delete(&cl); 
}

/*
static struct ubusd_msg_buf *ubusd_reply_from_blob(struct ubusd_context *self, struct ubusd_msg_buf *ub, bool shared){
	struct ubusd_msg_buf *new;

	new = ubusd_msg_from_blob(self, shared);
	if (!new)
		return NULL;

	ubusd_msg_init(new, UBUS_MSG_METHOD_RETURN, ub->hdr.seq, ub->hdr.peer);
	return new;
}
*/
/*
static void ubusd_send_msg_from_blob(struct ubusd_client *cl, struct ubusd_msg_buf *ub, uint8_t type){
	ub = ubusd_reply_from_blob(self, ub, true);
	if (!ub)
		return;

	ub->hdr.type = type;
	ubusd_msg_send(cl, ub, true);
	
}
*/
/*
static void ubusd_forward_method_call(struct ubusd_object *obj, const char *method,
		     struct ubusd_msg_buf *ub, struct blob_attr *data){
	blob_buf_put_i32(&self->buf, obj->id.id);
	blob_buf_put_string(&self->buf, method);
	if (data)
		blob_buf_put_attr(&self->buf, data);

	ubusd_send_msg_from_blob(obj->client, ub, UBUS_MSG_METHOD_CALL);
}
*/
static int ubusd_handle_method_call(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr){
	struct ubusd_object *obj = NULL;
	struct ubusd_id *id;
	const char *method;

	if (!attr[UBUS_ATTR_METHOD] || !attr[UBUS_ATTR_OBJID])
		return UBUS_STATUS_INVALID_ARGUMENT;

	id = ubusd_find_id(&objects, blob_attr_get_u32(attr[UBUS_ATTR_OBJID]));
	if (!id)
		return UBUS_STATUS_NOT_FOUND;

	obj = container_of(id, struct ubusd_object, id);

	method = blob_attr_data(attr[UBUS_ATTR_METHOD]);

	if (!obj->client)
		return obj->recv_msg(cl, method, attr[UBUS_ATTR_DATA]);

	ub->hdr.peer = cl->id.id;

	//blob_buf_reset(&b);
	//ubusd_forward_method_call(obj, method, ub, attr[UBUS_ATTR_DATA]);
	//ubusd_msg_free(ub);

	return -1;
}

static int ubusd_handle_signal(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr){
	struct ubusd_object *obj = NULL;
	//struct ubusd_subscription *s;
	struct ubusd_id *id;
	//const char *method;
	bool no_reply = false;
	//void *c;

	if (!attr[UBUS_ATTR_METHOD] || !attr[UBUS_ATTR_OBJID])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (attr[UBUS_ATTR_NO_REPLY])
		no_reply = blob_attr_get_i8(attr[UBUS_ATTR_NO_REPLY]);

	id = ubusd_find_id(&objects, blob_attr_get_u32(attr[UBUS_ATTR_OBJID]));
	if (!id)
		return UBUS_STATUS_NOT_FOUND;

	obj = container_of(id, struct ubusd_object, id);
	if (obj->client != cl)
		return UBUS_STATUS_PERMISSION_DENIED;

	if (!no_reply) {
		/*blob_buf_reset(&b);
		blob_buf_put_i32(&b, id->id);
		c = blob_buf_open_array(&b);
		list_for_each_entry(s, &obj->subscribers, list) {
			blob_buf_put_i32(&b, s->subscriber->id.id);
		}
		blob_buf_close_array(&b, c);
		blob_buf_put_i32(&b, 0);
		*/
		//TODO: send out status
		//ubusd_send_msg_from_blob(cl, ub, UBUS_MSG_STATUS);
	}

	ub->hdr.peer = cl->id.id;
	/*method = blob_attr_data(attr[UBUS_ATTR_METHOD]);
	list_for_each_entry(s, &obj->subscribers, list) {
		blob_buf_reset(&b);
		if (no_reply)
			blob_buf_put_i8(&b, 1);
		ubusd_forward_method_call(s->subscriber, method, ub, attr[UBUS_ATTR_DATA]);
		
	}
	ubusd_msg_free(ub);
	*/

	return -1;
}

static __attribute__((unused)) struct ubusd_client *ubusd_get_client_by_id(struct ubusd_context *self, uint32_t id){
	struct ubusd_id *clid;

	clid = ubusd_find_id(&self->clients, id);
	if (!clid)
		return NULL;

	return container_of(clid, struct ubusd_client, id);
}

static int ubusd_handle_response(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr)
{
	struct ubusd_object *obj;
	
	/*
	if (!attr[UBUS_ATTR_OBJID] ||
	    (ub->hdr.type == UBUS_MSG_STATUS && !attr[UBUS_ATTR_STATUS]) ||
	    (ub->hdr.type == UBUS_MSG_DATA && !attr[UBUS_ATTR_DATA]))
		goto error;
*/
	obj = ubusd_find_object(blob_attr_get_u32(attr[UBUS_ATTR_OBJID]));
	if (!obj)
		goto error;

	if (cl != obj->client)
		goto error;

	/*cl = ubusd_get_client_by_id(ub->hdr.peer);
	if (!cl)
		goto error;

	ub->hdr.peer = blob_attr_get_u32(attr[UBUS_ATTR_OBJID]);
	ubusd_msg_send(cl, ub, true);
	*/
	return -1;

error:
	ubusd_msg_free(ub);
	return -1;
}

int ubusd_process_events(struct ubusd_context *self){
	return 0; 	
}
