/* MegaZeux
 *
 * Copyright (C) 2016-2018 Ian Burgmyer <spectere@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __LINKED_LIST_H
#define __LINKED_LIST_H

#include <inttypes.h>

#include "compat.h"

#define LIST_ITERATE(v, l) for(v = list_get_first(l); \
                               v != NULL; \
                               v = list_get_next(l))

#define LIST_REV_ITERATE(v, l) for(v = list_get_last(l); \
                                   v != NULL; \
                                   v = list_get_prev(l))

typedef struct list_node
{
  void *data;

  struct list_node *prev;
  struct list_node *next;
} list_node;

typedef struct list
{
  list_node *head;
  list_node *tail;
  list_node *current;

  int count;
} list;

boolean list_delete_current(list *list);
void list_clear(list *list);
void *list_get_first(list *list);
void *list_get_last(list *list);
list_node *list_get_last_node(list *list);
void *list_get_next(list *list);
void *list_get_prev(list *list);
void *list_insert_after_current(list *list);
void *list_insert_before_current(list *list);
void *list_insert_first(list *list);
void *list_insert_last(list *list);
boolean list_is_at_end(list *list);
boolean list_is_at_start(list *list);
boolean list_is_empty(list *list);
void list_new(list *list);

#endif /* __LINKED_LIST_H */
