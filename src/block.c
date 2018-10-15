/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#include "data.h"
#include "idput.h"
#include "robot.h"
#include "world_struct.h"

static inline void copy_board_to_board_buffer(struct world *mzx_world,
 struct board *src_board, int src_offset, struct board *dest_board,
 int block_width, int block_height, char *buffer_id, char *buffer_color,
 char *buffer_param, char *buffer_under_id, char *buffer_under_color,
 char *buffer_under_param)
{
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;

  int src_width = src_board->board_width;
  int src_skip = src_width - block_width;

  int buffer_offset = 0;

  enum thing src_id;
  int src_param;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      src_id = (enum thing)level_id[src_offset];
      src_param = level_param[src_offset];

      // Storage objects need to be copied to the destination board
      if(is_robot(src_id))
      {
        struct robot *src_robot = src_board->robot_list[src_param];
        src_param = duplicate_robot(mzx_world, dest_board, src_robot,
         0, 0, 1);
      }
      else

      if(is_signscroll(src_id))
      {
        struct scroll *src_scroll = src_board->scroll_list[src_param];
        src_param = duplicate_scroll(dest_board, src_scroll);
      }
      else

      if(src_id == SENSOR)
      {
        struct sensor *src_sensor = src_board->sensor_list[src_param];
        src_param = duplicate_sensor(dest_board, src_sensor);
      }

      // Copy to the buffer
      if(src_param != -1)
      {
        buffer_id[buffer_offset] = src_id;
        buffer_param[buffer_offset] = src_param;
        buffer_color[buffer_offset] = level_color[src_offset];
        buffer_under_id[buffer_offset] = level_under_id[src_offset];
        buffer_under_param[buffer_offset] = level_under_param[src_offset];
        buffer_under_color[buffer_offset] = level_under_color[src_offset];
      }

      // If the storage object failed to allocate, copy under data instead
      else
      {
        buffer_id[buffer_offset] = level_under_id[src_offset];
        buffer_param[buffer_offset] = level_under_param[src_offset];
        buffer_color[buffer_offset] = level_under_color[src_offset];
        buffer_under_id[buffer_offset] = level_under_id[src_offset];
        buffer_under_param[buffer_offset] = level_under_param[src_offset];
        buffer_under_color[buffer_offset] = level_under_color[src_offset];
      }

      src_offset++;
      buffer_offset++;
    }

    src_offset += src_skip;
  }
}

static inline void copy_board_buffer_to_board(
 struct board *dest_board, int dest_offset, int block_width, int block_height,
 char *buffer_id, char *buffer_color, char *buffer_param, char *buffer_under_id,
 char *buffer_under_color, char *buffer_under_param)
{
  char *level_id = dest_board->level_id;
  char *level_param = dest_board->level_param;
  char *level_color = dest_board->level_color;
  char *level_under_id = dest_board->level_under_id;
  char *level_under_param = dest_board->level_under_param;
  char *level_under_color = dest_board->level_under_color;

  int dest_width = dest_board->board_width;
  int dest_skip = dest_width - block_width;

  int buffer_offset = 0;

  enum thing buffer_id_cur;
  enum thing dest_id;
  int dest_param;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_id = (enum thing)level_id[dest_offset];
      buffer_id_cur = (enum thing)buffer_id[buffer_offset];

      if(dest_id != PLAYER)
      {
        dest_param = level_param[dest_offset];

        // Storage objects need to be deleted from the destination board
        if(dest_id == SENSOR)
        {
          clear_sensor_id(dest_board, dest_param);
        }
        else

        if(is_signscroll(dest_id))
        {
          clear_scroll_id(dest_board, dest_param);
        }
        else

        if(is_robot(dest_id))
        {
          clear_robot_id(dest_board, dest_param);
        }

        if(is_robot(buffer_id_cur))
        {
          struct robot *cur_robot =
           dest_board->robot_list[(int)buffer_param[buffer_offset]];

          cur_robot->xpos = dest_offset % dest_width;
          cur_robot->ypos = dest_offset / dest_width;
        }

        // Copy off of the buffer onto the board
        if(buffer_id_cur != PLAYER)
        {
          level_id[dest_offset] = buffer_id_cur;
          level_param[dest_offset] = buffer_param[buffer_offset];
          level_color[dest_offset] = buffer_color[buffer_offset];
          level_under_id[dest_offset] = buffer_under_id[buffer_offset];
          level_under_param[dest_offset] = buffer_under_param[buffer_offset];
          level_under_color[dest_offset] = buffer_under_color[buffer_offset];
        }

        // Unless the buffer contains a player, in which case, copy under data
        else
        {
          level_id[dest_offset] = buffer_under_id[buffer_offset];
          level_param[dest_offset] = buffer_under_param[buffer_offset];
          level_color[dest_offset] = buffer_under_color[buffer_offset];
        }
      }

      dest_offset++;
      buffer_offset++;
    }

    dest_offset += dest_skip;
  }
}

void copy_board_to_board(struct world *mzx_world,
 struct board *src_board, int src_offset,
 struct board *dest_board, int dest_offset,
 int block_width, int block_height)
{
  // While we could not use buffering if the boards are different, this
  // is a bit more complex than layer copying, so use buffers anyway.
  // The different boards case only affects the editor anyway.

  char *buffer_id = cmalloc(block_width * block_height);
  char *buffer_color = cmalloc(block_width * block_height);
  char *buffer_param = cmalloc(block_width * block_height);
  char *buffer_under_id = cmalloc(block_width * block_height);
  char *buffer_under_color = cmalloc(block_width * block_height);
  char *buffer_under_param = cmalloc(block_width * block_height);

  copy_board_to_board_buffer(mzx_world,
   src_board, src_offset, dest_board, block_width, block_height,
   buffer_id, buffer_color, buffer_param, buffer_under_id,
   buffer_under_color, buffer_under_param);

  copy_board_buffer_to_board(
   dest_board, dest_offset, block_width, block_height,
   buffer_id, buffer_color, buffer_param, buffer_under_id,
   buffer_under_color, buffer_under_param);

  free(buffer_id);
  free(buffer_color);
  free(buffer_param);
  free(buffer_under_id);
  free(buffer_under_color);
  free(buffer_under_param);
}

static void copy_layer_to_layer_buffered(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  char *buffer_char = cmalloc(block_width * block_height);
  char *buffer_color = cmalloc(block_width * block_height);
  int buffer_offset = 0;

  int src_skip = src_width - block_width;
  int dest_skip = dest_width - block_width;

  // Copying to the same layer; don't worry about char 32
  int i, i2;

  // Copy layer to buffer
  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      buffer_char[buffer_offset] = src_char[src_offset];
      buffer_color[buffer_offset] = src_color[src_offset];

      src_offset++;
      buffer_offset++;
    }

    src_offset += src_skip;
  }

  buffer_offset = 0;

  // Copy buffer back to layer
  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_char[dest_offset] = buffer_char[buffer_offset];
      dest_color[dest_offset] = buffer_color[buffer_offset];

      dest_offset++;
      buffer_offset++;
    }

    dest_offset += dest_skip;
  }

  free(buffer_char);
  free(buffer_color);
}

static void copy_layer_to_layer_direct(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  int src_skip = src_width - block_width;
  int dest_skip = dest_width - block_width;

  // Copying to a different layer; track char 32s
  int src_char_cur;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      src_char_cur = src_char[src_offset];
      if(src_char_cur != 32)
      {
        dest_char[dest_offset] = src_char_cur;
        dest_color[dest_offset] = src_color[src_offset];
      }
      src_offset++;
      dest_offset++;
    }
    src_offset += src_skip;
    dest_offset += dest_skip;
  }
}

void copy_layer_to_layer(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  // Same pointer? Use a buffer
  if(src_char == dest_char || src_color == dest_color)
  {
    copy_layer_to_layer_buffered(
     src_char, src_color, src_width, src_offset,
     dest_char, dest_color, dest_width, dest_offset,
     block_width, block_height);
  }
  // Otherwise, it's safe to just copy direct
  else
  {
    copy_layer_to_layer_direct(
     src_char, src_color, src_width, src_offset,
     dest_char, dest_color, dest_width, dest_offset,
     block_width, block_height);
  }
}

void copy_board_to_layer(
 struct board *src_board, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  int src_width = src_board->board_width;

  int src_skip = src_width - block_width;
  int dest_skip = dest_width - block_width;

  int src_char;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      src_char = get_id_char(src_board, src_offset);
      if(src_char != 32)
      {
        dest_char[dest_offset] = src_char;
        dest_color[dest_offset] = get_id_color(src_board, src_offset);
      }
      src_offset++;
      dest_offset++;
    }
    src_offset += src_skip;
    dest_offset += dest_skip;
  }
}

void copy_layer_to_board(
 char *src_char, char *src_color, int src_width, int src_offset,
 struct board *dest_board, int dest_offset,
 int block_width, int block_height, enum thing convert_id)
{
  char *level_id = dest_board->level_id;
  char *level_param = dest_board->level_param;
  char *level_color = dest_board->level_color;

  int dest_width = dest_board->board_width;

  int src_skip = src_width - block_width;
  int dest_skip = dest_width - block_width;

  char src_char_cur;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      src_char_cur = src_char[src_offset];
      if(src_char_cur != 32 && level_id[dest_offset] != PLAYER)
      {
        level_id[dest_offset] = (char)convert_id;
        level_param[dest_offset] = src_char_cur;
        level_color[dest_offset] = src_color[src_offset];
      }
      src_offset++;
      dest_offset++;
    }
    src_offset += src_skip;
    dest_offset += dest_skip;
  }
}

#ifdef CONFIG_EDITOR

// This goes here so the block buffer monstrosity can be inlined.

void move_board_block(struct world *mzx_world,
 struct board *src_board, int src_x, int src_y,
 struct board *dest_board, int dest_x, int dest_y,
 int block_width, int block_height,
 int clear_width, int clear_height)
{
  char *buffer_id = cmalloc(block_width * block_height);
  char *buffer_color = cmalloc(block_width * block_height);
  char *buffer_param = cmalloc(block_width * block_height);
  char *buffer_under_id = cmalloc(block_width * block_height);
  char *buffer_under_color = cmalloc(block_width * block_height);
  char *buffer_under_param = cmalloc(block_width * block_height);

  // The source board needs to be cleared, so set that up too.
  // This is pretty much copied from clear_board_block, but I think
  // that's a fair sacrifice.

  // The section of the source board that gets cleared can also be
  // larger than the block being copied due to bounds clipping.

  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;

  int src_width = src_board->board_width;
  int src_skip = src_width - clear_width;

  enum thing src_id;
  int src_param;
  int i, i2;

  int src_offset = src_x + (src_y * src_width);
  int dest_offset = dest_x + (dest_y * dest_board->board_width);

  copy_board_to_board_buffer(mzx_world,
   src_board, src_offset, dest_board, block_width, block_height,
   buffer_id, buffer_color, buffer_param, buffer_under_id,
   buffer_under_color, buffer_under_param);

  // Work around to move the player
  if((mzx_world->player_x >= src_x) &&
   (mzx_world->player_y >= src_y) &&
   (mzx_world->player_x < (src_x + clear_width)) &&
   (mzx_world->player_y < (src_y + clear_height)) &&
   (src_board == dest_board))
  {
    place_player_xy(mzx_world,
     mzx_world->player_x - src_x + dest_x,
     mzx_world->player_y - src_y + dest_y);
  }

  for(i = 0; i < clear_height; i++)
  {
    for(i2 = 0; i2 < clear_width; i2++)
    {
      src_id = (enum thing)level_id[src_offset];
      if(src_id != PLAYER)
      {
        src_param = level_param[src_offset];

        if(src_id == SENSOR)
        {
          clear_sensor_id(src_board, src_param);
        }
        else

        if(is_signscroll(src_id))
        {
          clear_scroll_id(src_board, src_param);
        }
        else

        if(is_robot(src_id))
        {
          clear_robot_id(src_board, src_param);
        }

        level_id[src_offset] = (char)SPACE;
        level_param[src_offset] = 0;
        level_color[src_offset] = 7;
      }

      level_under_id[src_offset] = (char)SPACE;
      level_under_param[src_offset] = 0;
      level_under_color[src_offset] = 7;

      src_offset++;
    }

    src_offset += src_skip;
  }

  copy_board_buffer_to_board(
   dest_board, dest_offset, block_width, block_height,
   buffer_id, buffer_color, buffer_param, buffer_under_id,
   buffer_under_color, buffer_under_param);

  free(buffer_id);
  free(buffer_color);
  free(buffer_param);
  free(buffer_under_id);
  free(buffer_under_color);
  free(buffer_under_param);
}

#endif //CONFIG_EDITOR
