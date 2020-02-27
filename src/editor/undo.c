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

#include "block.h"
#include "board.h"
#include "buffer.h"
#include "buffer_struct.h"
#include "edit.h"
#include "graphics.h"
#include "robot.h"
#include "undo.h"

#include "../block.h"
#include "../board.h"
#include "../graphics.h"
#include "../idarray.h"
#include "../platform.h"
#include "../robot.h"
#include "../world.h"
#include "../world_struct.h"

/* Operations for handling undo histories:
 *   Construct a history buffer of some size
 *   Add a new frame to the history, clearing old frames as-needed
 *   Add a position to the current frame (optional, primarily for mouse input)
 *   Update the current frame of the history (primarily for mouse input)
 *   Perform an undo operation: apply previous version, then step back
 *   Perform a redo operation: step forward, then apply current version
 *   Destruct a history buffer
 *
 * Most of these options are generalizable and use the same functions,
 * but the creation of a history buffer and adding a new frame require
 * implementation-specific functions.
 *
 * All necessary data regarding the scope of the frame must be given when
 * a new frame is created; this information can't change easily due to the
 * way e.g. boards are stored.
 *
 * A special case currently exists where the undo actions will operate
 * outside of the undo frame; moving the player on the board.
 */

enum undo_frame_type
{
  POS_FRAME,
  BLOCK_FRAME,
};

struct undo_frame
{
  enum undo_frame_type type;
};

struct undo_history
{
  struct undo_frame **frames;
  struct undo_frame *current_frame;
  int current;
  int first;
  int last;
  int size;
  void (*add_pos_function)(struct undo_frame *, int, int);
  void (*undo_function)(struct undo_frame *);
  void (*redo_function)(struct undo_frame *);
  void (*update_function)(struct undo_frame *);
  void (*clear_function)(struct undo_frame *);
};

static struct undo_history *construct_undo_history(int max_size)
{
  struct undo_history *h = cmalloc(sizeof(struct undo_history));
  h->frames = ccalloc(max_size, sizeof(struct undo_frame *));
  h->current_frame = NULL;
  h->current = -1;
  h->first = -1;
  h->last = -1;
  h->size = max_size;
  h->add_pos_function = NULL;
  h->undo_function = NULL;
  h->redo_function = NULL;
  h->update_function = NULL;
  h->clear_function = NULL;
  return h;
}

static void add_undo_frame(struct undo_history *h, void *f)
{
  struct undo_frame *temp;
  int i, j;

  h->current_frame = f;

  if(h->current != h->last)
  {
    // Clear everything after the current frame
    // Stop at the position after the last frame.
    j = h->last + 1;
    if(j >= h->size)
      j = 0;

    // Might be -1 (e.g. every frame has been undone)
    if(h->current > -1)
    {
      i = h->current + 1;
      if(i >= h->size)
        i = 0;

      h->last = h->current;
    }
    else
    {
      i = h->first;
      h->first = -1;
    }

    // Clear everything after the current frame
    while(i != j)
    {
      temp = h->frames[i];
      h->frames[i] = NULL;
      h->clear_function(temp);

      i++;
      if(i == h->size)
        i = 0;
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

boolean apply_undo(struct undo_history *h)
{
  // If current is -1, we're at the start of the history and can't undo
  if(h && h->current > -1)
  {
    // Apply the undo
    h->undo_function(h->current_frame);

    // Reverse to the previous frame
    if(h->current == h->first)
    {
      h->current = -1;
      h->current_frame = NULL;
      return true;
    }

    else if(h->current == 0)
      h->current = h->size - 1;

    else
      h->current--;

    h->current_frame = h->frames[h->current];
    return true;
  }
  return false;
}

boolean apply_redo(struct undo_history *h)
{
  // Only works if frames exist and we're not on the last one
  if(h && h->current != h->last)
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
    return true;
  }
  return false;
}

void add_undo_position(struct undo_history *h, int x, int y)
{
  struct undo_frame *f;

  if(h && h->add_pos_function)
  {
    f = h->current_frame;

    if(f && f->type == POS_FRAME)
      h->add_pos_function(f, x, y);
  }
}

void update_undo_frame(struct undo_history *h)
{
  if(h && h->current_frame)
    h->update_function(h->current_frame);
}

void destruct_undo_history(struct undo_history *h)
{
  struct undo_frame *f;
  int i;

  if(h)
  {
    for(i = 0; i < h->size; i++)
    {
      f = h->frames[i];
      if(f)
        h->clear_function(f);
    }

    free(h->frames);
    free(h);
  }
}


/******************************/
/* Charset specific functions */
/******************************/

struct charset_undo_frame
{
  struct undo_frame f;
  Uint8 offset;
  Uint8 charset;
  Uint8 width;
  Uint8 height;
  char *prev_chars;
  char *current_chars;
};

static void apply_charset_undo(struct undo_frame *f)
{
  struct charset_undo_frame *current = (struct charset_undo_frame *)f;

  ec_change_block(current->offset, current->charset,
   current->width, current->height, current->prev_chars);
}

static void apply_charset_redo(struct undo_frame *f)
{
  struct charset_undo_frame *current = (struct charset_undo_frame *)f;

  ec_change_block(current->offset, current->charset,
   current->width, current->height, current->current_chars);
}

static void apply_charset_update(struct undo_frame *f)
{
  struct charset_undo_frame *current = (struct charset_undo_frame *)f;

  ec_read_block(current->offset, current->charset,
   current->width, current->height, current->current_chars);
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
  if(max_size)
  {
    struct undo_history *h = construct_undo_history(max_size);

    h->undo_function = apply_charset_undo;
    h->redo_function = apply_charset_redo;
    h->update_function = apply_charset_update;
    h->clear_function = apply_charset_clear;
    return h;
  }
  return NULL;
}

void add_charset_undo_frame(struct undo_history *h, int charset, int first_char,
 int width, int height)
{
  if(h)
  {
    struct charset_undo_frame *current
     = cmalloc(sizeof(struct charset_undo_frame));

    add_undo_frame(h, current);

    current->offset = first_char;
    current->charset = charset;
    current->width = width;
    current->height = height;

    current->prev_chars = cmalloc(width * height * CHAR_SIZE);
    current->current_chars = cmalloc(width * height * CHAR_SIZE);

    ec_read_block(current->offset, current->charset,
     current->width, current->height, current->prev_chars);
  }
}


/****************************/
/* Board specific functions */
/****************************/

// "board_undo_frame" is for small updates or updates that take place over
// a difficult-to-define area of the board (e.g. mouse placement, flood fill).
// This version takes special updates with the new positions drawn, but still
// requires a normal update after placement. This version requires something
// to be placed from the buffer to work

// "block_undo_frame" is for larger updates that occur on a well-known
// area of the board, and use fake boards and block copies. This version does
// not require anything to be placed from the buffer

// storage_obj is a pointer to a robot, scroll, or sensor
struct board_undo_pos
{
  Sint16 x;
  Sint16 y;
  char id;
  char color;
  char param;
  char under_id;
  char under_color;
  char under_param;
  void *storage_obj;
};

struct board_undo_frame
{
  struct undo_frame f;
  struct world *mzx_world;
  int move_player;
  int prev_player_x;
  int prev_player_y;
  int current_player_x;
  int current_player_y;
  struct board_undo_pos replace;
  struct board_undo_pos *prev;
  int prev_alloc;
  int prev_size;
};

struct block_undo_frame
{
  struct undo_frame f;
  struct world *mzx_world;
  int move_player;
  int prev_player_x;
  int prev_player_y;
  int current_player_x;
  int current_player_y;
  int width;
  int height;
  int src_offset;
  struct board *src_board;
  struct board *prev_board;
  struct board *current_board;
};

static void alloc_board_undo_pos(struct board_undo_pos *pos)
{
  // An undo position that is going to take a storage object needs
  // to have that storage object manually allocated for it
  enum thing id = (enum thing)pos->id;

  pos->storage_obj = NULL;

  if(is_robot(id))
  {
    pos->param = -1;
    pos->storage_obj = cmalloc(sizeof(struct robot));
    memset(pos->storage_obj, 0, sizeof(struct robot));
  }
  else

  if(is_signscroll(id))
  {
    pos->param = -1;
    pos->storage_obj = cmalloc(sizeof(struct scroll));
    memset(pos->storage_obj, 0, sizeof(struct scroll));
  }
  else

  if(id == SENSOR)
  {
    pos->param = -1;
    pos->storage_obj = cmalloc(sizeof(struct sensor));
    memset(pos->storage_obj, 0, sizeof(struct sensor));
  }
}

static void get_board_undo_pos_storage(struct board_undo_pos *pos,
 struct buffer_info *temp_buffer)
{
  enum thing id = (enum thing)pos->id;

  temp_buffer->robot = NULL;
  temp_buffer->scroll = NULL;
  temp_buffer->sensor = NULL;

  if(is_robot(id))
  {
    temp_buffer->robot = pos->storage_obj;
  }
  else

  if(is_signscroll(id))
  {
    temp_buffer->scroll = pos->storage_obj;
  }
  else

  if(id == SENSOR)
  {
    temp_buffer->sensor = pos->storage_obj;
  }
}

static void clear_board_undo_pos(struct board_undo_pos *pos)
{
  enum thing id = (enum thing)pos->id;

  if(is_robot(id))
  {
    clear_robot_contents(pos->storage_obj);
  }
  else

  if(is_signscroll(id))
  {
    clear_scroll_contents(pos->storage_obj);
  }

  free(pos->storage_obj);
}

static void apply_board_undo(struct undo_frame *f)
{
  struct board_undo_frame *current = (struct board_undo_frame *)f;
  struct world *mzx_world = current->mzx_world;
  struct player *player = &mzx_world->players[0];

  // Can't overwrite the player, so move the player first. This needs to be
  // done for both types of operations, as either may require relocating the
  // player to the player's previous position.
  if(current->move_player)
    id_remove_top(mzx_world, player->x, player->y);

  switch(f->type)
  {
    case POS_FRAME:
    {
      struct board_undo_pos *prev;
      struct buffer_info temp_buffer;
      int i;

      for(i = current->prev_size - 1; i >= 0; i--)
      {
        // Place the under, then the regular
        prev = &(current->prev[i]);
        temp_buffer.id = prev->under_id;
        temp_buffer.param = prev->under_param;
        temp_buffer.color = prev->under_color;
        temp_buffer.robot = NULL;
        temp_buffer.scroll = NULL;
        temp_buffer.sensor = NULL;

        place_current_at_xy(mzx_world, &temp_buffer, prev->x, prev->y,
         EDIT_BOARD, NULL);

        temp_buffer.id = prev->id;
        temp_buffer.param = prev->param;
        temp_buffer.color = prev->color;
        get_board_undo_pos_storage(prev, &temp_buffer);

        place_current_at_xy(mzx_world, &temp_buffer, prev->x, prev->y,
         EDIT_BOARD, NULL);
      }
      break;
    }

    case BLOCK_FRAME:
    {
      struct block_undo_frame *current = (struct block_undo_frame *)f;

      copy_board_to_board(mzx_world,
       current->prev_board, 0, current->src_board, current->src_offset,
       current->width, current->height
      );
      break;
    }
  }

  if(current->move_player)
    copy_replace_one_player(mzx_world, 0, current->prev_player_x,
     current->prev_player_y);
}

static void apply_board_redo(struct undo_frame *f)
{
  struct board_undo_frame *current = (struct board_undo_frame *)f;
  struct world *mzx_world = current->mzx_world;
  struct player *player = &mzx_world->players[0];

  switch(f->type)
  {
    case POS_FRAME:
    {
      // Don't need any special handling for the player here. The only thing
      // that can move the player during this redo operation is if the player
      // is the thing being placed; place_current_at_xy can handle that.
      struct board_undo_pos *next = &(current->replace);
      struct board_undo_pos *prev;
      struct buffer_info temp_buffer;
      int i;

      temp_buffer.id = next->id;
      temp_buffer.param = next->param;
      temp_buffer.color = next->color;
      get_board_undo_pos_storage(next, &temp_buffer);

      for(i = 0; i < current->prev_size; i++)
      {
        prev = &(current->prev[i]);
        place_current_at_xy(mzx_world, &temp_buffer, prev->x, prev->y,
         EDIT_BOARD, NULL);
      }
      break;
    }

    case BLOCK_FRAME:
    {
      struct block_undo_frame *current = (struct block_undo_frame *)f;

      // Copy won't overwrite the player, so remove the player first.
      if(current->move_player)
        id_remove_top(mzx_world, player->x, player->y);

      copy_board_to_board(current->mzx_world,
       current->current_board, 0, current->src_board, current->src_offset,
       current->width, current->height
      );

      if(current->move_player)
        copy_replace_one_player(mzx_world, 0, current->current_player_x,
         current->current_player_y);

      break;
    }
  }
}

static void apply_board_update(struct undo_frame *f)
{
  struct board_undo_frame *current = (struct board_undo_frame *)f;
  struct player *player;

  switch(f->type)
  {
    case POS_FRAME:
    {
      // Trim extra size off of the allocation
      int size = current->prev_size;

      current->prev_alloc = size;
      current->prev =
       crealloc(current->prev, size * sizeof(struct board_undo_pos));
      break;
    }

    case BLOCK_FRAME:
    {
      struct block_undo_frame *current = (struct block_undo_frame *)f;

      // Update the block frame
      if(current->current_board)
        clear_board(current->current_board);

      current->current_board =
       create_buffer_board(current->width, current->height);

      copy_board_to_board(current->mzx_world,
       current->src_board, current->src_offset, current->current_board, 0,
       current->width, current->height
      );
      break;
    }
  }

  // Handle player position changes
  player = &current->mzx_world->players[0];
  current->current_player_x = player->x;
  current->current_player_y = player->y;

  if((current->current_player_x != current->prev_player_x) ||
   (current->current_player_y != current->prev_player_y))
    current->move_player = 1;

  else
    current->move_player = 0;
}

static void apply_board_clear(struct undo_frame *f)
{
  switch(f->type)
  {
    case POS_FRAME:
    {
      struct board_undo_frame *current = (struct board_undo_frame *)f;
      struct board_undo_pos *prev = current->prev;
      int i;

      clear_board_undo_pos(&(current->replace));

      for(i = 0; i < current->prev_size; i++)
      {
        clear_board_undo_pos(prev);
        prev++;
      }

      free(current->prev);
      break;
    }

    case BLOCK_FRAME:
    {
      struct block_undo_frame *current = (struct block_undo_frame *)f;
      clear_board(current->prev_board);
      clear_board(current->current_board);
    }
  }
  free(f);
}

static void add_board_undo_position(struct undo_frame *f, int x, int y)
{
  struct board_undo_frame *current = (struct board_undo_frame *)f;

  struct world *mzx_world = current->mzx_world;
  struct board *src_board = mzx_world->current_board;
  int offset = x + (src_board->board_width * y);

  struct board_undo_pos *pos;
  struct board_undo_pos *prev = current->prev;
  int prev_alloc = current->prev_alloc;
  int prev_size = current->prev_size;

  struct buffer_info temp_buffer;

  // Can't place over player
  if(src_board->level_id[offset] == PLAYER)
    return;

  if(prev_size == prev_alloc)
  {
    if(!prev_alloc)
      prev_alloc = 1;

    else
      prev_alloc *= 2;

    prev = crealloc(prev, prev_alloc * sizeof(struct board_undo_pos));
  }

  pos = &(prev[prev_size]);
  prev_size++;

  pos->x = x;
  pos->y = y;
  pos->id = src_board->level_id[offset];
  pos->color = -1;
  pos->param = -1;
  pos->under_id = src_board->level_under_id[offset];
  pos->under_color = src_board->level_under_color[offset];
  pos->under_param = src_board->level_under_param[offset];

  alloc_board_undo_pos(pos);
  memset(&temp_buffer, 0, sizeof(struct buffer_info));
  temp_buffer.robot = pos->storage_obj;
  temp_buffer.scroll = pos->storage_obj;
  temp_buffer.sensor = pos->storage_obj;

  grab_at_xy(mzx_world, &temp_buffer, x, y, EDIT_BOARD);

  pos->id = temp_buffer.id;
  pos->color = temp_buffer.color;
  pos->param = temp_buffer.param;

  current->prev_alloc = prev_alloc;
  current->prev_size = prev_size;
  current->prev = prev;
}

struct undo_history *construct_board_undo_history(int max_size)
{
  if(max_size)
  {
    struct undo_history *h = construct_undo_history(max_size);

    h->add_pos_function = add_board_undo_position;
    h->undo_function = apply_board_undo;
    h->redo_function = apply_board_redo;
    h->update_function = apply_board_update;
    h->clear_function = apply_board_clear;
    return h;
  }
  return NULL;
}

void add_board_undo_frame(struct world *mzx_world, struct undo_history *h,
 struct buffer_info *buffer, int x, int y)
{
  struct player *player = &mzx_world->players[0];

  if(h)
  {
    struct board_undo_frame *current =
     cmalloc(sizeof(struct board_undo_frame));

    struct board_undo_pos *replace = &(current->replace);

    add_undo_frame(h, current);
    current->f.type = POS_FRAME;

    // The player might be moved by the frame, so back up the position
    current->mzx_world = mzx_world;
    current->prev_player_x = player->x;
    current->prev_player_y = player->y;
    current->move_player = 0;

    // We only care about a handful of things here
    replace->id = buffer->id;
    replace->color = buffer->color;
    replace->param = buffer->param;
    alloc_board_undo_pos(replace);

    if(is_robot(buffer->id))
      duplicate_robot_direct_source(mzx_world, buffer->robot,
       replace->storage_obj, 0, 0);

    else
    if(is_signscroll(buffer->id))
      duplicate_scroll_direct(buffer->scroll, replace->storage_obj);

    else
    if(buffer->id == SENSOR)
      duplicate_sensor_direct(buffer->sensor, replace->storage_obj);

    current->prev = NULL;
    current->prev_size = 0;
    current->prev_alloc = 0;

    // Most places this is used don't want to bother doing this manually,
    // so do it here for the first position.
    add_undo_position(h, x, y);
  }
}

void add_block_undo_frame(struct world *mzx_world, struct undo_history *h,
 struct board *src_board, int src_offset, int width, int height)
{
  struct player *player = &mzx_world->players[0];

  if(h)
  {
    struct block_undo_frame *current =
     cmalloc(sizeof(struct block_undo_frame));

    add_undo_frame(h, current);
    current->f.type = BLOCK_FRAME;

    // The player might be moved by the frame, so back up the position
    current->mzx_world = mzx_world;
    current->prev_player_x = player->x;
    current->prev_player_y = player->y;
    current->move_player = 0;

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
}


/****************************/
/* Layer specific functions */
/****************************/

struct layer_undo_pos
{
  Sint16 x;
  Sint16 y;
  char chr;
  char color;
};

struct layer_undo_pos_frame
{
  struct undo_frame f;
  int layer_width;
  char *layer_chars;
  char *layer_colors;
  struct layer_undo_pos replace;
  struct layer_undo_pos *prev;
  int prev_alloc;
  int prev_size;
};

struct layer_undo_frame
{
  struct undo_frame f;
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
  switch(f->type)
  {
    case POS_FRAME:
    {
      struct layer_undo_pos_frame *current = (struct layer_undo_pos_frame *)f;
      struct layer_undo_pos *prev;
      int offset;
      int i;

      for(i = current->prev_size - 1; i >= 0; i--)
      {
        prev = &(current->prev[i]);
        offset = prev->x + (prev->y * current->layer_width);
        current->layer_chars[offset] = prev->chr;
        current->layer_colors[offset] = prev->color;
      }
      break;
    }

    case BLOCK_FRAME:
    {
      struct layer_undo_frame *lf = (struct layer_undo_frame *)f;

      copy_layer_buffer_to_buffer(
       lf->prev_chars, lf->prev_colors, lf->width, 0,
       lf->layer_chars, lf->layer_colors, lf->layer_width, lf->layer_offset,
       lf->width, lf->height
      );
      break;
    }
  }
}

static void apply_layer_redo(struct undo_frame *f)
{
  switch(f->type)
  {
    case POS_FRAME:
    {
      struct layer_undo_pos_frame *current = (struct layer_undo_pos_frame *)f;
      struct layer_undo_pos *replace = &(current->replace);
      struct layer_undo_pos *prev;
      int size = current->prev_size;
      int offset;
      int i;

      for(i = 0; i < size; i++)
      {
        prev = &(current->prev[i]);
        offset = prev->x + (prev->y * current->layer_width);
        current->layer_chars[offset] = replace->chr;
        current->layer_colors[offset] = replace->color;
      }
      break;
    }

    case BLOCK_FRAME:
    {
      struct layer_undo_frame *lf = (struct layer_undo_frame *)f;

      copy_layer_buffer_to_buffer(
       lf->current_chars, lf->current_colors, lf->width, 0,
       lf->layer_chars, lf->layer_colors, lf->layer_width, lf->layer_offset,
       lf->width, lf->height
      );
      break;
    }
  }
}

static void apply_layer_update(struct undo_frame *f)
{
  switch(f->type)
  {
    case POS_FRAME:
    {
      // Shrink alloc to trim unused space.
      struct layer_undo_pos_frame *current = (struct layer_undo_pos_frame *)f;
      int size = current->prev_size;

      current->prev_alloc = size;
      current->prev =
       crealloc(current->prev, size * sizeof(struct layer_undo_pos));
      break;
    }

    case BLOCK_FRAME:
    {
      struct layer_undo_frame *lf = (struct layer_undo_frame *)f;

      copy_layer_buffer_to_buffer(
       lf->layer_chars, lf->layer_colors, lf->layer_width, lf->layer_offset,
       lf->current_chars, lf->current_colors, lf->width, 0,
       lf->width, lf->height
      );
      break;
    }
  }
}

static void apply_layer_clear(struct undo_frame *f)
{
  switch(f->type)
  {
    case POS_FRAME:
    {
      struct layer_undo_pos_frame *current = (struct layer_undo_pos_frame *)f;
      free(current->prev);
      break;
    }

    case BLOCK_FRAME:
    {
      struct layer_undo_frame *current = (struct layer_undo_frame *)f;
      free(current->prev_chars);
      free(current->prev_colors);
      free(current->current_chars);
      free(current->current_colors);
      break;
    }
  }
  free(f);
}

static void add_layer_undo_position(struct undo_frame *f, int x, int y)
{
  struct layer_undo_pos_frame *current = (struct layer_undo_pos_frame *)f;

  struct layer_undo_pos *prev = current->prev;
  int prev_size = current->prev_size;
  int prev_alloc = current->prev_alloc;

  int offset = x + (y * current->layer_width);

  if(prev_size == prev_alloc)
  {
    if(!prev_alloc)
      prev_alloc = 1;

    else
      prev_alloc *= 2;

    prev = crealloc(prev, prev_alloc * sizeof(struct board_undo_pos));
  }

  prev[prev_size].x = x;
  prev[prev_size].y = y;
  prev[prev_size].chr = current->layer_chars[offset];
  prev[prev_size].color = current->layer_colors[offset];

  prev_size++;

  current->prev = prev;
  current->prev_size = prev_size;
  current->prev_alloc = prev_alloc;
}

struct undo_history *construct_layer_undo_history(int max_size)
{
  if(max_size)
  {
    struct undo_history *h = construct_undo_history(max_size);

    h->add_pos_function = add_layer_undo_position;
    h->undo_function = apply_layer_undo;
    h->redo_function = apply_layer_redo;
    h->update_function = apply_layer_update;
    h->clear_function = apply_layer_clear;
    return h;
  }
  return NULL;
}

void add_layer_undo_pos_frame(struct undo_history *h, char *layer_chars,
 char *layer_colors, int layer_width, struct buffer_info *buffer, int x, int y)
{
  if(h)
  {
    struct layer_undo_pos_frame *current =
     cmalloc(sizeof(struct layer_undo_pos_frame));

    add_undo_frame(h, current);
    current->f.type = POS_FRAME;

    current->layer_width = layer_width;
    current->layer_chars = layer_chars;
    current->layer_colors = layer_colors;

    current->replace.chr = buffer->param;
    current->replace.color = buffer->color;

    current->prev = NULL;
    current->prev_alloc = 0;
    current->prev_size = 0;

    // Most places this is used don't want to bother doing this manually,
    // so do it here for the first position.
    add_undo_position(h, x, y);
  }
}

void add_layer_undo_frame(struct undo_history *h, char *layer_chars,
 char *layer_colors, int layer_width, int layer_offset, int width, int height)
{
  if(h)
  {
    struct layer_undo_frame *current =
     cmalloc(sizeof(struct layer_undo_frame));

    add_undo_frame(h, current);
    current->f.type = BLOCK_FRAME;

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

    copy_layer_buffer_to_buffer(
     layer_chars, layer_colors, layer_width, layer_offset,
     current->prev_chars, current->prev_colors, width, 0,
     width, height
    );
  }
}
