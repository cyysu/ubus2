
void ubusd_notify_subscription(struct ubusd_object *obj)
{
	bool active = !list_empty(&obj->subscribers);
	struct ubusd_msg_buf *ub;

	blob_buf_reset(&b);
	blob_buf_put_i32(&b, obj->id.id);
	blob_buf_put_i8(&b, active);

	ub = ubusd_msg_from_blob(false);
	if (!ub)
		return;

	ubusd_msg_init(ub, UBUS_MSG_NOTIFY, ++obj->invoke_seq, 0);
	ubusd_msg_send(obj->client, ub, true);
}

void ubusd_notify_unsubscribe(struct ubusd_subscription *s)
{
	struct ubusd_msg_buf *ub;

	blob_buf_reset(&b);
	blob_buf_put_i32(&b, s->subscriber->id.id);
	blob_buf_put_i32(&b, s->target->id.id);

	ub = ubusd_msg_from_blob(false);
	if (ub != NULL) {
		ubusd_msg_init(ub, UBUS_MSG_UNSUBSCRIBE, ++s->subscriber->invoke_seq, 0);
		ubusd_msg_send(s->subscriber->client, ub, true);
	}

	ubusd_unsubscribe(s);
}


static int ubusd_handle_add_watch(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr)
{
	struct ubusd_object *obj, *target;

	if (!attr[UBUS_ATTR_OBJID] || !attr[UBUS_ATTR_TARGET])
		return UBUS_STATUS_INVALID_ARGUMENT;

	obj = ubusd_find_object(blob_attr_get_u32(attr[UBUS_ATTR_OBJID]));
	if (!obj)
		return UBUS_STATUS_NOT_FOUND;

	if (cl != obj->client)
		return UBUS_STATUS_INVALID_ARGUMENT;

	target = ubusd_find_object(blob_attr_get_u32(attr[UBUS_ATTR_TARGET]));
	if (!target)
		return UBUS_STATUS_NOT_FOUND;

	if (cl == target->client)
		return UBUS_STATUS_INVALID_ARGUMENT;

	ubusd_subscribe(obj, target);
	return 0;
}

static int ubusd_handle_remove_watch(struct ubusd_client *cl, struct ubusd_msg_buf *ub, struct blob_attr **attr)
{
	struct ubusd_object *obj;
	struct ubusd_subscription *s;
	uint32_t id;

	if (!attr[UBUS_ATTR_OBJID] || !attr[UBUS_ATTR_TARGET])
		return UBUS_STATUS_INVALID_ARGUMENT;

	obj = ubusd_find_object(blob_attr_get_u32(attr[UBUS_ATTR_OBJID]));
	if (!obj)
		return UBUS_STATUS_NOT_FOUND;

	if (cl != obj->client)
		return UBUS_STATUS_INVALID_ARGUMENT;

	id = blob_attr_get_u32(attr[UBUS_ATTR_TARGET]);
	list_for_each_entry(s, &obj->target_list, target_list) {
		if (s->target->id.id != id)
			continue;

		ubusd_unsubscribe(s);
		return 0;
	}

	return UBUS_STATUS_NOT_FOUND;
}


void ubusd_subscribe(struct ubusd_object *obj, struct ubusd_object *target)
{
	struct ubusd_subscription *s;
	bool first = list_empty(&target->subscribers);

	s = calloc(1, sizeof(*s));
	if (!s)
		return;

	s->subscriber = obj;
	s->target = target;
	list_add(&s->list, &target->subscribers);
	list_add(&s->target_list, &obj->target_list);

	if (first)
		ubusd_notify_subscription(target);
}

void ubusd_unsubscribe(struct ubusd_subscription *s)
{
	struct ubusd_object *obj = s->target;

	list_del(&s->list);
	list_del(&s->target_list);
	free(s);

	if (list_empty(&obj->subscribers))
		ubusd_notify_subscription(obj);
}

