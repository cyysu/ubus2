/*
 * Copyright (C) 2011 Felix Fietkau <nbd@openwrt.org>
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

#ifndef __UBUSD_ID_H
#define __UBUSD_ID_H

#include <libubox/avl.h>
#include <stdint.h>

struct ubusd_id {
	struct avl_node avl;
	uint32_t id;
};

void ubusd_init_id_tree(struct avl_tree *tree);
void ubusd_init_string_tree(struct avl_tree *tree, bool dup);
bool ubusd_alloc_id(struct avl_tree *tree, struct ubusd_id *id, uint32_t val);

static inline void ubusd_free_id(struct avl_tree *tree, struct ubusd_id *id)
{
	avl_delete(tree, &id->avl);
}

static inline struct ubusd_id *ubusd_find_id(struct avl_tree *tree, uint32_t id)
{
	struct avl_node *avl;

	avl = avl_find(tree, &id);
	if (!avl)
		return NULL;

	return container_of(avl, struct ubusd_id, avl);
}

#endif
