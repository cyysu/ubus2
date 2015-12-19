
static int ubusd_handle_remove_object(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr){
	struct ubusd_object *obj;

	if (!attr[UBUS_ATTR_OBJID])
		return UBUS_STATUS_INVALID_ARGUMENT;

	obj = ubusd_find_object(blob_attr_get_u32(attr[UBUS_ATTR_OBJID]));
	if (!obj)
		return UBUS_STATUS_NOT_FOUND;

	if (obj->client != cl)
		return UBUS_STATUS_PERMISSION_DENIED;

	blob_buf_reset(&b);
	blob_buf_put_i32(&b, obj->id.id);

	if (obj->type && obj->type->refcount == 1)
		blob_buf_put_i32(&b, obj->type->id.id);

	ubusd_free_object(obj);
	ubusd_send_msg_from_blob(cl, ub, UBUS_MSG_METHOD_RETURN);

	return 0;
}

static int ubusd_handle_add_object(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr)
{
	struct ubusd_object *obj;

	obj = ubusd_create_object(cl, attr);
	if (!obj)
		return UBUS_STATUS_INVALID_ARGUMENT;

	blob_buf_reset(&b);
	blob_buf_put_i32(&b, obj->id.id);
	if (attr[UBUS_ATTR_SIGNATURE])
		blob_buf_put_i32(&b, obj->type->id.id);

	ubusd_send_msg_from_blob(cl, ub, UBUS_MSG_METHOD_RETURN);
	return 0;
}

static void ubusd_send_obj(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct ubusd_object *obj)
{
	struct ubusd_method *m;
	void *s;

	blob_buf_reset(&b);

	blob_offset_t tbl = blob_buf_open_table(&b); 
		blob_buf_put_string(&b, "id"); 
		blob_buf_put_i32(&b, obj->id.id);
		blob_buf_put_string(&b, "client"); 
		blob_buf_put_i32(&b, obj->client->id.id); 

		if (obj->path.key) {
			blob_buf_put_string(&b, "path"); 
			blob_buf_put_string(&b, obj->path.key);
		}
		blob_buf_put_string(&b, "type"); 
		blob_buf_put_i32(&b, obj->type->id.id);
		
		blob_buf_put_string(&b, "methods"); 
		s = blob_buf_open_table(&b);
		list_for_each_entry(m, &obj->type->methods, list)
			blob_buf_put_attr(&b, m->data);
		blob_buf_close_table(&b, s);
	blob_buf_close_table(&b, tbl); 

	ubusd_send_msg_from_blob(cl, ub, UBUS_MSG_METHOD_RETURN);
}

static int ubusd_handle_lookup(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr)
{
	struct ubusd_object *obj;
	char *objpath;
	bool found = false;
	int len;

	if (!attr[UBUS_ATTR_OBJPATH]) {
		avl_for_each_element(&path, obj, path)
			ubusd_send_obj(cl, ub, obj);
		return 0;
	}

	objpath = blob_attr_data(attr[UBUS_ATTR_OBJPATH]);
	len = strlen(objpath);
	if (objpath[len - 1] != '*') {
		obj = avl_find_element(&path, objpath, obj, path);
		if (!obj)
			return UBUS_STATUS_NOT_FOUND;

		ubusd_send_obj(cl, ub, obj);
		return 0;
	}

	objpath[--len] = 0;

	obj = avl_find_ge_element(&path, objpath, obj, path);
	if (!obj)
		return UBUS_STATUS_NOT_FOUND;

	while (!strncmp(objpath, obj->path.key, len)) {
		found = true;
		ubusd_send_obj(cl, ub, obj);
		if (obj == avl_last_element(&path, obj, path))
			break;
		obj = avl_next_element(obj, path);
	}

	if (!found)
		return UBUS_STATUS_NOT_FOUND;

	return 0;
}


