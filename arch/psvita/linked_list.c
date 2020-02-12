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

#include <stdlib.h>

#include "linked_list.h"

void init_list(list *list)
{
  list_node *new_node = malloc(sizeof(list_node));
  new_node->data = NULL;
  new_node->next = new_node->prev = NULL;
  list->head = list->tail = list->current = new_node;
  list->count++;
}

void list_clear(list *list)
{
  if(list->head == NULL && list->tail == NULL)
    return;

  while(list_get_first(list) != NULL)
    list_delete_current(list);

  list->count = 0;
}

boolean list_delete_current(list *list)
{
  list_node *new_current;

  if(list_is_empty(list)) return false;

  list->count--;

  if(list->current->prev == NULL)
  {
    /* Current object is the first one in the list. */
    list->head = list->current->next;
  }
  else
  {
    list->current->prev->next = list->current->next;
  }

  if(list->current->next == NULL)
  {
    /* Current object is the last one in the list. */
    list->tail = list->current->prev;
  }
  else
  {
    list->current->next->prev = list->current->prev;
  }

  /* Bump the current pointer back. If we deleted the first entry, set it to NULL. */
  new_current = list->current->prev;

  if(list->current->data != NULL)
    free(list->current->data);

  free(list->current);

  list->current = new_current;

  return true;
}

void *list_get_first(list *list)
{
  if(list->head == NULL)
    return NULL;

  list->current = list->head;
  return list->current->data;
}

void *list_get_last(list *list)
{
  if(list->tail == NULL)
    return NULL;

  list->current = list->tail;
  return list->current->data;
}

list_node *list_get_last_node(list *list)
{
  if(list->tail == NULL)
    return NULL;

  return list->tail;
}

void *list_get_next(list *list)
{
  /* If the current entry is NULL, set this to a valid value. */
  if(list->current == NULL)
    return list_get_first(list);

  if(list->current->next == NULL)
    return NULL;

  list->current = list->current->next;
  return list->current->data;
}

void *list_get_prev(list *list)
{
  if(list->current->prev == NULL)
    return NULL;

  list->current = list->current->prev;
  return list->current->data;
}

void *list_insert_after_current(list *list)
{
  list->count++;
  if(list_is_empty(list))
  {
    init_list(list);
    return list->current->data;
  }

  list_node *new_node = malloc(sizeof(list_node));
  new_node->data = NULL;

  new_node->prev = list->current;
  if(list->current->next == NULL)
  {
    /* Current object is the last one in the list. */
    list->tail = new_node;
    new_node->next = NULL;
  }
  else
  {
    new_node->next = list->current->next;
    new_node->next->prev = new_node;
  }

  list->current = list->current->next = new_node;

  return new_node->data;
}

void *list_insert_before_current(list *list)
{
  list->count++;
  if(list_is_empty(list))
  {
    init_list(list);
    return list->current->data;
  }

  list_node *new_node = malloc(sizeof(list_node));
  new_node->data = NULL;

  new_node->next = list->current;
  if(list->current->prev == NULL)
  {
    /* Current object is the first one in the list. */
    list->head = new_node;
    new_node->prev = NULL;
  }
  else
  {
    new_node->prev = list->current->prev;
    new_node->prev->next = new_node;
  }

  list->current = list->current->prev = new_node;
  return new_node->data;
}

void *list_insert_first(list *list)
{
  list->count++;
  if(list_is_empty(list))
  {
    init_list(list);
    return list->current->data;
  }

  list_node *new_node = malloc(sizeof(list_node));
  new_node->data = NULL;

  new_node->next = list->head;
  list->head->prev = new_node;
  list->current = list->head = new_node;
  new_node->prev = NULL;

  return new_node->data;
}

void *list_insert_last(list *list)
{
  list->count++;
  if(list_is_empty(list))
  {
    init_list(list);
    return list->current->data;
  }

  list_node *new_node = malloc(sizeof(list_node));
  new_node->data = NULL;

  new_node->prev = list->tail;
  list->tail->next = new_node;
  list->current = list->tail = new_node;
  new_node->next = NULL;

  return new_node->data;
}

boolean list_is_at_end(list *list)
{
  return list->current->next == NULL;
}

boolean list_is_at_start(list *list)
{
  return list->current->prev == NULL;
}

boolean list_is_empty(list *list)
{
  return list->head == NULL && list->tail == NULL;
}

void list_new(list *list)
{
  list->head = NULL;
  list->tail = NULL;
  list->current = NULL;
  list->count = 0;
}
