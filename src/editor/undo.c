/* MegaZeux
 *
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "undo.h"

#include "../graphics.h"
#include "../platform.h"
#include "../world.h"

static struct undo_history *construct_undo_history(int max_size)
{
  struct undo_history *h = cmalloc(sizeof(struct undo_history));
  h->frames = ccalloc(max_size, sizeof(struct undo_frame *));
  h->current_frame = NULL;
  h->current = -1;
  h->first = -1;
  h->last = -1;
  h->size = max_size;
  h->undo_function = NULL;
  h->redo_function = NULL;
  h->update_function = NULL;
  h->clear_function = NULL;
  return h;
}

static void add_undo_frame(struct undo_history *h, void *f)
{
  struct undo_frame *temp;
  int i;

  h->current_frame = f;

  if(h->current != h->last)
  {
    // Clear everything after the current frame
    // Might be -1 (e.g. every frame has been undone)
    if(h->current > -1)
    {
      i = h->current;
      h->last = h->current;
    }
    else
    {
      // Clear the first frame too (the loop won't)
      i = h->first;
      temp = h->frames[i];
      h->frames[i] = NULL;
      h->clear_function(temp);
      h->first = -1;
    }

    // Clear everything after the current frame
    while(i != h->last)
    {
      i++;
      if(i == h->size)
        i = 0;

      temp = h->frames[i];
      h->frames[i] = NULL;
      h->clear_function(temp);
    }
  }

  if(h->first == -1)
  {
    // First frame in the history
    h->frames[0] = f;
    h->first = 0;
    h->current = 0;
    h->last = 0;
    return;
  }

  h->last++;
  if(h->last == h->size)
    h->last = 0;

  if(h->last == h->first)
  {
    // History full? Delete the first
    temp = h->frames[h->first];
    h->clear_function(temp);

    h->first++;
    if(h->first == h->size)
      h->first = 0;
  }

  h->frames[h->last] = f;
  h->current = h->last;
}

void apply_undo(struct undo_history *h)
{
  // If current is -1, we're at the start of the history and can't undo
  if(h->current > -1)
  {
    // Apply the undo
    h->undo_function(h->current_frame);

    // Reverse to the previous frame
    if(h->current == h->first)
      h->current = -1;

    else if(h->current == 0)
      h->current = h->size - 1;

    else
      h->current--;

    h->current_frame = h->frames[h->current];
  }
}

void apply_redo(struct undo_history *h)
{
  // Only works if frames exist and we're not on the last one
  if(h->current != h->last)
  {
    // Advance to the next frame
    if(h->current == -1)
      h->current = h->first;

    else if(h->current + 1 == h->size)
      h->current = 0;

    else
      h->current++;

    h->current_frame = h->frames[h->current];

    // Apply the redo
    h->redo_function(h->current_frame);
  }
}

void update_undo_frame(struct undo_history *h)
{
  h->update_function(h->current_frame);
}

void destruct_undo_history(struct undo_history *h)
{
  struct undo_frame *f;
  int i;

  for(i = 0; i < h->size; i++)
  {
    f = h->frames[i];
    if(f)
      h->clear_function(f);
  }

  free(h);
}


/******************************/
/* Charset specific functions */
/******************************/

#define CHARSET_WIDTH 32

struct charset_undo_frame
{
  int type;
  int offset;
  int width;
  int height;
  char *prev_chars;
  char *current_chars;
};

static void read_charset_data(char *buffer,
 int offset, int width, int height)
{
  size_t copy_size = width * CHAR_SIZE;
  Uint16 charset_offset = offset;
  Uint16 buffer_offset = 0;
  int i;

  for(i = 0; i < height; i++)
  {
    ec_mem_save_set_var((Uint8 *)buffer + buffer_offset,
     copy_size, charset_offset);

    charset_offset += CHARSET_WIDTH;
    buffer_offset += copy_size;
  }
}

static void write_charset_data(char *buffer,
 int offset, int width, int height)
{
  size_t copy_size = width * CHAR_SIZE;
  Uint16 charset_offset = offset;
  Uint16 buffer_offset = 0;
  int i;

  for(i = 0; i < height; i++)
  {
    // Need to pass the world version because this was intended for runtime use
    ec_mem_load_set_var(buffer + buffer_offset,
     copy_size, charset_offset, WORLD_VERSION);

    charset_offset += CHARSET_WIDTH;
    buffer_offset += copy_size;
  }
}

static void apply_charset_undo(struct undo_frame *f)
{
  struct charset_undo_frame *current = (struct charset_undo_frame *)f;

  write_charset_data(current->prev_chars, current->offset,
   current->width, current->height);
}

static void apply_charset_redo(struct undo_frame *f)
{
  struct charset_undo_frame *current = (struct charset_undo_frame *)f;

  write_charset_data(current->current_chars, current->offset,
   current->width, current->height);
}

static void apply_charset_update(struct undo_frame *f)
{
  struct charset_undo_frame *current = (struct charset_undo_frame *)f;

  read_charset_data(current->current_chars, current->offset,
   current->width, current->height);
}

static void apply_charset_clear(struct undo_frame *f)
{
  struct charset_undo_frame *current = (struct charset_undo_frame *)f;

  free(current->prev_chars);
  free(current->current_chars);
  free(current);
}

struct undo_history *construct_charset_undo_history(int max_size)
{
  struct undo_history *h = construct_undo_history(max_size);

  h->undo_function = apply_charset_undo;
  h->redo_function = apply_charset_redo;
  h->update_function = apply_charset_update;
  h->clear_function = apply_charset_clear;
  return h;
}

void add_charset_undo_frame(struct undo_history *h, int offset,
 int width, int height)
{
  struct charset_undo_frame *current =
   cmalloc(sizeof(struct charset_undo_frame));

  add_undo_frame(h, current);

  current->offset = offset;
  current->width = width;
  current->height = height;

  current->prev_chars = cmalloc(width * height * CHAR_SIZE);
  current->current_chars = cmalloc(width * height * CHAR_SIZE);

  read_charset_data(current->prev_chars, offset, width, height);
}
