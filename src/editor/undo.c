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

#include "board.h"
#include "undo.h"

#include "../block.h"
#include "../board.h"
#include "../graphics.h"
#include "../platform.h"
#include "../robot.h"
#include "../world.h"
#include "../world_struct.h"

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


/****************************/
/* Board specific functions */
/****************************/

// Using full boards here seems tacky, but no one cares about memory usage
// in the editor, these are small boards, and there's not much of a clean
// way to partition the board struct to ignore extra setting fields

struct board_undo_frame
{
  struct world *mzx_world;
  int type;
  int width;
  int height;
  int src_offset;
  struct board *src_board;
  struct board *prev_board;
  struct board *current_board;
};

static void apply_board_undo(struct undo_frame *f)
{
  struct board_undo_frame *current = (struct board_undo_frame *)f;

  copy_board_to_board(current->mzx_world,
   current->prev_board, 0, current->src_board, current->src_offset,
   current->width, current->height
  );
}

static void apply_board_redo(struct undo_frame *f)
{
  struct board_undo_frame *current = (struct board_undo_frame *)f;

  copy_board_to_board(current->mzx_world,
   current->current_board, 0, current->src_board, current->src_offset,
   current->width, current->height
  );
}

static void apply_board_update(struct undo_frame *f)
{
  struct board_undo_frame *current = (struct board_undo_frame *)f;

  // Updates override previous updates (mostly applies to mouse usage)
  if(current->current_board)
    clear_board(current->current_board);

  current->current_board = create_buffer_board(current->width, current->height);

  copy_board_to_board(current->mzx_world,
   current->src_board, current->src_offset, current->current_board, 0,
   current->width, current->height
  );
}

static void apply_board_clear(struct undo_frame *f)
{
  struct board_undo_frame *current = (struct board_undo_frame *)f;

  clear_board(current->prev_board);
  clear_board(current->current_board);
  free(current);
}

struct undo_history *construct_board_undo_history(int max_size)
{
  struct undo_history *h = construct_undo_history(max_size);

  h->undo_function = apply_board_undo;
  h->redo_function = apply_board_redo;
  h->update_function = apply_board_update;
  h->clear_function = apply_board_clear;
  return h;
}

void add_board_undo_frame(struct world *mzx_world, struct undo_history *h,
 struct board *src_board, int src_offset, int width, int height)
{
  struct board_undo_frame *current =
   cmalloc(sizeof(struct board_undo_frame));

  add_undo_frame(h, current);

  current->mzx_world = mzx_world;

  current->width = width;
  current->height = height;
  current->src_offset = src_offset;
  current->src_board = src_board;

  current->prev_board = create_buffer_board(width, height);
  current->current_board = NULL;

  copy_board_to_board(mzx_world,
   src_board, src_offset, current->prev_board, 0,
   width, height
  );
}


/****************************/
/* Layer specific functions */
/****************************/

struct layer_undo_frame
{
  int type;
  int width;
  int height;
  int layer_width;
  int layer_offset;
  char *layer_chars;
  char *layer_colors;
  char *prev_chars;
  char *prev_colors;
  char *current_chars;
  char *current_colors;
};

static void apply_layer_undo(struct undo_frame *f)
{
  struct layer_undo_frame *lf = (struct layer_undo_frame *)f;

  copy_layer_to_layer(
   lf->prev_chars, lf->prev_colors, lf->width, 0,
   lf->layer_chars, lf->layer_colors, lf->layer_width, lf->layer_offset,
   lf->width, lf->height
  );
}

static void apply_layer_redo(struct undo_frame *f)
{
  struct layer_undo_frame *lf = (struct layer_undo_frame *)f;

  copy_layer_to_layer(
   lf->current_chars, lf->current_colors, lf->width, 0,
   lf->layer_chars, lf->layer_colors, lf->layer_width, lf->layer_offset,
   lf->width, lf->height
  );
}

static void apply_layer_update(struct undo_frame *f)
{
  struct layer_undo_frame *lf = (struct layer_undo_frame *)f;

  copy_layer_to_layer(
   lf->layer_chars, lf->layer_colors, lf->layer_width, lf->layer_offset,
   lf->current_chars, lf->current_colors, lf->width, 0,
   lf->width, lf->height
  );
}

static void apply_layer_clear(struct undo_frame *f)
{
  struct layer_undo_frame *current = (struct layer_undo_frame *)f;

  free(current->prev_chars);
  free(current->prev_colors);
  free(current->current_chars);
  free(current->current_colors);
  free(current);
}

struct undo_history *construct_layer_undo_history(int max_size)
{
  struct undo_history *h = construct_undo_history(max_size);

  h->undo_function = apply_layer_undo;
  h->redo_function = apply_layer_redo;
  h->update_function = apply_layer_update;
  h->clear_function = apply_layer_clear;
  return h;
}

void add_layer_undo_frame(struct undo_history *h, char *layer_chars,
 char *layer_colors, int layer_width, int layer_offset, int width, int height)
{
  struct layer_undo_frame *current =
   cmalloc(sizeof(struct layer_undo_frame));

  add_undo_frame(h, current);

  current->width = width;
  current->height = height;
  current->layer_width = layer_width;
  current->layer_offset = layer_offset;
  current->layer_chars = layer_chars;
  current->layer_colors = layer_colors;

  current->prev_chars = cmalloc(width * height);
  current->prev_colors = cmalloc(width * height);
  current->current_chars = cmalloc(width * height);
  current->current_colors = cmalloc(width * height);

  copy_layer_to_layer(
   layer_chars, layer_colors, layer_width, layer_offset,
   current->prev_chars, current->prev_colors, width, 0,
   width, height
  );
}
