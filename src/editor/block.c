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

/* Block actions */
/* For the selection dialogs that used to be called "block.c", see select.c */

/* All bounds are expected to be checked before being passed here. */

#include "block.h"

#include "../board_struct.h"
#include "../data.h"
#include "../robot.h"

void clear_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  int dest_skip = dest_width - block_width;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_char[dest_offset] = 32;
      dest_color[dest_offset] = 7;
      dest_offset++;
    }

    dest_offset += dest_skip;
  }
}

void clear_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height)
{
  char *level_id = dest_board->level_id;
  char *level_param = dest_board->level_param;
  char *level_color = dest_board->level_color;
  char *level_under_id = dest_board->level_under_id;
  char *level_under_param = dest_board->level_under_param;
  char *level_under_color = dest_board->level_under_color;

  int dest_width = dest_board->board_width;
  int dest_skip = dest_width - block_width;

  enum thing dest_id;
  int dest_param;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_id = (enum thing)level_id[dest_offset];
      if(dest_id != PLAYER)
      {
        dest_param = level_param[dest_offset];

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

        level_id[dest_offset] = (char)SPACE;
        level_param[dest_offset] = 0;
        level_color[dest_offset] = 7;
      }

      level_under_id[dest_offset] = (char)SPACE;
      level_under_param[dest_offset] = 0;
      level_under_color[dest_offset] = 7;

      dest_offset++;
    }

    dest_offset += dest_skip;
  }
}

void flip_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  char *buffer = malloc(sizeof(char) * block_width);

  int start_offset = dest_offset;
  int end_offset = dest_offset + dest_width * (block_height - 1);

  while(start_offset < end_offset)
  {
    memcpy(buffer, dest_char + start_offset,
     block_width);
    memcpy(dest_char + start_offset, dest_char + end_offset,
     block_width);
    memcpy(dest_char + end_offset, buffer,
     block_width);

    memcpy(buffer, dest_color + start_offset,
     block_width);
    memcpy(dest_color + start_offset, dest_color + end_offset,
     block_width);
    memcpy(dest_color + end_offset, buffer,
     block_width);

    start_offset += dest_width;
    end_offset -= dest_width;
  }

  free(buffer);
}

void flip_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height)
{
  char *buffer = malloc(sizeof(char) * block_width);

  char *level_id = dest_board->level_id;
  char *level_color = dest_board->level_color;
  char *level_param = dest_board->level_param;
  char *level_under_id = dest_board->level_under_id;
  char *level_under_color = dest_board->level_under_color;
  char *level_under_param = dest_board->level_under_param;

  int dest_width = dest_board->board_width;

  int start_offset = dest_offset;
  int end_offset = dest_offset + dest_width * (block_height - 1);

  while(start_offset < end_offset)
  {
    memcpy(buffer, level_id + start_offset,
     block_width);
    memcpy(level_id + start_offset, level_id + end_offset,
     block_width);
    memcpy(level_id + end_offset, buffer,
     block_width);

    memcpy(buffer, level_color + start_offset,
     block_width);
    memcpy(level_color + start_offset, level_color + end_offset,
     block_width);
    memcpy(level_color + end_offset, buffer,
     block_width);

    memcpy(buffer, level_param + start_offset,
     block_width);
    memcpy(level_param + start_offset,
     level_param + end_offset, block_width);
    memcpy(level_param + end_offset, buffer,
     block_width);

    memcpy(buffer, level_under_id + start_offset,
     block_width);
    memcpy(level_under_id + start_offset, level_under_id + end_offset,
     block_width);
    memcpy(level_under_id + end_offset, buffer,
     block_width);

    memcpy(buffer, level_under_color + start_offset,
     block_width);
    memcpy(level_under_color + start_offset, level_under_color + end_offset,
     block_width);
    memcpy(level_under_color + end_offset, buffer,
     block_width);

    memcpy(buffer, level_under_param + start_offset,
     block_width);
    memcpy(level_under_param + start_offset, level_under_param + end_offset,
     block_width);
    memcpy(level_under_param + end_offset, buffer,
     block_width);

    start_offset += dest_width;
    end_offset -= dest_width;
  }

  free(buffer);
}

void mirror_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  char t;
  int i;
  int a;
  int b;

  for(i = 0; i < block_height; i++)
  {
    a = dest_offset;
    b = dest_offset + block_width - 1;

    while(a < b)
    {
      t = dest_char[a];
      dest_char[a] = dest_char[b];
      dest_char[b] = t;

      t = dest_color[a];
      dest_color[a] = dest_color[b];
      dest_color[b] = t;

      a++;
      b--;
    }

    dest_offset += dest_width;
  }
}

void mirror_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height)
{
  char *level_id = dest_board->level_id;
  char *level_color = dest_board->level_color;
  char *level_param = dest_board->level_param;
  char *level_under_id = dest_board->level_under_id;
  char *level_under_color = dest_board->level_under_color;
  char *level_under_param = dest_board->level_under_param;

  int dest_width = dest_board->board_width;

  char t;
  int i;
  int a;
  int b;

  for(i = 0; i < block_height; i++)
  {
    a = dest_offset;
    b = dest_offset + block_width - 1;

    while(a < b)
    {
      t = level_id[a];
      level_id[a] = level_id[b];
      level_id[b] = t;

      t = level_color[a];
      level_color[a] = level_color[b];
      level_color[b] = t;

      t = level_param[a];
      level_param[a] = level_param[b];
      level_param[b] = t;

      t = level_under_id[a];
      level_under_id[a] = level_under_id[b];
      level_under_id[b] = t;

      t = level_under_color[a];
      level_under_color[a] = level_under_color[b];
      level_under_color[b] = t;

      t = level_under_param[a];
      level_under_param[a] = level_under_param[b];
      level_under_param[b] = t;

      a++;
      b--;
    }

    dest_offset += dest_width;
  }
}

void paint_layer_block(char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height, int paint_color)
{
  int dest_skip = dest_width - block_width;
  int i;
  int i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_color[dest_offset] = paint_color;
      dest_offset++;
    }

    dest_offset += dest_skip;
  }
}

void copy_layer_buffer_to_buffer(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  int src_skip = src_width - block_width;
  int dest_skip = dest_width - block_width;
  int i, i2;

  // Like a direct layer-to-layer copy, but without the char 32 checks
  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_char[dest_offset] = src_char[src_offset];
      dest_color[dest_offset] = src_color[src_offset];

      src_offset++;
      dest_offset++;
    }

    src_offset += src_skip;
    dest_offset += dest_skip;
  }
}

void move_layer_block(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height,
 int clear_width, int clear_height)
{
  // Similar to copy_layer_to_layer_buffered, but deletes the source
  // after copying to the buffer.

  char *buffer_char = cmalloc(block_width * block_height);
  char *buffer_color = cmalloc(block_width * block_height);

  // Copy source to buffer
  copy_layer_buffer_to_buffer(
   src_char, src_color, src_width, src_offset,
   buffer_char, buffer_color, block_width, 0,
   block_width, block_height);

  // Clear the source
  clear_layer_block(
   src_char, src_color, src_width, src_offset,
   clear_width, clear_height);

  // Copy buffer to destination
  copy_layer_buffer_to_buffer(
   buffer_char, buffer_color, block_width, 0,
   dest_char, dest_color, dest_width, dest_offset,
   block_width, block_height);

  free(buffer_char);
  free(buffer_color);
}
