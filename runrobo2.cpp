/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 2004 Gilead Kutnick
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

//Part two of RUNROBOT.CPP

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "error.h"
#include "intake.h"
#include "edit.h"
#include "graphics.h"
#include "scrdisp.h"
#include "window.h"
#include "sfx.h"
#include "audio.h"
#include "idarray.h"
#include "idput.h"
#include "const.h"
#include "data.h"
#include "counter.h"
#include "game.h"
#include "game2.h"
#include "counter.h"
#include "mzm.h"
#include "sprite.h"
#include "event.h"
#include "robot.h"
#include "world.h"

extern int topindex, backindex;

#define parsedir(a, b, c, d) \
parsedir(mzx_world, a, b, c, d, _bl[0], _bl[1], _bl[2], _bl[3]) \

// Commands per cycle - Exo
int commands = 40;

char *item_to_counter[9] =
{
  "GEMS",
  "AMMO",
  "TIME",
  "SCORE",
  "HEALTH",
  "LIVES",
  "LOBOMBS",
  "HIBOMBS",
  "COINS"
};

// side-effects: mangles the input string
// bleah; this is so unreadable; just a very quick dirty hack
static void magic_load_mod(char *filename)
{
  int mod_name_size = strlen(filename);
  if((mod_name_size > 1) && (filename[mod_name_size - 1] == '*'))
  {
    filename[mod_name_size - 1] = '\000';
    load_mod(filename);
    load_mod("*");
  }
  else
  {
    load_mod(filename);
  }
}

void save_player_position(World *mzx_world, int pos)
{
  mzx_world->pl_saved_x[pos] = mzx_world->player_x;
  mzx_world->pl_saved_y[pos] = mzx_world->player_y;
  mzx_world->pl_saved_board[pos] = mzx_world->current_board_id;
}

void restore_player_position(World *mzx_world, int pos)
{
  target_x = mzx_world->pl_saved_x[pos];
  target_y = mzx_world->pl_saved_y[pos];
  target_board = mzx_world->pl_saved_board[pos];
  target_where = 0;
}

void calculate_blocked(World *mzx_world, int x, int y, int id, int bl[4])
{
  Board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  char *level_id = src_board->level_id;

  if(id)
  {
    int i, offset, new_x, new_y;
    for(i = 0; i < 4; i++)
    {
      new_x = x;
      new_y = y;

      if(!move_dir(src_board, &new_x, &new_y, i))
      {
        offset = new_x + (new_y * board_width);
        // Not edge... blocked?
        if((flags[level_id[offset]] & A_UNDER) != A_UNDER)
          bl[i] = 1;
        else
          bl[i] = 0;
      }
      else
      {
        bl[i] = 1; // Edge is considered blocked
      }
    }
  }
}

int place_at_xy(World *mzx_world, int id, int color, int param, int x, int y)
{
  Board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int offset = x + (y * board_width);
  int new_id = src_board->level_id[offset];

  // Okay, I'm really stabbing at behavior here.
  // Wildcard param, use source but only if id matches and isn't
  // greater than or equal to 122..
  if(param == 256)
  {
    // The param must represent the char for this to happen,
    // nothing else will give.
    if((id_chars[new_id] != 255)  || (id_chars[id] != 255))
      param = 0;
    else
      param = src_board->level_param[offset];
  }

  if(new_id == 122)
  {
    int new_param = src_board->level_param[offset];
    clear_sensor_id(src_board, new_param);
  }
  else

  if((new_id == 126) || (new_id == 125))
  {
    int new_param = src_board->level_param[offset];
    clear_scroll_id(src_board, new_param);
  }
  else

  if((new_id == 123) || (new_id == 124))
  {
    int new_param = src_board->level_param[offset];
    clear_robot_id(src_board, new_param);
  }
  else

  if(new_id == 127)
    return 0; // Don't allow the player to be overwritten

  if(param == 256)
  {
    param = src_board->level_param[offset];
  }

  id_place(mzx_world, x, y, id,
   fix_color(color, src_board->level_color[offset]), param);
  return 1;
}

int place_under_xy(Board *src_board, int id, int color, int param, int x,
 int y)
{
  int board_width = src_board->board_width;
  char *level_under_id = src_board->level_under_id;
  char *level_under_color = src_board->level_under_color;
  char *level_under_param = src_board->level_under_param;

  // This is kinda a new addition, maybe not necessary
  // Dest must be underable, and the source must not be
  if(flags[id] & A_UNDER)
  {
    int offset = x + (y * board_width);
    if(param == 256)
      param = level_under_param[offset];

    color = fix_color(color, level_under_color[offset]);
    if(level_under_id[offset] == 122)
      clear_sensor_id(src_board, level_under_param[offset]);

    // Put it now.
    level_under_id[offset] = id;
    level_under_color[offset] = color;
    level_under_param[offset] = param;

    return 1;
  }

  return 0;
}

int place_dir_xy(World *mzx_world, int id, int color, int param, int x, int y,
 int direction, Robot *cur_robot, int *_bl)
{
  Board *src_board = mzx_world->current_board;

  // Check under
  if(direction == 11)
  {
    return place_under_xy(src_board, id, color, param, x, y);
  }
  else
  {
    int new_x = x;
    int new_y = y;
    direction = parsedir(direction, x, y, cur_robot->walk_dir);

    if((direction >= 1) && (direction <= 4))
    {
      if(!move_dir(src_board, &new_x, &new_y, direction - 1))
      {
        return place_at_xy(mzx_world, id, color, param, new_x, new_y);
      }
    }
  }
}

int place_player_xy(World *mzx_world, int x, int y)
{
  if((mzx_world->player_x != x) || (mzx_world->player_y != y))
  {
    Board *src_board = mzx_world->current_board;
    int offset = x + (y * src_board->board_width);
    int did = src_board->level_id[offset];
    int dparam = src_board->level_param[offset];

    if((did == 123) || (did == 124))
    {
      clear_robot_id(src_board, dparam);
    }
    else

    if((did == 125) || (did == 126))
    {
      clear_scroll_id(src_board, dparam);
    }
    else

    if(did == 122)
    {
      step_sensor(mzx_world, dparam);
    }

    id_remove_top(mzx_world, mzx_world->player_x, mzx_world->player_y);
    id_place(mzx_world, x, y, 127, 0, 0);
    mzx_world->player_x = x;
    mzx_world->player_y = y;

    return 1;
  }

  return 0;
}

void send_at_xy(World *mzx_world, int id, int x, int y, char *label)
{
  Board *src_board = mzx_world->current_board;
  int offset = x + (y * src_board->board_width);
  int d_id = src_board->level_id[offset];

  if((d_id == 123) || (d_id == 124))
  {
    char label_buffer[128];
    tr_msg(mzx_world, label, id, label_buffer);
    send_robot_id(mzx_world, src_board->level_param[offset], label_buffer, 0);
  }
}

int get_random_range(int min_value, int max_value)
{
  int result;
  int difference;

  if(min_value == max_value)
  {
    result = min_value;
  }
  else
  {
    if(max_value > min_value)
      difference = max_value - min_value;
    else
      difference = min_value - max_value;

    if(difference)
      result = (rand() % (difference + 1)) + min_value;
    else
      result = rand();
  }

  return result;
}

int send_self_label_tr(World *mzx_world, char *param, int id)
{
  char label_buffer[128];
  tr_msg(mzx_world, param, id, label_buffer);

  if(send_robot_id(mzx_world, id, label_buffer, 1))
    return 0;
  else
    return 1;
}

void split_colors(int color, int *fg, int *bg)
{
  if(color & 256)
  {
    color ^= 256;
    if(color == 32)
    {
      *fg = 16;
      *bg = 16;
    }
    else

    if(color < 16)
    {
      *bg = 16;
      *fg = color;
    }
    else
    {
      *bg = color - 16;
      *fg = 16;
    }
  }
  else
  {
    *bg = color >> 4;
    *fg = color & 0x0F;
  }
}

int check_at_xy(Board *src_board, int id, int fg, int bg, int param,
 int offset)
{
  int did = src_board->level_id[offset];
  int dcolor = src_board->level_color[offset];
  int dparam = src_board->level_param[offset];

  // Whirlpool matches down
  if((did > 67) && (did <= 70))
    did = 67;

  if((did == id) && (((dcolor & 0x0F) == fg) || (fg == 16)) &&
   (((dcolor >> 4) == bg) || (bg == 16)) &&
   ((dparam == param) || (param == 256)))
  {
    return 1;
  }

  return 0;
}

int check_under_xy(Board *src_board, int id, int fg, int bg, int param,
 int offset)
{
  int did = src_board->level_under_id[offset];
  int dcolor = src_board->level_under_color[offset];
  int dparam = src_board->level_under_param[offset];

  if((did == id) && (((dcolor & 0x0F) == fg) || (fg == 16)) &&
   (((dcolor >> 4) == bg) || (bg == 16)) &&
   ((dparam == param) || (param == 256)))
    return 1;

  return 0;
}

int check_dir_xy(World *mzx_world, int id, int color, int param,
 int x, int y, int direction, Robot *cur_robot, int *_bl)
{
  Board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int fg, bg;
  int offset;

  split_colors(color, &fg, &bg);

  // Check under
  if(direction == 11)
  {
    offset = x + (y * board_width);
    if(check_under_xy(src_board, id, fg, bg, param, offset))
      return 1;

    return 0;
  }
  else
  {
    int new_x = x;
    int new_y = y;
    direction = parsedir(direction, x, y, cur_robot->walk_dir);

    if(!move_dir(src_board, &new_x, &new_y, direction - 1))
    {
      offset = new_x + (new_y * board_width);

      if(check_at_xy(src_board, id, fg, bg, param, offset))
        return 1;
    }
    return 0;
  }
}

void copy_xy_to_xy(World *mzx_world, int src_x, int src_y,
 int dest_x, int dest_y)
{
  Board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int src_offset = src_x + (src_y * board_width);
  int dest_offset = dest_x + (dest_y * board_width);
  int src_id = src_board->level_id[src_offset];

  // Cannot copy from player to player
  if(src_id != 127)
  {
    int src_param = src_board->level_param[src_offset];

    // Duplicate robot
    if((src_id == 123) || (src_id == 124))
    {
      Robot *src_robot = src_board->robot_list[src_param];
      src_param = duplicate_robot(src_board, src_robot,
       dest_x, dest_y);
    }
    else

    // Duplicate scroll
    if((src_id == 125) || (src_id == 126))
    {
      Scroll *src_scroll = src_board->scroll_list[src_param];
      src_param = duplicate_scroll(src_board, src_scroll);
    }
    else

    // Duplicate sensor
    // Duplicate scroll
    if(src_id == 122)
    {
      Sensor *src_sensor = src_board->sensor_list[src_param];
      src_param = duplicate_sensor(src_board, src_sensor);
    }

    // Now perform the copy; this should perform any necessary
    // deletions at the destination as well, and will also disallow
    // overwriting the player.
    if(src_param != -1)
      place_at_xy(mzx_world, src_id, src_board->level_color[src_offset],
       src_param, dest_x, dest_y);
  }
}

void copy_board_to_board_buffer(Board *src_board, int x,
 int y, int width, int height, char *dest_id, char *dest_param,
 char *dest_color, char *dest_under_id, char *dest_under_param,
 char *dest_under_color)
{
  int board_width = src_board->board_width;
  int src_offset = x + (y * board_width);
  int src_skip = board_width - width;
  int dest_offset = 0;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  int src_id, src_param;
  int i, i2;

  for(i = 0; i < height; i++, src_offset += src_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      src_id = level_id[src_offset];
      src_param = level_param[src_offset];
      // Duplicate robot
      if((src_id == 123) || (src_id == 124))
      {
        Robot *src_robot = src_board->robot_list[src_param];
        src_param = duplicate_robot(src_board, src_robot,
         0, 0);
      }
      else

      // Duplicate scroll
      if((src_id == 125) || (src_id == 126))
      {
        Scroll *src_scroll = src_board->scroll_list[src_param];
        src_param = duplicate_scroll(src_board, src_scroll);
      }
      else

      // Duplicate sensor
      // Duplicate scroll
      if(src_id == 122)
      {
        Sensor *src_sensor = src_board->sensor_list[src_param];
        src_param = duplicate_sensor(src_board, src_sensor);
      }

      // Now perform the copy to the buffer sections
      if(src_param != -1)
      {
        dest_id[dest_offset] = src_id;
        dest_param[dest_offset] = src_param;
        dest_color[dest_offset] = level_color[src_offset];
        dest_under_id[dest_offset] = level_under_id[src_offset];
        dest_under_param[dest_offset] = level_under_param[src_offset];
        dest_under_color[dest_offset] = level_under_color[src_offset];
      }
    }
  }
}

void copy_board_buffer_to_board(Board *src_board, int x, int y,
 int width, int height, char *src_id, char *src_param,
 char *src_color, char *src_under_id, char *src_under_param,
 char *src_under_color)
{
  int board_width = src_board->board_width;
  int dest_offset = x + (y * board_width);
  int dest_skip = board_width - width;
  int src_offset = 0;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  int src_id_cur, dest_id, dest_param;
  int i, i2;

  for(i = 0; i < height; i++, dest_offset += dest_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      dest_id = level_id[dest_offset];
      if(dest_id != 127)
      {
        dest_param = level_param[dest_offset];

        if(dest_id == 122)
        {
          clear_sensor_id(src_board, dest_param);
        }
        else

        if((dest_id == 126) || (dest_id == 125))
        {
          clear_scroll_id(src_board, dest_param);
        }
        else

        if((dest_id == 123) || (dest_id == 124))
        {
          clear_robot_id(src_board, dest_param);
        }

        // Now perform the copy from the buffer sections
        src_id_cur = src_id[src_offset];
        if((src_id_cur == 123) || (src_id_cur == 124))
        {
          // Fix robot x/y
          (src_board->robot_list[src_param[src_offset]])->xpos =
           x + i2;
          (src_board->robot_list[src_param[src_offset]])->xpos =
           y + i;
        }

        if(src_id_cur != 127)
        {
          level_id[dest_offset] = src_id_cur;
          level_param[dest_offset] = src_param[src_offset];
          level_color[dest_offset] = src_color[src_offset];
          level_under_id[dest_offset] = src_under_id[src_offset];
          level_under_param[dest_offset] = src_under_param[src_offset];
          level_under_color[dest_offset] = src_under_color[src_offset];
        }
      }
    }
  }
}

void copy_layer_to_buffer(int x, int y, int width, int height,
 char *src_char, char *src_color,  char *dest_char,
 char *dest_color, int layer_width)
{
  int src_offset = x + (y * layer_width);
  int src_skip = layer_width - width;
  int dest_offset = 0;
  int i, i2;

  for(i = 0; i < height; i++, src_offset += src_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      // Now perform the copy to the buffer sections
      dest_char[dest_offset] = src_char[src_offset];
      dest_color[dest_offset] = src_color[src_offset];
    }
  }
}

void copy_buffer_to_layer(int x, int y, int width,
 int height, char *src_char, char *src_color,
 char *dest_char, char *dest_color, int layer_width)
{
  int dest_offset = x + (y * layer_width);
  int dest_skip = layer_width - width;
  int src_offset = 0;
  int i, i2;

  for(i = 0; i < height; i++, dest_offset += dest_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      // Now perform the copy to the buffer sections
      dest_char[dest_offset] = src_char[src_offset];
      dest_color[dest_offset] = src_color[src_offset];
    }
  }
}

void copy_layer_to_layer(int src_x, int src_y,
 int dest_x, int dest_y, int width, int height, char *src_char,
 char *src_color, char *dest_char, char *dest_color, int src_width,
 int dest_width)
{
  int dest_offset = dest_x + (dest_y * dest_width);
  int dest_skip = dest_width - width;
  int src_offset = src_x + (src_y * src_width);
  int src_skip = src_width - width;
  int src_char_cur;
  int i, i2;

  for(i = 0; i < height; i++, dest_offset += dest_skip,
   src_offset += src_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      src_char_cur = src_char[src_offset];
      if(src_char_cur != 32)
      {
        // Now perform the copy, if src != 32
        dest_char[dest_offset] = src_char_cur;
        dest_color[dest_offset] = src_color[src_offset];
      }
    }
  }
}

void copy_board_to_layer(Board *src_board, int x, int y,
 int width, int height, char *dest_char, char *dest_color,
 int dest_width)
{
  int board_width = src_board->board_width;
  int src_offset = x + (y * board_width);
  int src_skip = board_width - width;
  int dest_skip = dest_width - width;
  int dest_offset = 0;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  int src_char;
  int i, i2;

  for(i = 0; i < height; i++, src_offset += src_skip, dest_offset += dest_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      src_char = get_id_char(src_board, src_offset);
      if(src_char != 32)
      {
        dest_char[dest_offset] = get_id_char(src_board, src_offset);
        dest_color[dest_offset] = get_id_color(src_board, src_offset);
      }
    }
  }
}

// Make sure convert ID is a custom_*, otherwise this could get ugly

void copy_layer_to_board(Board *src_board, int x, int y,
 int width, int height, char *src_char, char *src_color,
 int src_width, int convert_id)
{
  int board_width = src_board->board_width;
  int dest_offset = x + (y * board_width);
  int dest_skip = src_board->board_width - width;
  int src_skip = src_width - width;
  int src_offset = 0;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  int src_char_cur;
  int i, i2;

  for(i = 0; i < height; i++, src_offset += src_skip,
   dest_offset += dest_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      src_char_cur = src_char[src_offset];
      if(src_char_cur != 32)
      {
        level_id[dest_offset] = convert_id;
        level_param[dest_offset] = src_char_cur;
        level_color[dest_offset] = src_color[src_offset];
      }
    }
  }
}

void clear_layer_block(int src_x, int src_y, int width,
 int height, char *dest_char, char *dest_color, int dest_width)
{
  int dest_offset = src_x + (src_y * dest_width);
  int dest_skip = dest_width - width;
  int i, i2;

  for(i = 0; i < height; i++, dest_offset += dest_skip)
  {
    for(i2 = 0; i2 < width; i2++, dest_offset++)
    {
      // Now perform the copy to the buffer sections
      dest_char[dest_offset] = 32;
      dest_color[dest_offset] = 7;
    }
  }
}

void clear_board_block(Board *src_board, int x, int y,
 int width, int height)
{
  int board_width = src_board->board_width;
  int dest_offset = x + (y * board_width);
  int dest_skip = board_width - width;
  int src_offset = 0;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  int src_id_cur, dest_id, dest_param;
  int i, i2;

  for(i = 0; i < height; i++, dest_offset += dest_skip)
  {
    for(i2 = 0; i2 < width; i2++, src_offset++,
     dest_offset++)
    {
      dest_id = level_id[dest_offset];
      if(dest_id != 127)
      {
        dest_param = level_param[dest_offset];

        if(dest_id == 122)
        {
          clear_sensor_id(src_board, dest_param);
        }
        else

        if((dest_id == 126) || (dest_id == 125))
        {
          clear_scroll_id(src_board, dest_param);
        }
        else

        if((dest_id == 123) || (dest_id == 124))
        {
          clear_robot_id(src_board, dest_param);
        }

        level_id[dest_offset] = 0;
        level_param[dest_offset] = 0;
        level_color[dest_offset] = 7;
        level_under_id[dest_offset] = 0;
        level_under_param[dest_offset] = 0;
        level_under_color[dest_offset] = 7;
      }
    }
  }
}

void setup_overlay(Board *src_board, int mode)
{
  if(!mode && src_board->overlay_mode)
  {
    // Deallocate overlay
    free(src_board->overlay);
    free(src_board->overlay_color);
  }

  if(!src_board->overlay_mode && mode)
  {
    int board_size = src_board->board_width * src_board->board_height;
    // Allocate an overlay
    src_board->overlay = (char *)malloc(board_size);
    src_board->overlay_color = (char *)malloc(board_size);
    memset(src_board->overlay, 32, board_size);
    memset(src_board->overlay_color, 7, board_size);
  }
  src_board->overlay_mode = mode;
}

void replace_player(World *mzx_world)
{
  Board *src_board = mzx_world->current_board;
  char *level_id = src_board->level_id;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int dx, dy, offset;
  int ldone = 0;

  for(dy = 0, offset = 0; (dy < board_height) && (!ldone); dy++)
  {
    for(dx = 0; dx < board_width; dx++, offset++)
    {
      if(A_UNDER & flags[level_id[offset]])
      {
        // Place the player here
        mzx_world->player_x = dx;
        mzx_world->player_y = dy;
        id_place(mzx_world, mzx_world->player_x, mzx_world->player_y,
         127, 0, 0);
        ldone = 1;
        break;
      }
    }
  }
}

// Run a single robot through a single cycle.
// If id is negative, only run it if status is 2
void run_robot(World *mzx_world, int id, int x, int y)
{
  Board *src_board = mzx_world->current_board;
  Robot *cur_robot = src_board->robot_list[id];
  int i, fade = get_fade_status();
  int cmd; // Command to run
  int lines_run = 0; // For preventing infinite loops
  int gotoed; // Set to 1 if we shouldn't advance cmd since we went to a lbl

  int old_pos; // Old position to verify gotos DID something
  // Whether blocked in a given direction (2 = OUR bullet)
  int _bl[4] = { 0, 0, 0, 0 };
  // New * "" command (changes Built_In_Messages to be sure to display the messages)
  char *program;
  char *cmd_ptr; // Points to current command
  char done = 0; // Set to 1 on a finishing command
  char update_blocked = 0; // Set to 1 to update the _bl[4] array
  char first_cmd = 1; // It is the first cmd.
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;

  if((id < 0) && ((src_board->robot_list[-id])->status != 2))
    return;

  // Reset global prefixes
  mzx_world->first_prefix = 0;
  mzx_world->mid_prefix = 0;
  mzx_world->last_prefix = 0;

  if(id < 0)
  {
    id = -id;
    cur_robot = src_board->robot_list[id];
    cur_robot->xpos = x;
    cur_robot->ypos = y;
    cur_robot->cycle_count = 0;
  }
  else
  {
    int walk_dir = cur_robot->walk_dir;
    // Reset x/y
    cur_robot->xpos = x;
    cur_robot->ypos = y;
    // Update cycle count

    cur_robot->cycle_count++;
    if((cur_robot->cycle_count) < (cur_robot->robot_cycle))
    {
      cur_robot->status = 1;
      return;
    }

    cur_robot->cycle_count = 0;

    // Does program exist?
    if(cur_robot->program_length < 3)
    {
      return; // (nope)
    }
    // Walk?
    if((walk_dir >= 1) && (walk_dir <= 4))
    {
      int move_status = move(mzx_world, x, y, walk_dir - 1,
       1 | 2 | 8 | 16 + (4 * cur_robot->can_lavawalk));

      if(move_status == 3)
      {
        // Send to edge, if no response, then to thud.
        if(send_robot_id(mzx_world, id, "edge", 1))
          send_robot_id(mzx_world, id, "thud", 1);
      }
      else

      if(move_status > 0)
        send_robot_id(mzx_world, id, "thud", 1);

      else
      {
        // Update x/y
        switch(walk_dir)
        {
          case 1:
            y--;
            break;
          case 2:
            y++;
            break;
          case 3:
            x++;
            break;
          case 4:
            x--;
            break;
        }
      }
    }

    if(cur_robot->cur_prog_line == 0)
    {
      cur_robot->status = 1;
      goto breaker; // Inactive
    }
  }

  // Get program location
  program = cur_robot->program;

  // NOW quit if inactive (had to do walk first)
  if(cur_robot->cur_prog_line == 0)
  {
    cur_robot->status = 1;
    goto end_prog;
  }

  // Figure blocked vars (accurate until robot program ends OR a put
  // or change command is issued)

  calculate_blocked(mzx_world, x, y, id, _bl);

  // Update player x/y if necessary
  find_player(mzx_world);

  // Main robot loop

  do
  {
    gotoed = 0;
    old_pos = cur_robot->cur_prog_line;

    // Get ptr to command
    cmd_ptr = program + old_pos + 1;

    // Get command number
    cmd = cmd_ptr[0];
    // Act according to command

    //printf("running cmd %d (id %d) at %d\n", cmd, id, old_pos);

    switch(cmd)
    {
      case 0: // End
      {
        if(first_cmd)
          cur_robot->status = 1;
        goto end_prog;
      }

      case 1: // Die
      {
        clear_robot_id(src_board, id);
        if(id)
        {
          id_remove_top(mzx_world, x, y);
        }
        return;
      }

      case 80: // Die item
      {
        clear_robot_id(src_board, id);
        if(id)
        {
          id_remove_top(mzx_world, x, y);
          place_player_xy(mzx_world, x, y);
        }
        return;
      }

      case 2: // Wait
      {
        int wait_time = parse_param(mzx_world, cmd_ptr + 1, id) & 0xFF;
        if(wait_time == cur_robot->pos_within_line)
          break;
        cur_robot->pos_within_line++;
        if(first_cmd)
          cur_robot->status = 1;

        goto breaker;
      }

      case 3: // Cycle
      {
        cur_robot->robot_cycle = parse_param(mzx_world, cmd_ptr + 1,id);
        done = 1;
        break;
      }

      case 4: // Go dir num
      {
        if(id)
        {
          int direction = parsedir(cmd_ptr[2], x, y, cur_robot->walk_dir);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int num = parse_param(mzx_world, p2, id);
          // Inc. pos. or break
          if(num != cur_robot->pos_within_line)
          {
            cur_robot->pos_within_line++;
            {
              // Parse dir
              direction--;
              if((direction >= 0) && (direction <= 3))
              {
                int move_status = move(mzx_world, x, y, direction,
                 1 | 2 | 8 | 16 | 4 * cur_robot->can_lavawalk);
                if(!move_status)
                {
                  move_dir(src_board, &x, &y, direction);
                }
              }
            }
            goto breaker;
          }
        }
        break;
      }

      case 68: // Go dir label
      {
        if(id)
        {
          // Parse dir
          int direction = parsedir(cmd_ptr[2], x, y, cur_robot->walk_dir);
          direction--;
          if((direction >= 0) && (direction <= 3))
          {
            int move_status = move(mzx_world, x, y, direction,
             1 | 2 | 8 | 16 | 4 * cur_robot->can_lavawalk);
            if(move_status)
            {
              // blocked- send to label
              char *p2 = next_param_pos(cmd_ptr + 1);
              gotoed = send_self_label_tr(mzx_world,  p2 + 1, id);
            }
            else
            {
              move_dir(src_board, &x, &y, direction);
              // not blocked- make sure only moves once!
              done = 1;
            }
          }
        }
        break;
      }

      case 5: // Walk dir
      {
        if(id)
        {
          int direction = parsedir(cmd_ptr[2], x, y, cur_robot->walk_dir);
          if((direction < 1) || (direction > 4))
          {
            cur_robot->walk_dir = 0;
          }
          else
          {
            cur_robot->walk_dir = direction;
          }
        }
        break;
      }

      case 6: // Become color thing param
      {
        if(id)
        {
          int offset = x + (y * board_width);
          int color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int new_id = parse_param(mzx_world, p2, id);
          int param = parse_param(mzx_world, p2 + 3, id);
          color = fix_color(color, level_color[offset]);
          level_color[offset] = color;
          level_id[offset] = new_id;

          // Param- Only change if not becoming a robot
          if((new_id != 123) && (new_id != 124))
          {
            level_param[offset] = param;
            clear_robot_id(src_board, id);
            // If became a scroll, sensor, or sign...
            if((new_id >= 122) && (new_id <= 127))
              id_remove_top(mzx_world, x, y);
            // Delete "under"? (if became another type of "under")
            if(flags[new_id] & A_UNDER)
            {
              // Became an under, so delete under.
              src_board->level_under_param[offset] = 0;
              src_board->level_under_id[offset] = 0;
              src_board->level_under_color[offset] = 7;
            }
          }
          return;
        }

        // Became a robot.
        break;
      }

      case 7: // Char
      {
        cur_robot->robot_char = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 8: // Color
      {
        if(id)
        {
          int offset = x + (y * board_width);
          level_color[offset] =
           fix_color(parse_param(mzx_world, cmd_ptr + 1, id), level_color[offset]);
        }
        break;
      }

      case 9: // Gotoxy
      {
        if(id)
        {
          int new_x = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int new_y = parse_param(mzx_world, p2, id);
          prefix_mid_xy(mzx_world, &new_x, &new_y, x, y);

          if((new_x != x) || (new_y != y))
          {
            // delete at x y
            int offset = x + (y * board_width);
            int new_id = level_id[offset];
            int param = level_param[offset];
            int color = level_color[offset];
            if(place_at_xy(mzx_world, new_id, color, param, new_x, new_y))
              id_remove_top(mzx_world, x, y);
            done = 1;
          }
        }
        break;
      }

      case 10: // set counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = next_param_pos(cmd_ptr + 1);
        char src_buffer[128];
        char dest_buffer[128];
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        // Setting a string
        if(is_string(dest_buffer))
        {
          // Is it a non-immediate
          if(*src_string)
          {
            // Translate into src buffer
            tr_msg(mzx_world, src_string + 1, id, src_buffer);

            if(is_string(src_buffer))
            {
              // Is it another string?
              get_string(mzx_world, src_buffer, id, src_buffer);
            }
          }
          else
          {
            // Set it to immediate representation
            sprintf(src_buffer, "%d", parse_param(mzx_world, src_string, id));
          }
          set_string(mzx_world, dest_buffer, src_buffer, id);
        }
        else
        {
          // Set to counter
          int value;
          mzx_world->special_counter_return = NONE;
          value = parse_param(mzx_world, src_string, id);
          if(mzx_world->special_counter_return != NONE)
          {
            gotoed = set_counter_special(mzx_world,
             mzx_world->special_counter_return, dest_buffer, value, id);

            // Swapped? Get out of here
            if(mzx_world->swapped)
              return;

            program = cur_robot->program;
            cmd_ptr = program + cur_robot->cur_prog_line;
          }
          else
          {
            set_counter(mzx_world, dest_buffer, value, id);
          }
        }
        break;
      }

      case 11: // inc counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = next_param_pos(cmd_ptr + 1);
        char src_buffer[128];
        char dest_buffer[128];
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        // Incrementing a string
        if(is_string(dest_buffer))
        {
          // Must be a non-immediate
          if(*src_string)
          {
            // Translate into src buffer
            tr_msg(mzx_world, src_string + 1, id, src_buffer);

            if(is_string(src_buffer))
            {
              // Is it another string? Grab it
              get_string(mzx_world, src_buffer, id, src_buffer);
            }
            // Set it
            inc_string(mzx_world, dest_buffer, src_buffer, id);
          }
        }
        else
        {
          // Set to counter
          int value = parse_param(mzx_world, src_string, id);
          inc_counter(mzx_world, dest_buffer, value, id);
        }
        break;
      }

      case 12: // dec counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = next_param_pos(cmd_ptr + 1);
        char src_buffer[128];
        char dest_buffer[128];
        tr_msg(mzx_world, dest_string, id, dest_buffer);
        int value = parse_param(mzx_world, src_string, id);

        // Decrementing a string
        if(is_string(dest_buffer))
        {
          // Set it to immediate representation
          dec_string_int(mzx_world, dest_buffer, value, id);
        }
        else
        {
          // Set to counter
          dec_counter(mzx_world, dest_buffer, value, id);
        }
        break;
      }

      case 16: // if c?n l
      {
        int difference = 0;
        char *dest_string = cmd_ptr + 2;
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *src_string = p2 + 3;
        int comparison = parse_param(mzx_world, p2, id);
        char src_buffer[128];
        char dest_buffer[128];
        int success = 0;

        if(is_string(dest_string))
        {
          tr_msg(mzx_world, dest_string, id, dest_buffer);
          // Get a pointer to the dest string
          get_string(mzx_world, dest_buffer, id, dest_buffer);
          // Is it immediate?
          if(*src_string)
          {
            tr_msg(mzx_world, src_string + 1, id, src_buffer);
            if(is_string(src_buffer))
            {
              get_string(mzx_world, src_buffer, id, src_buffer);
            }
          }
          else
          {
            sprintf(src_buffer, "%d", parse_param(mzx_world, src_string, id));
          }

          difference = strcasecmp(dest_buffer, src_buffer);
        }
        else
        {
          int dest_value = parse_param(mzx_world, cmd_ptr + 1, id);
          int src_value = parse_param(mzx_world, src_string, id);
          difference = dest_value - src_value;
        }

        switch(comparison)
        {
          case 0:
          {
            if(!difference)
              success = 1;
            break;
          }

          case 1:
          {
            if(difference < 0)
              success = 1;
            break;
          }

          case 2:
          {
            if(difference > 0)
              success = 1;
            break;
          }

          case 3:
          {
            if(difference >= 0)
              success = 1;
            break;
          }

          case 4:
          {
            if(difference <= 0)
              success = 1;
            break;
          }

          case 5:
          {
            if(difference)
              success = 1;
            break;
          }
        }

        if(success)
        {
          char *p3 = next_param_pos(p2 + 3);
          gotoed = send_self_label_tr(mzx_world,  p3 + 1, id);
        }

        break;
      }

      case 18: // if condition label
      case 19: // if not cond. label
      {
        int condition = parse_param(mzx_world, cmd_ptr + 1, id);
        int direction = condition >> 8;
        int success = 0;
        condition &= 0xFF;
        direction = parsedir(direction, x, y, cur_robot->walk_dir);

        switch(condition)
        {
          case 0: // Walking dir
          {
            if(direction < 5)
            {
              if(cur_robot->walk_dir == direction)
                success = 1;
              break;
            }
            else

            if(direction == 14)
            {
              if(cur_robot->walk_dir == 0)
                success = 1;
            }
            else

            // assumed anydir
            if(cur_robot->walk_dir != 0)
              success = 1;
            break;
          }

          case 1: // swimming
          {
            if(id)
            {
              int offset = x + (y * board_width);
              int under = level_under_id[offset];

              if((under > 19) && (under < 25))
                success = 1;
            }
            break;
          }

          case 2: // firewalking
          {
            if(id)
            {
              int offset = x + (y * board_width);
              int under = level_under_id[offset];

              if((under == 26) || (under == 63))
                success = 1;
            }
            break;
          }

          case 3: // touching dir
          {
            if(id)
            {
              int new_x;
              int new_y;

              if((direction >= 1) && (direction <= 4))
              {
                // Is player in dir t2?
                new_x = x;
                new_y = y;
                if(!move_dir(src_board, &new_x, &new_y, direction - 1))
                {
                  if((mzx_world->player_x == new_x) &&
                   (mzx_world->player_y == new_y))
                    success = 1;
                }
              }
              else
              {
                int i;
                // either anydir or nodir
                // is player touching at all?
                for(i = 0; i < 4; i++)
                {
                  // try all dirs
                  new_x = x;
                  new_y = y;
                  if(!move_dir(src_board, &new_x, &new_y, i))
                  {
                    if((mzx_world->player_x == new_x) &&
                     (mzx_world->player_y == new_y))
                    success = 1;
                  }
                }

              // We want NODIR though, so reverse success
              if((direction == 14) || (direction == 0))
                success ^= 1;
              }
            }
            break;
          }

          case 4: // Blocked dir
          {
            int new_bl[4];
            int i;

            // If REL PLAYER or REL COUNTERS, use special code
            switch(mzx_world->mid_prefix)
            {
              case 2:
              {
                // Give an ID of -1 to throw it off from not
                // allowing global robot.
                calculate_blocked(mzx_world, mzx_world->player_x,
                 mzx_world->player_y, -1, new_bl);
                update_blocked = 1;
                break;
              }

              case 3:
              {
                // Give an ID of -1 to throw it off from not
                // allowing global robot.
                calculate_blocked(mzx_world,
                 get_counter(mzx_world, "XPOS", 0),
                 get_counter(mzx_world, "YPOS", 0), -1, new_bl);
                update_blocked = 1;
                break;
              }

              default:
              {
                // Use the blocked list that's already there
                memcpy(new_bl, _bl, sizeof(int) * 4);
              }
            }

            if((direction >= 1) && (direction <= 4))
            {
              success = new_bl[direction - 1];
            }
            else
            {
              // either anydir or nodir
              // is blocked any dir at all?
              success = new_bl[0] | new_bl[1] | new_bl[2] | new_bl[3];
              // success = 1 for anydir

              // We want NODIR though, so reverse success

              if((direction == 14) || (direction == 0))
                success ^= 1;
            }
            break;
          }

          case 5: // Aligned
          {
            if(id)
            {
              if((mzx_world->player_x == x) || (mzx_world->player_y == y))
                success = 1;
            }
            break;
          }

          case 6: // AlignedNS
          {
            if(id)
            {
              if(mzx_world->player_x == x)
                success = 1;
            }
            break;
          }

          case 7: // AlignedEW
          {
            if(id)
            {
              if(mzx_world->player_y == y)
                success = 1;
            }
            break;
          }

          case 8: // LASTSHOT dir (only 1 - 4 accepted)
          {
            if(id)
            {
              if((direction >= 1) && (direction <= 4))
              {
                if(direction == cur_robot->last_shot_dir)
                  success = 1;
              }
            }
            break;
          }

          case 9: // LASTTOUCH dir (only 1 - 4 accepted)
          {
            if(id)
            {
              direction = parsedir(direction, x, y, cur_robot->walk_dir);
              if((direction >= 1) && (direction <= 4))
              {
                if(direction == cur_robot->last_touch_dir)
                  success = 1;
              }
            }
            break;
          }

          case 10: // RTpressed
          {
            success = get_key_status(keycode_SDL, SDLK_RIGHT) > 0;
            break;
          }

          case 11: // LTpressed
          {
            success = get_key_status(keycode_SDL, SDLK_LEFT) > 0;
            break;
          }

          case 12: // UPpressed
          {
            success = get_key_status(keycode_SDL, SDLK_UP) > 0;
            break;
          }

          case 13: // dnpressed
          {
            success = get_key_status(keycode_SDL, SDLK_DOWN) > 0;
            break;
          }

          case 14: // sppressed
          {
            success = get_key_status(keycode_SDL, SDLK_SPACE);
            break;
          }

          case 15: // delpressed
          {
            success = get_key_status(keycode_SDL, SDLK_DELETE);
            break;
          }

          case 16: // musicon
          {
            // FIXME - globalize?
            if(music_on)
              success = 1;
            break;
          }

          case 17: // soundon
          {
            // FIXME - globalize?
            if(sfx_on)
              success = 1;
            break;
          }
        }

        if(cmd == 19)
          success ^= 1;     // Reverse truth if NOT is present

        if(success)
        {
          // jump
          gotoed = send_self_label_tr(mzx_world, cmd_ptr + 5, id);
        }
        break;
      }

      case 20: // if any thing label
      {
        // Get foreg/backg allowed in fb/bg
        int offset, did, dcolor, dparam;
        int color = parse_param(mzx_world, cmd_ptr + 1, id); // Color
        int fg, bg;
        char *p2 = next_param_pos(cmd_ptr + 1);
        int check_id = parse_param(mzx_world, p2, id); // Thing
        char *p3 = next_param_pos(p2);
        // Param
        int check_param = parse_param(mzx_world, p3, id);

        split_colors(color, &fg, &bg);

        for(offset = 0; offset < (board_width * board_height); offset++)
        {
          if(check_at_xy(src_board, check_id, fg, bg, check_param, offset))
          {
            char *p4 = next_param_pos(p3);
            gotoed = send_self_label_tr(mzx_world,  p4 + 1, id);
          }
        }

        break;
      }

      case 21: // if no thing label
      {
        // Get foreg/backg allowed in fb/bg
        int offset, did, dcolor, dparam;
        int color = parse_param(mzx_world, cmd_ptr + 1, id); // Color
        int fg, bg;
        char *p2 = next_param_pos(cmd_ptr + 1);
        int check_id = parse_param(mzx_world, p2, id); // Thing
        char *p3 = next_param_pos(p2);
        // Param
        int check_param = parse_param(mzx_world, p3, id);

        split_colors(color, &fg, &bg);

        for(offset = 0; offset < (board_width * board_height); offset++)
        {
          if(check_at_xy(src_board, check_id, fg, bg, check_param, offset))
            break;
        }

        if(offset == (board_width * board_height))
        {
          char *p4 = next_param_pos(p3);
          gotoed = send_self_label_tr(mzx_world,  p4 + 1, id);
        }

        break;
      }

      case 22: // if thing dir
      {
        if(id)
        {
          int check_color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int check_id = parse_param(mzx_world, p2, id);
          char *p3 = next_param_pos(p2);
          int check_param = parse_param(mzx_world, p3, id);
          char *p4 = next_param_pos(p3);
          int direction = parse_param(mzx_world, p4, id);

          if(check_dir_xy(mzx_world, check_id, check_color,
           check_param, x, y, direction, cur_robot, _bl))
          {
            char *p5 = next_param_pos(p4);
            gotoed = send_self_label_tr(mzx_world, p5 + 1, id);
          }
        }
        break;
      }

      case 23: // if NOT thing dir
      {
        if(id)
        {
          int check_color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int check_id = parse_param(mzx_world, p2, id);
          char *p3 = next_param_pos(p2);
          int check_param = parse_param(mzx_world, p3, id);
          char *p4 = next_param_pos(p3);
          int direction = parse_param(mzx_world, p4, id);

          if(!check_dir_xy(mzx_world, check_id, check_color,
           check_param, x, y, direction, cur_robot, _bl))
          {
            char *p5 = next_param_pos(p4);
            gotoed = send_self_label_tr(mzx_world, p5 + 1, id);
          }
        }
        break;
      }

      case 24: // if thing x y
      {
        int check_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int check_id = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int check_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int check_x = parse_param(mzx_world, p4, id);
        char *p5 = next_param_pos(p4);
        int check_y = parse_param(mzx_world, p5, id);
        int fg, bg;
        int offset;

        if(check_id == 98)
        {
          int ret;
          int check_start = 0;

          prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);

          if(check_color == 288)
            check_param = mzx_world->sprite_num;

          if(check_param == 256)
          {
            int i;
            for(i = check_color; i < MAX_SPRITES; i++)
            {
              if(sprite_at_xy(mzx_world->sprite_list[i], check_x,
               check_y)) break;
            }
            if(i == MAX_SPRITES)
            {
              i = -1;
              ret = 0;
            }
            else
            {
              mzx_world->sprite_num = i;
              ret = 1;
            }
          }
          else
          {
            ret = sprite_at_xy(mzx_world->sprite_list[check_param],
             check_x, check_y);
          }

          if(ret)
          {
            char *p6 = next_param_pos(p5);
            gotoed = send_self_label_tr(mzx_world, p6 + 1, id);
          }
        }
        else

        // Check collision detection for sprite - Exo

        if(check_id == 99)
        {
          Sprite *check_sprite;

          int ret;
          if(check_param == 256)
            check_param = mzx_world->sprite_num;

          check_sprite = mzx_world->sprite_list[check_param];

          if(check_color == 288)
          {
            check_x += check_sprite->x;
            check_y += check_sprite->y;
          }

          prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);
          offset = check_x + (check_y * board_width);

          ret = sprite_colliding_xy(mzx_world, check_sprite,
           check_x, check_y);

          if(ret)
          {
            char *p6 = next_param_pos(p5);
            gotoed = send_self_label_tr(mzx_world, p6 + 1, id);
          }
        }
        else
        {
          prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);
          offset = check_x + (check_y * board_width);

          split_colors(check_color, &fg, &bg);
          if(check_at_xy(src_board, check_id, fg, bg, check_param,
           offset))
          {

            char *p6 = next_param_pos(p5);
            gotoed = send_self_label_tr(mzx_world, p6 + 1, id);
          }
        }
        break;
      }

      case 25: // if at x y label
      {
        if(id)
        {
          int check_x = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int check_y = parse_param(mzx_world, p2, id);

          prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);
          if((check_x == x) && (check_y == y))
          {
            char *p3 = next_param_pos(p2);
            gotoed = send_self_label_tr(mzx_world, p3 + 1, id);
          }
        }
        break;
      }

      case 26: // if dir of player is thing, "label"
      {
        int direction = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int check_color = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int check_id = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int check_param = parse_param(mzx_world, p4, id);

        if(check_dir_xy(mzx_world, check_id, check_color,
         check_param, mzx_world->player_x, mzx_world->player_y,
         direction, cur_robot, _bl))
        {
          char *p5 = next_param_pos(p4);
          gotoed = send_self_label_tr(mzx_world, p5 + 1, id);
        }
        break;
      }

      case 27: // double c
      {
        char dest_buffer[128];
        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        mul_counter(mzx_world, dest_buffer, 2, id);
        break;
      }

      case 28: // half c
      {
        char dest_buffer[128];
        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        div_counter(mzx_world, dest_buffer, 2, id);
        break;
      }

      case 29: // Goto
      {
        gotoed = send_self_label_tr(mzx_world, cmd_ptr + 2, id);
        break;
      }

      case 30: // Send robot label
      {
        char robot_name_buffer[128];
        char label_buffer[128];
        char *p2 = next_param_pos(cmd_ptr + 1);
        tr_msg(mzx_world, cmd_ptr + 2, id, robot_name_buffer);
        tr_msg(mzx_world, p2 + 1, id, label_buffer);

        send_robot(mzx_world, robot_name_buffer, label_buffer, 0);
        // Did the position get changed? (send to self)
        if(old_pos != cur_robot->cur_prog_line)
          gotoed = 1;

        break;
      }

      case 31: // Explode
      {
        if(id)
        {
          int offset = x + (y * board_width);
          level_param[offset] =
           (parse_param(mzx_world, cmd_ptr + 1, id) - 1) * 16;
          level_id[offset] = 38;
        }
        clear_robot_id(src_board, id);

        return;
      }

      case 32: // put thing dir
      {
        if(id)
        {
          int put_color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int put_id = parse_param(mzx_world, p2, id);
          char *p3 = next_param_pos(p2);
          int put_param = parse_param(mzx_world, p3, id);
          char *p4 = next_param_pos(p3);
          int direction = parse_param(mzx_world, p4, id);

          place_dir_xy(mzx_world, put_id, put_color, put_param, x, y,
           direction, cur_robot, _bl);
          update_blocked = 1;
        }
        break;
      }

      case 33: // Give # item
      {
        int amount = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int item_number = *(p2 + 1);
        inc_counter(mzx_world, item_to_counter[item_number], amount, 0);
        break;
      }

      case 34: // Take # item
      {
        int amount = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int item_number = *(p2 + 1);
        int success = 0;

        if(get_counter(mzx_world, item_to_counter[item_number], 0) >= amount)
        {
          dec_counter(mzx_world, item_to_counter[item_number], amount, 0);
          success = 1;
        }

        break;
      }

      case 35: // Take # item "label"
      {
        int amount = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int item_number = *(p2 + 1);
        int success = 0;

        if(get_counter(mzx_world, item_to_counter[item_number], 0) >= amount)
        {
          dec_counter(mzx_world, item_to_counter[item_number], amount, 0);
          success = 1;
        }
        else
        {
          gotoed = send_self_label_tr(mzx_world,  p2 + 4, id);
        }

        break;
      }

      case 36: // Endgame
      {
        set_counter(mzx_world, "LIVES", 0, 0);
        set_counter(mzx_world, "HEALTH", 0, 0);
        break;
      }

      case 37: // Endlife
      {
        set_counter(mzx_world, "HEALTH", 0, 0);
        break;
      }

      // FIXME - Get sound system working, of course...
      case 38: // Mod
      {
        char mod_name_buffer[128];
        tr_msg(mzx_world, cmd_ptr + 2, id, mod_name_buffer);
        magic_load_mod(mod_name_buffer);
        strcpy(src_board->mod_playing, mod_name_buffer);
        strcpy(mzx_world->real_mod_playing, mod_name_buffer);
        volume_mod(src_board->volume);
        break;
      }

      case 39: // sam
      {
        char sam_name_buffer[128];
        int frequency = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        tr_msg(mzx_world, p2 + 1, id, sam_name_buffer);
        play_sample(frequency, sam_name_buffer);
        break;
      }

      case 40: // Volume
      {
        int volume = parse_param(mzx_world, cmd_ptr + 1, id);
        src_board->volume = volume;
        src_board->volume_target = volume;
        volume_mod(volume);
        break;
      }

      case 41: // End mod
      {
        end_mod();
        src_board->mod_playing[0] = 0;
        mzx_world->real_mod_playing[0] = 0;
        break;
      }

      case 42: // End sam
      {
        end_sample();
        break;
      }

      case 43: // Play notes
      {
        play_str(cmd_ptr + 2, 0);
        break;
      }

      case 44: // End play
      {
        clear_sfx_queue();
        break;
      }

      // FIXME - This probably needs a different implementation
      case 45: // wait play "str"
      {
        int index_dif = topindex - backindex;
        if(index_dif < 0)
         index_dif = topindex + (NOISEMAX - backindex);

        if(index_dif > 10)
          goto breaker;

        play_str(cmd_ptr + 2, 0);

        break;
      }

      case 46: // wait play
      {
        int index_dif = topindex - backindex;
        if(index_dif < 0)
         index_dif = topindex + (NOISEMAX - backindex);

        if(index_dif > 10)
          goto breaker;

        break;
      }

      case 48: // sfx num
      {
        int sfx_num = parse_param(mzx_world, cmd_ptr + 1, id);
        play_sfx(mzx_world, sfx_num);
        break;
      }

      case 49: // play sfx notes
      {
        if(!is_playing())
          play_str(cmd_ptr + 2, 0);

        break;
      }

      case 50: // open
      {
        if(id)
        {
          int direction = parse_param(mzx_world, cmd_ptr + 1, id);
          parsedir(direction, x, y, cur_robot->walk_dir);
          if((direction >= 1) & (direction <= 4))
          {
            int new_x = x;
            int new_y = y;
            if(!move_dir(src_board, &new_x, &new_y, direction - 1))
            {
              int new_offset = new_x + (new_y * board_width);
              int new_id = level_id[new_offset];
              if((new_id == 41) || (new_id == 47))
              {
                // Become pushable for right now
                int offset = x + (y * board_width);
                int old_id = level_id[offset];
                level_id[offset] = 123;
                grab_item(mzx_world, new_offset, 0);
                // Find the robot
                if(level_id[offset] != 123)
                {
                  int i;
                  for(i = 0; i < 4; i++)
                  {
                    new_x = x;
                    new_y = y;
                    if(!move_dir(src_board, &new_x, &new_y, i))
                    {
                      // Not edge... robot?
                      new_offset = new_x + (new_y * board_width);
                      if((level_id[new_offset] == 123) &&
                       (level_param[new_offset] == id))
                      {
                        offset = new_offset;
                        x = new_x;
                        y = new_y;
                        break;
                      }
                    }
                  }
                }
                level_id[offset] = old_id;
                update_blocked = 1;
              }
            }
          }
        }
        break;
      }

      case 51: // Lockself
      {
        cur_robot->is_locked = 1;
        break;
      }

      case 52: // Unlockself
      {
        cur_robot->is_locked = 0;
        break;
      }

      case 53: // Send DIR "label"
      {
        if(id)
        {
          int send_x = x;
          int send_y = y;
          int direction = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);

          if((direction >= 1) && (direction <= 4))
          {
            if(!move_dir(src_board, &send_x, &send_y, direction - 1))
            {
              send_at_xy(mzx_world, id, send_x, send_y, p2 + 1);
              // Did the position get changed? (send to self)
              if(old_pos != cur_robot->cur_prog_line)
                gotoed = 1;
            }
          }
        }
        break;
      }

      case 54: // Zap label num
      {
        char label_buffer[128];
        char *p2 = next_param_pos(cmd_ptr + 1);
        int num_times = parse_param(mzx_world, p2, id);
        int i;

        tr_msg(mzx_world, cmd_ptr + 2, id, label_buffer);
        for(i = 0; i < num_times; i++)
        {
          if(!zap_label(cur_robot, label_buffer))
            break;
        }
        break;
      }

      case 55: // Restore label num
      {
        char label_buffer[128];
        char *p2 = next_param_pos(cmd_ptr + 1);
        int num_times = parse_param(mzx_world, p2, id);
        int i;

        tr_msg(mzx_world, cmd_ptr + 2, id, label_buffer);
        for(i = 0; i < num_times; i++)
        {
          if(!restore_label(cur_robot, label_buffer))
            break;
        }
        break;
      }

      case 56: // Lockplayer
      {
        src_board->player_ns_locked = 1;
        src_board->player_ew_locked = 1;
        src_board->player_attack_locked = 1;
        break;
      }

      case 57: // unlockplayer
      {
        src_board->player_ns_locked = 0;
        src_board->player_ew_locked = 0;
        src_board->player_attack_locked = 0;
        break;
      }

      case 58: // lockplayer ns
      {
        src_board->player_ns_locked = 1;
        break;
      }

      case 59: // lockplayer ew
      {
        src_board->player_ew_locked = 1;
        break;
      }

      case 60: // lockplayer attack
      {
        src_board->player_attack_locked = 1;
        break;
      }

      case 61: // move player dir
      case 62: // move pl dir "label"
      {
        int direction = parse_param(mzx_world, cmd_ptr + 1, id);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);
        if((direction >= 1) && (direction <= 4))
        {
          int old_x = mzx_world->player_x;
          int old_y = mzx_world->player_y;
          int old_board = mzx_world->current_board_id;
          int old_target = target_where;
          // Have to fix vars and move to next command NOW, in case player
          // is send to another screen!
          mzx_world->first_prefix = 0;
          mzx_world->mid_prefix = 0;
          mzx_world->last_prefix = 0;
          cur_robot->pos_within_line = 0;
          cur_robot->cur_prog_line += program[cur_robot->cur_prog_line] + 2;

          if(!program[cur_robot->cur_prog_line])
            cur_robot->cur_prog_line = 0;
          cur_robot->cycle_count = 0;
          cur_robot->xpos = x;
          cur_robot->ypos = y;

          // Move player
          move_player(mzx_world, direction - 1);
          if((mzx_world->player_x == old_x) && (mzx_world->player_y == old_y) &&
           (mzx_world->current_board_id == old_board) && (cmd == 62) &&
           (target_where == old_target))
          {
            char *p2 = next_param_pos(cmd_ptr + 1);
            gotoed = send_self_label_tr(mzx_world,  p2 + 1, id);
            goto breaker;
          }
          return;
        }
        break;
      }

      case 63: // Put player x y
      {
        int put_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int put_y = parse_param(mzx_world, p2, id);

        prefix_mid_xy(mzx_world, &put_x, &put_y, x, y);

        if(place_player_xy(mzx_world, put_x, put_y))
        {
          done = 1;
          if((mzx_world->player_x == x) && (mzx_world->player_y == y))
          {
            return;
          }
        }
        break;
      }

      case 66: // if player x y
      {
        int check_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int check_y = parse_param(mzx_world, p2, id);
        prefix_mid_xy(mzx_world, &check_x, &check_y, x, y);

        if((check_x == mzx_world->player_x) && (check_y == mzx_world->player_y))
        {
          char *p3 = next_param_pos(p2);
          gotoed = send_self_label_tr(mzx_world, p3 + 1, id);
        }
        break;
      }

      case 67: // put player dir
      {
        if(id)
        {
          int put_dir = parse_param(mzx_world, cmd_ptr + 1, id);
          put_dir = parsedir(put_dir, x, y, cur_robot->walk_dir);

          if((put_dir >= 1) && (put_dir <= 4))
          {
            int put_x = x;
            int put_y = y;
            if(!move_dir(src_board, &put_x, &put_y, put_dir - 1))
            {
              if(place_player_xy(mzx_world, put_x, put_y))
                done = 1;
            }
          }
        }
        break;
      }

      case 69: // rotate cw
      {
        if(id)
        {
          rotate(mzx_world, x, y, 0);
          // Figure blocked vars
          update_blocked = 1;
        }
        break;
      }

      case 70: // rotate ccw
      {
        if(id)
        {
          rotate(mzx_world, x, y, 1);
          // Figure blocked vars
          update_blocked = 1;
          break;
        }
      }

      case 71: // switch dir dir
      {
        if(id)
        {
          int src_x = x;
          int dest_x = x;
          int src_y = y;
          int dest_y = y;
          int dest_dir = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int src_dir = parse_param(mzx_world, p2, id);
          src_dir = parsedir(src_dir, x, y, cur_robot->walk_dir);
          dest_dir = parsedir(dest_dir, x, y, cur_robot->walk_dir);

          if((src_dir >= 1) && (src_dir <= 4) && (dest_dir >= 1) &&
           (dest_dir <= 4))
          {
            int move_status = move_dir(src_board, &src_x, &src_y, src_dir - 1);
            move_status |= move_dir(src_board, &dest_x, &dest_y, dest_dir - 1);
            if(!move_status)
            {
              // Switch src_x, src_y with dest_x, dest_y
              // The nice thing is that nothing gets deleted,
              // nothing gets copied.
              if((src_x != dest_x) || (src_y != dest_y))
              {
                int src_offset = src_x + (src_y * board_width);
                int dest_offset = dest_x + (dest_y * board_width);
                int cp_id = level_id[src_offset];
                int cp_param = level_param[src_offset];
                int cp_color = level_color[src_offset];
                level_id[src_offset] = level_id[dest_offset];
                level_param[src_offset] = level_param[dest_offset];
                level_color[src_offset] = level_color[dest_offset];
                level_id[dest_offset] = cp_id;
                level_param[dest_offset] = cp_param;
                level_color[dest_offset] = cp_color;
                // Figure blocked vars
                update_blocked = 1;
              }
            }
          }
        }
        break;
      }

      case 72: // Shoot
      {
        if(id)
        {
          int direction = parsedir(cmd_ptr[2], x, y, cur_robot->walk_dir);
          if((direction >= 1) && (direction <= 4))
          {
            if(!(_bl[direction] & 2))
            {
              // Block
              shoot(mzx_world, x, y, direction - 1, cur_robot->bullet_type);
              if(_bl[direction])
                _bl[direction] = 3;
            }
          }
        }
        break;
      }

      case 73: // laybomb
      {
        if(id)
        {
          int direction = parse_param(mzx_world, cmd_ptr + 1, id);
          place_dir_xy(mzx_world, 37, 8, 0, x, y, direction,
           cur_robot, _bl);
          update_blocked = 1;
        }
        break;
      }

      case 74: // laybomb high
      {
        if(id)
        {
          int direction = parse_param(mzx_world, cmd_ptr + 1, id);
          place_dir_xy(mzx_world, 37, 8, 128, x, y, direction,
           cur_robot, _bl);
          update_blocked = 1;
        }
        break;
      }

      case 75: // shoot missile
      {
        if(id)
        {
          int direction = parse_param(mzx_world, cmd_ptr + 1, id);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);
          if((direction >= 1) && (direction <= 4))
          {
            shoot_missile(mzx_world, x, y, direction - 1);
            // Figure blocked vars
            update_blocked = 1;
          }
        }
        break;
      }

      case 76: // shoot seeker
      {
        if(id)
        {
          int direction = parse_param(mzx_world, cmd_ptr + 1, id);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);
          if((direction >= 1) && (direction <= 4))
          {
            shoot_seeker(mzx_world, x, y, direction - 1);
            // Figure blocked vars
            update_blocked = 1;
          }
        }
        break;
      }

      case 77: // spit fire
      {
        if(id)
        {
          int direction = parse_param(mzx_world, cmd_ptr + 1, id);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);
          if((direction >= 1) && (direction <= 4))
          {
            shoot_fire(mzx_world, x, y, direction - 1);
            // Figure blocked vars
            update_blocked = 1;
          }
        }
        break;
      }

      case 78: // lazer wall
      {
        if(id)
        {
          int direction = parse_param(mzx_world, cmd_ptr + 1, id);
          direction = parsedir(direction, x, y, cur_robot->walk_dir);

          if((direction >= 1) && (direction <= 4))
          {
            char *p2 = next_param_pos(cmd_ptr + 1);
            int duration = parse_param(mzx_world, p2, id);
            shoot_lazer(mzx_world, x, y, direction - 1, duration,
             level_color[x + (y * board_width)]);
            // Figure blocked vars
            update_blocked = 1;
          }
        }
        break;
      }

      case 79: // put at xy
      {
        // Defer initialization of color until later because it might be
        // an MZM name string instead.
        int put_color;
        char *p2 = next_param_pos(cmd_ptr + 1);
        int put_id = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int put_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int put_x = parse_param(mzx_world, p4, id);
        char *p5 = next_param_pos(p4);
        int put_y = parse_param(mzx_world, p5, id);

        // MZM image file
        if((put_id == 100) && *(cmd_ptr + 1) && (*(cmd_ptr + 2) == '@'))
        {
          int dest_width, dest_height;
          char mzm_name_buffer[128];
          // "Type" must be 0, 1, or 2; board, overlay, or vlayer
          put_param %= 3;

          if(put_param == 2)
          {
            // Use vlayer dimensions
            dest_width = mzx_world->vlayer_width;
            dest_height = mzx_world->vlayer_height;
          }
          else
          {
            dest_width = src_board->board_width;
            dest_height = src_board->board_height;
          }

          prefix_mid_xy_var(mzx_world, &put_x, &put_y, x, y,
           dest_width, dest_height);

          tr_msg(mzx_world, cmd_ptr + 3, id, mzm_name_buffer);
          load_mzm(mzx_world, mzm_name_buffer, put_x, put_y, put_param);
        }
        else

        // Sprite
        if(put_id == 98)
        {
          put_color = parse_param(mzx_world, cmd_ptr + 1, id);
          if(put_param == 256)
            put_param = mzx_world->sprite_num;

          prefix_mid_xy(mzx_world, &put_x, &put_y, x, y);

          plot_sprite(mzx_world, mzx_world->sprite_list[put_param],
           put_color, put_x, put_y);
        }
        else
        {
          put_color = parse_param(mzx_world, cmd_ptr + 1, id);
          prefix_mid_xy(mzx_world, &put_x, &put_y, x, y);
          place_at_xy(mzx_world, put_id, put_color, put_param, put_x, put_y);
          // Still alive?
          if((put_x == x) && (put_y == y))
            return;

          update_blocked = 1;
        }
        break;
      }

      case 81: // Send x y "label"
      {
        if(id)
        {
          int send_x = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int send_y = parse_param(mzx_world, p2, id);
          char *p3 = next_param_pos(p2);
          prefix_mid_xy(mzx_world, &send_x, &send_y, x, y);

          send_at_xy(mzx_world, id, send_x, send_y, p3 + 1);
          // Did the position get changed? (send to self)
          if(old_pos != cur_robot->cur_prog_line)
            gotoed = 1;
        }
        break;
      }

      case 82: // copyrobot ""
      {
        Robot *dest_robot;
        int first, last;
        char robot_name_buffer[128];
        // Get the robot name
        tr_msg(mzx_world, cmd_ptr + 2, id, robot_name_buffer);

        // Check the global robot
        if(!strcasecmp(mzx_world->global_robot.robot_name, robot_name_buffer))
        {
          replace_robot(src_board, &(mzx_world->global_robot), id);
        }
        else
        {
          // Find the first robot that matches
          if(find_robot(src_board, robot_name_buffer, &first, &last))
          {
            if(first != id)
            {
              replace_robot(src_board, src_board->robot_list_name_sorted[first], id);
            }
          }
        }
        cur_robot = src_board->robot_list[id];
        goto breaker;
      }

      case 83: // copyrobot x y
      {
        int copy_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int copy_y = parse_param(mzx_world, p2, id);
        int offset, did;

        prefix_mid_xy(mzx_world, &copy_x, &copy_y, x, y);
        offset = copy_x + (copy_y * board_width);
        did = level_id[offset];

        if((did == 123) || (did == 124))
        {
          replace_robot(src_board,
           src_board->robot_list[level_param[offset]], id);
        }
        cur_robot = src_board->robot_list[id];
        goto breaker;
      }

      case 84: // copyrobot dir
      {
        int copy_dir = parse_param(mzx_world, cmd_ptr + 1, id);
        int offset, did;
        int copy_x = x;
        int copy_y = y;

        copy_dir = parsedir(copy_dir, x, y, cur_robot->walk_dir);

        if((copy_dir >= 1) && (copy_dir <= 4))
        {
          if(!move_dir(src_board, &copy_x, &copy_y, copy_dir - 1))
          {
            offset = copy_x + (copy_y * board_width);
            did = level_id[offset];

            if((did == 123) || (did == 124))
            {
              replace_robot(src_board,
               src_board->robot_list[level_param[offset]], id);
            }
          }
        }
        cur_robot = src_board->robot_list[id];
        goto breaker;
      }

      case 85: // dupe self dir
      {
        if(id)
        {
          int duplicate_dir = parse_param(mzx_world, cmd_ptr + 1, id);
          int dest_id;
          int duplicate_id, duplicate_color, offset;
          int duplicate_x = x;
          int duplicate_y = y;

          duplicate_dir = parsedir(duplicate_dir, x, y, cur_robot->walk_dir);

          if((duplicate_dir >= 1) && (duplicate_dir <= 4))
          {
            offset = x + (y * board_width);
            duplicate_color = level_color[offset];
            duplicate_id = level_id[offset];

            if(!move_dir(src_board, &duplicate_x, &duplicate_y,
             duplicate_dir - 1))
            {
              dest_id = duplicate_robot(src_board, cur_robot,
               duplicate_x, duplicate_y);

              if(dest_id != -1)
                place_at_xy(mzx_world, duplicate_id, duplicate_color,
                 dest_id, duplicate_x, duplicate_y);
            }
          }
        }
        break;
      }

      case 86: // dupe self xy
      {
        int duplicate_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int duplicate_y = parse_param(mzx_world, p2, id);
        int dest_id;
        int duplicate_id, duplicate_color, offset;

        prefix_mid_xy(mzx_world, &duplicate_x, &duplicate_y, x, y);

        if((duplicate_x != x) || (duplicate_y != y))
        {
          if(id)
          {
            offset = x + (y * board_width);
            duplicate_color = level_color[offset];
            duplicate_id = level_id[offset];
          }
          else
          {
            duplicate_color = 7;
            duplicate_id = 124;
          }

          dest_id = duplicate_robot(src_board, cur_robot, duplicate_x, duplicate_y);

          if(dest_id != -1)
          {
            place_at_xy(mzx_world, duplicate_id, duplicate_color, dest_id,
             duplicate_x, duplicate_y);
          }
        }
        else
        {
          cur_robot->cur_prog_line = 1;
          gotoed = 1;
        }
        break;
      }

      case 87: // bulletn
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        bullet_char[0] = new_char;
        bullet_char[4] = new_char;
        bullet_char[8] = new_char;
        break;
      }

      case 88: // bullets
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        bullet_char[1] = new_char;
        bullet_char[5] = new_char;
        bullet_char[9] = new_char;
        break;
      }

      case 89: // bullete
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        bullet_char[2] = new_char;
        bullet_char[6] = new_char;
        bullet_char[10] = new_char;
        break;
      }

      case 90: // bulletw
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        bullet_char[3] = new_char;
        bullet_char[7] = new_char;
        bullet_char[11] = new_char;
        break;
      }

      case 91: // givekey col
      {
        int key_num = parse_param(mzx_world, cmd_ptr + 1, id) & 0x0F;
        give_key(mzx_world, key_num);
        break;
      }

      case 92: // givekey col "l"
      {
        int key_num = parse_param(mzx_world, cmd_ptr + 1, id) & 0x0F;
        if(give_key(mzx_world, key_num))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case 93: // takekey col
      {
        int key_num = parse_param(mzx_world, cmd_ptr + 1, id) & 0x0F;
        take_key(mzx_world, key_num);
        break;
      }

      case 94: // takekey col "l"
      {
        int key_num = parse_param(mzx_world, cmd_ptr + 1, id) & 0x0F;
        if(take_key(mzx_world, key_num))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case 95: // inc c r
      {
        char dest_buffer[128];
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *p3 = next_param_pos(p2);
        int min_value = parse_param(mzx_world, p2, id);
        int max_value = parse_param(mzx_world, p3, id);
        int result;

        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        result = get_random_range(min_value, max_value);
        inc_counter(mzx_world, dest_buffer, result, id);
        break;
      }

      case 96: // dec c r
      {
        char dest_buffer[128];
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *p3 = next_param_pos(p2);
        int min_value = parse_param(mzx_world, p2, id);
        int max_value = parse_param(mzx_world, p3, id);
        int result;

        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        result = get_random_range(min_value, max_value);
        dec_counter(mzx_world, dest_buffer, result, id);
        break;
      }

      case 97: // set c r
      {
        char dest_buffer[128];
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *p3 = next_param_pos(p2);
        int min_value = parse_param(mzx_world, p2, id);
        int max_value = parse_param(mzx_world, p3, id);
        int result;

        tr_msg(mzx_world, cmd_ptr + 2, id, dest_buffer);
        result = get_random_range(min_value, max_value);
        set_counter(mzx_world, dest_buffer, result, id);
        break;
      }

      case 98: // Trade givenum givetype takenum taketype poorlabel
      {
        int give_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int give_type = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int take_num = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int take_type = parse_param(mzx_world, p4, id);
        int amount_held = get_counter(mzx_world, item_to_counter[take_type], 0);

        if(amount_held < take_num)
        {
          char *p5 = next_param_pos(p4);
          gotoed = send_self_label_tr(mzx_world, p5 + 1, id);
        }
        else
        {
          dec_counter(mzx_world, item_to_counter[take_type], take_num, 0);
          inc_counter(mzx_world, item_to_counter[give_type], give_num, 0);
        }
        break;
      }

      case 99: // Send DIR of player "label"
      {
        int send_x = mzx_world->player_x;
        int send_y = mzx_world->player_y;
        int direction = parse_param(mzx_world, cmd_ptr + 1, id);
        direction = parsedir(direction, send_x, send_y, cur_robot->walk_dir);

        if(!move_dir(src_board, &send_x, &send_y, direction - 1))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          send_at_xy(mzx_world, id, send_x, send_y, p2 + 1);
          // Did the position get changed? (send to self)
          if(old_pos != cur_robot->cur_prog_line)
            gotoed = 1;
        }
        break;
      }

      case 100: // put thing dir of player
      {
        if(id)
        {
          int put_color = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int put_id = parse_param(mzx_world, p2, id);
          char *p3 = next_param_pos(p2);
          int put_param = parse_param(mzx_world, p3, id);
          char *p4 = next_param_pos(p3);
          int direction = parse_param(mzx_world, p4, id);

          place_dir_xy(mzx_world, put_id, put_color, put_param,
           mzx_world->player_x, mzx_world->player_y, direction,
           cur_robot, _bl);
          update_blocked = 1;
        }
        break;
      }

      case 101: // /"dirs"
      case 232: // Persistent go [str]
      {
        if(id)
        {
          int current_char, direction;
          // get next dir char, if none, next cmd
          cur_robot->pos_within_line++;
          current_char = cmd_ptr[cur_robot->pos_within_line + 1];
          // current_char must be 'n', 's', 'w', 'e' or 'i'
          switch(current_char)
          {
            case 'n':
            case 'N':
              direction = 0;
              break;
            case 's':
            case 'S':
              direction = 1;
              break;
            case 'e':
            case 'E':
              direction = 2;
              break;
            case 'w':
            case 'W':
              direction = 3;
              break;
            case 'i':
            case 'I':
              direction = -1;
              break;
            default:
              direction = -2;
          }

          if(direction >= 0)
          {
            if((move(mzx_world, x, y, direction,
             1 | 2 | 8 | 16 | 4 * cur_robot->can_lavawalk)) &&
             (cmd == 232))
              cur_robot->pos_within_line--; // persistent...

            move_dir(src_board, &x, &y, direction);
          }

          if(direction == -2)
            goto next_cmd;
          else
            goto breaker;
        }
        break;
      }

      case 102: // Mesg
      {
        char message_buffer[128];
        tr_msg(mzx_world, cmd_ptr + 2, id, message_buffer);
        set_mesg_direct(src_board, message_buffer);
        break;
      }

      case 103:
      case 104:
      case 105:
      case 116:
      case 117:
      {
        // Box messages!
        char label_buffer[128];
        int cur_prog_line = cur_robot->cur_prog_line;
        int next_cmd = 0;

        robot_box_display(mzx_world, cmd_ptr - 1, label_buffer, id);

        // Move to end of all box mesg cmds.
        do
        {
          cur_prog_line += program[cur_prog_line] + 2;
          // At next line- check type
          if(!program[cur_prog_line])
            goto end_prog;

          next_cmd = program[cur_prog_line + 1];
        } while((next_cmd == 47) || ((next_cmd >= 103) && (next_cmd <= 106))
         || (next_cmd == 116) || (next_cmd == 117));

        cur_robot->cur_prog_line = cur_prog_line;

        // Send label
        if(label_buffer[0])
        {
          gotoed = send_self_label_tr(mzx_world, label_buffer, id);
        }

        goto breaker;
      }

      case 107: // comment-do nothing!
      {
        // (unless first char is a @)
        if(cmd_ptr[2] == '@')
        {
          char name_buffer[128];
          tr_msg(mzx_world, cmd_ptr + 3, id, name_buffer);
          name_buffer[15] = 0;

          if(id)
          {
            change_robot_name(src_board, cur_robot, name_buffer);
          }
          else
          {
            // Special code for global robot
            strcpy(cur_robot->robot_name, name_buffer);
          }
        }
        break;
      }

      case 109: // teleport
      {
        char board_dest_buffer[128];
        char *p2 = next_param_pos(cmd_ptr + 1);
        int teleport_x = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int teleport_y = parse_param(mzx_world, p3, id);
        int current_board_id = mzx_world->current_board_id;
        int board_id;

        tr_msg(mzx_world, cmd_ptr + 2, id, board_dest_buffer);
        board_id = find_board(mzx_world, board_dest_buffer);

        if(board_id != NO_BOARD)
        {
          mzx_world->current_board = mzx_world->board_list[board_id];
          prefix_mid_xy(mzx_world, &teleport_x, &teleport_y,
           x, y);
          // And switch back
          mzx_world->current_board =
           mzx_world->board_list[current_board_id];
          target_board = board_id;
          target_x = teleport_x;
          target_y = teleport_y;
          target_where = -1;
          done = 1;
        }

        break;
      }

      case 110: // scrollview dir num
      {
        int direction = parse_param(mzx_world, cmd_ptr + 1, id);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);

        if((direction >= 1) && (direction <= 4))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          int num = parse_param(mzx_world, p2, id);

          switch(direction)
          {
            case 1:
              src_board->scroll_y -= num;
              break;
            case 2:
              src_board->scroll_y += num;
              break;
            case 3:
              src_board->scroll_x += num;
              break;
            case 4:
              src_board->scroll_x -= num;
              break;
          }
        }
        break;
      }

      case 111: // input string
      {
        char input_buffer[128];

        m_hide();
        save_screen();
        if(fade)
        {
          clear_screen(32, 7);
          insta_fadein();
        }
        draw_window_box(3, 11, 77, 14, EC_DEBUG_BOX, EC_DEBUG_BOX_DARK,
         EC_DEBUG_BOX_CORNER, 1, 1);
        tr_msg(mzx_world, cmd_ptr + 2, id, input_buffer);
        write_string(input_buffer, 5, 12, EC_DEBUG_LABEL, 1);
        m_show();
        src_board->input_string[0] = 0;

        intake(src_board->input_string, 70, 5, 13, 15, 1, 0);
        if(fade)
          insta_fadeout();

        restore_screen();
        src_board->input_size = strlen(src_board->input_string);
        src_board->num_input = atoi(src_board->input_string);
        break;
      }

      case 112: // If string "" "l"
      {
        char cmp_buffer[128];
        tr_msg(mzx_world, cmd_ptr + 2, id, cmp_buffer);
        if(!strcasecmp(cmp_buffer, src_board->input_string))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case 113: // If string not "" "l"
      {
        char cmp_buffer[128];
        tr_msg(mzx_world, cmd_ptr + 2, id, cmp_buffer);
        if(strcasecmp(cmp_buffer, src_board->input_string))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case 114: // If string matches "" "l"
      {
        // compare
        char cmp_buffer[128];
        char *input_string = src_board->input_string;
        tr_msg(mzx_world, cmd_ptr + 2, id, cmp_buffer);
        char current_char;
        char cmp_char;
        int cmp_len = strlen(cmp_buffer);
        int i = 0;

        for(i = 0; i < cmp_len; i++)
        {
          cmp_char = cmp_buffer[i];
          current_char = input_string[i];

          if((cmp_char >= 'a') && (cmp_char <= 'z'))
            cmp_char -= 32;
          if((current_char >= 'a') && (current_char <= 'z'))
            current_char -= 32;

          if(cmp_char == '?') continue;
          if(cmp_char == '#')
          {
            if((current_char < '0') || (current_char > '9'))
              break;
            continue;
          }
          if(cmp_char == '_')
          {
            if((current_char < 'A') || (current_char > 'Z'))
              break;
            continue;
          }
          if(cmp_char == '*')
          {
            i = 1000000;
            break;
          }

          if(cmp_char != current_char) break;
        }

        if(i >= cmp_len)
        {
          // Matches
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }
        break;
      }

      case 115: // Player char
      {
        int new_char = parse_param(mzx_world, cmd_ptr + 1, id);
        int i;

        for(i = 0; i < 4; i++)
        {
          player_char[i] = new_char;
        }
        break;
      }

      case 118: // move all thing dir
      {
        int move_color = parse_param(mzx_world, cmd_ptr + 1, id); // Color
        char *p2 = next_param_pos(cmd_ptr + 1);
        int move_id = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int move_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int move_dir = parse_param(mzx_world, p4, id);
        int lx, ly;
        int fg, bg;
        int offset;
        int dcolor;

        parsedir(move_dir, x, y, cur_robot->walk_dir);
        split_colors(move_color, &fg, &bg);

        if((move_dir >= 1) && (move_dir <= 4))
        {
          move_dir--;
          // if dir is 1 or 4, search top to bottom.
          // if dir is 2 or 3, search bottom to top.
          if((move_dir == 0) || (move_dir == 3))
          {
            for(ly = 0, offset = 0; ly < board_height; ly++)
            {
              for(lx = 0; lx < board_width; lx++, offset++)
              {
                int dcolor = level_color[offset];
                if((move_id == level_id[offset]) &&
                 ((move_param == 256) || (level_param[offset] == move_param)) &&
                 ((fg == 16) || (fg == (dcolor & 0x0F))) &&
                 ((bg == 16) || (bg == (dcolor >> 4))))
                {
                  move(mzx_world, lx, ly, move_dir, 1 | 2 | 4 | 8 | 16);
                }
              }
            }
          }
          else
          {
            offset = (board_width * board_height) - 1;
            for(ly = board_height - 1; ly >= 0; ly--)
            {
              for(lx = board_width - 1; lx >= 0; lx--, offset--)
              {
                int dcolor = level_color[offset];
                if((move_id == level_id[offset]) &&
                 ((move_param == 256) || (level_param[offset] == move_param)) &&
                 ((fg == 16) || (fg == (dcolor & 0x0F))) &&
                 ((bg == 16) || (bg == (dcolor >> 4))))
                {
                  move(mzx_world, lx, ly, move_dir, 1 | 2 | 4 | 8 | 16);
                }
              }
            }
          }
        }
        done = 1;
        break;
      }

      case 119: // copy x y x y
      {
        int src_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int src_y = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int dest_x = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int dest_y = parse_param(mzx_world, p4, id);

        prefix_first_last_xy(mzx_world, &src_x, &src_y,
         &dest_x, &dest_y, x, y);

        copy_xy_to_xy(mzx_world, src_x, src_y, dest_x, dest_y);
        update_blocked = 1;
        break;
      }

      case 120: // set edge color
      {
        mzx_world->edge_color = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 121: // board dir
      {
        int direction = parse_param(mzx_world, cmd_ptr + 1, id);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);
        if((direction >= 1) && (direction <= 4))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          char board_name_buffer[128];
          int board_number;

          tr_msg(mzx_world, p2 + 1, id, board_name_buffer);
          board_number = find_board(mzx_world, board_name_buffer);

          if(board_number != NO_BOARD)
            src_board->board_dir[direction - 1] = board_number;
        }
        break;
      }

      case 122: // board dir is none
      {
        int direction = parse_param(mzx_world, cmd_ptr + 1, id);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);
        if((direction >= 1) && (direction <= 4))
        {
          src_board->board_dir[direction - 1] = NO_BOARD;
        }
        break;
      }

      case 123: // char edit
      {
        int char_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *next_param = next_param_pos(cmd_ptr + 1);
        char char_buffer[14];
        int i;

        for(i = 0; i < 14; i++)
        {
          char_buffer[i] = parse_param(mzx_world, next_param, id);
          next_param = next_param_pos(next_param);
        }

        ec_change_char(char_num, char_buffer);
        break;
      }

      case 124: // Become push
      {
        if(id)
        {
          level_id[x + (y * board_width)] = 123;
        }
        break;
      }

      case 125: // Become nonpush
      {
        if(id)
        {
          level_id[x + (y * board_width)] = 124;
        }
        break;
      }

      case 126: // blind
      {
        mzx_world->blind_dur = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 127: // firewalker
      {
        mzx_world->firewalker_dur = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 128: // freezetime
      {
        mzx_world->freeze_time_dur = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 129: // slow time
      {
        mzx_world->slow_time_dur = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 130: // wind
      {
        mzx_world->wind_dur = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 131: // avalanche
      {
        int x, y, offset;
        // Adjust the rate for board size - it was hardcoded for 10000
        int placement_rate = 18 * (board_width * board_height) / 10000;

        for(y = 0, offset = 0; y < board_height; y++)
        {
          for(x = 0; x < board_width; x++, offset++)
          {
            int d_flag = flags[level_id[offset]];

            if((d_flag & A_UNDER) && !(d_flag & A_ENTRANCE) &&
             (rand() % placement_rate) == 0)
            {
              id_place(mzx_world, x, y, 8, 7, 0);
            }
          }
        }
        break;
      }

      case 132: // copy dir dir
      {
        if(id)
        {
          int src_dir = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int dest_dir = parse_param(mzx_world, p2, id);
          src_dir = parsedir(src_dir, x, y, cur_robot->walk_dir);
          dest_dir = parsedir(dest_dir, x, y, cur_robot->walk_dir);

          if((src_dir >= 1) && (src_dir <= 4) && (dest_dir >= 1) &&
           (dest_dir <= 4))
          {
            int src_x = x;
            int src_y = y;
            int dest_x = x;
            int dest_y = y;
            if(!move_dir(src_board, &src_x, &src_y, src_dir - 1) &&
             !move_dir(src_board, &dest_x, &dest_y, dest_dir - 1))
            {
              copy_xy_to_xy(mzx_world, src_x, src_y, dest_x, dest_y);
            }
          }
        }
        update_blocked = 1;
        break;
      }

      case 133: // become lavawalker
      {
        cur_robot->can_lavawalk = 1;
        break;
      }

      case 134: // become non lavawalker
      {
        cur_robot->can_lavawalk = 0;
        break;
      }

      case 135: // change color thing param color thing param
      {
        int check_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int check_id = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int check_param = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int put_color = parse_param(mzx_world, p4, id);
        char *p5 = next_param_pos(p4);
        int put_id = parse_param(mzx_world, p5, id);
        char *p6 = next_param_pos(p5);
        int put_param = parse_param(mzx_world, p6, id);
        int check_fg, check_bg, put_fg, put_bg;
        int offset, did, dparam, dcolor, put_x, put_y;

        split_colors(check_color, &check_fg, &check_bg);
        split_colors(put_color, &put_fg, &put_bg);

        // Make sure the change isn't incompatable
        // Only robots (pushable or otherwise) can be changed to
        // robots. The same goes for scrolls/signs and sensors.
        // Players cannot be changed at all

        if(((put_id != 122) || (check_id == 122)) &&
         (((put_id != 123) || ((check_id == 124) || (check_id == 123))) &&
          ((put_id != 124) || ((check_id == 123) || (check_id == 124))) &&
          ((put_id != 125) || (check_id == 126))) &&
          ((put_id != 126) || (check_id == 125)) &&
          ((put_id != 127) && (check_id != 127)))
        {
          // Clear out params for param-less objects,
          // lava, fire, ice, energizer, rotates, or life
          if((put_id == 25) || (put_id == 26) || (put_id == 33) ||
           (put_id == 45) || (put_id == 46) || (put_id == 63) ||
           (put_id == 68))
            put_param = 0;

          // Ignore upper stages for explosions
          if(put_id == 38)
            put_param &= 0xF3;

          // Open door becomes door
          if(check_id == 42)
            check_id = 41;

          // Whirlpool becomes base whirlpool
          if((check_id > 67) && (check_id <= 70))
            check_id = 67;

          // Cannot change param on these
          if(put_id > 122)
            put_param = 256;

          for(offset = 0; offset < (board_width * board_height); offset++)
          {
            did = level_id[offset];
            dparam = level_param[offset];
            dcolor = level_color[offset];

            // open door becomes door
            if(did == 42)
              did = 41;
            // Whirpool becomes base one
            if((did > 67) && (did <= 70))
              did = 67;

            if((did == check_id) && (((dcolor & 0x0F) == check_fg) ||
             (check_fg == 16)) && (((dcolor >> 4) == check_bg) ||
             (check_bg == 16)) && ((dparam == check_param) ||
             (check_param == 256)))
            {
              // Change the color and the ID
              level_color[offset] = fix_color(put_color, dcolor);
              level_id[offset] = put_id;

              if(((did == 123) || (did == 124)) && (put_id != 123) &&
               (put_id != 124))
              {
                // delete robot if not changing to a robot
                clear_robot_id(src_board, dparam);
              }
              else

              if(((did == 125) || (did == 126)) && (put_id != 125) &&
               (put_id != 126))
              {
                // delete scroll if not changing to a scroll/sign
                clear_scroll_id(src_board, dparam);
              }
              else

              if((did == 122) && (put_id != 122))
              {
                // delete sensor if not changing to a sensor
                clear_sensor_id(src_board, dparam);
              }

              if(put_id == 0)
              {
                offs_remove_id(mzx_world, offset);
                // If this LEAVES a space, use given color
                if(level_id[offset] == 0)
                  level_color[offset] = fix_color(put_color, dcolor);
              }
              else
              {
                if(put_param != 256)
                  level_param[offset] = put_param;
              }
            }
          }
          // If we got deleted, exit

          if(id)
          {
            did = level_id[x + (y * board_width)];

            if((did != 123) && (did != 124))
              return;
            update_blocked = 1;
          }
        }

        break;
      }

      case 136: // player color
      {
        int new_color = parse_param(mzx_world, cmd_ptr + 1, id);
        if(get_counter(mzx_world, "INVINCO", 0))
        {
          mzx_world->saved_pl_color =
           fix_color(new_color, mzx_world->saved_pl_color);
        }
        else
        {
          *player_color = fix_color(new_color, *player_color);
        }
        break;
      }

      case 137: // bullet color
      {
        int new_color = parse_param(mzx_world, cmd_ptr + 1, id);
        int i;

        for(i = 0; i < 3; i++)
        {
          bullet_color[i] =
           fix_color(new_color, bullet_color[i]);
        }
        break;
      }

      case 138: // missile color
      {
        missile_color =
         fix_color(parse_param(mzx_world, cmd_ptr + 1, id), missile_color);
        break;
      }

      case 139: // message row
      {
        int b_mesg_row = parse_param(mzx_world, cmd_ptr + 1, id);
        if(b_mesg_row > 24)
          b_mesg_row = 24;
        src_board->b_mesg_row = b_mesg_row;
        break;
      }

      case 140: // rel self
      {
        if(id)
        {
          mzx_world->first_prefix = 1;
          mzx_world->mid_prefix = 1;
          mzx_world->last_prefix = 1;
          lines_run--;
          goto next_cmd_prefix;
        }
        break;
      }

      case 141: // rel player
      {
        mzx_world->first_prefix = 2;
        mzx_world->mid_prefix = 2;
        mzx_world->last_prefix = 2;
        lines_run--;
        goto next_cmd_prefix;
      }

      case 142: // rel counters
      {
        mzx_world->first_prefix = 3;
        mzx_world->mid_prefix = 3;
        mzx_world->last_prefix = 3;
        lines_run--;
        goto next_cmd_prefix;
      }

      case 143: // set id char # to 'c'
      {
        int id_char = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int id_value = parse_param(mzx_world, p2, id);
        if((id_char >= 0) && (id_char <= 454))
        {
          if(id_char == 323)
            missile_color = id_value;
          else
            if((id_char >= 324) && (id_char <= 326))
              bullet_color[id_char - 324] = id_value;
          else
            if((id_char >= 327))
              id_dmg[id_char - 327] = id_value;
          else
            id_chars[id_char] = id_value;
        }
        break;
      }

      // FIXME - how to make this work..?
      case 144: // jump mod order #
      {
        jump_mod(parse_param(mzx_world, cmd_ptr + 1, id));
        break;
      }

      case 145: // ask yes/no
      {
        char question_buffer[128];
        int send_status;

        if(fade)
        {
          clear_screen(32, 7);
          insta_fadein();
        }

        tr_msg(mzx_world, cmd_ptr + 2, id, question_buffer);

        if(!ask_yes_no(mzx_world, question_buffer))
          send_status = send_robot_id(mzx_world, id, "YES", 1);
        else
          send_status = send_robot_id(mzx_world, id, "NO", 1);

        if(!send_status)
          gotoed = 1;

        if(fade)
          insta_fadeout();
        break;
      }

      case 146: // fill health
      {
        set_counter(mzx_world, "HEALTH", mzx_world->health_limit, 0);
        break;
      }

      case 147: // thick arrow dir char
      {
        int dir = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        dir = parsedir(dir, x, y, cur_robot->walk_dir);

        if((dir >= 1) && (dir <= 4))
        {
          id_chars[249 + dir] = parse_param(mzx_world, p2, id);
        }
        break;
      }

      case 148: // thin arrow dir char
      {
        int dir = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        dir = parsedir(dir, x, y, cur_robot->walk_dir);

        if((dir >= 1) && (dir <= 4))
        {
          id_chars[253 + dir] =
           parse_param(mzx_world, p2, id);
        }
        break;
      }

      case 149: // set max health
      {
        mzx_world->health_limit = parse_param(mzx_world, cmd_ptr + 1, id);
        inc_counter(mzx_world, "HEALTH", 0, 0);
        break;
      }

      case 150: // save pos
      {
        save_player_position(mzx_world, 0);
        break;
      }

      case 151: // restore
      {
        restore_player_position(mzx_world, 0);
        done = 1;
        break;
      }

      case 152: // exchange
      {
        restore_player_position(mzx_world, 0);
        save_player_position(mzx_world, 0);
        done = 1;
        break;
      }

      case 153: // mesg col
      {
        int b_mesg_col = parse_param(mzx_world, cmd_ptr + 1, id);
        if(b_mesg_col > 79)
          b_mesg_col = 79;
        src_board->b_mesg_col = b_mesg_col;
        break;
      }

      case 154: // center mesg
      {
        src_board->b_mesg_col = -1;
        break;
      }

      case 155: // clear mesg
      {
        src_board->b_mesg_timer = 0;
        break;
      }

      case 156: // resetview
      {
        src_board->scroll_x = 0;
        src_board->scroll_y = 0;
        break;
      }

      // FIXME - There may be no way to get this to work. It may have to be removed.
      case 157: // modsam freq num
      {
        if(music_on)
        {
          int frequency = parse_param(mzx_world, cmd_ptr + 1, id);
          char *p2 = next_param_pos(cmd_ptr + 1);
          int sam_num = parse_param(mzx_world, p2, id);
          spot_sample(frequency, sam_num);
        }
        break;
      }

      case 159: // scrollbase
      {
        mzx_world->scroll_base_color = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 160: // scrollcorner
      {
        mzx_world->scroll_corner_color = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 161: // scrolltitle
      {
        mzx_world->scroll_title_color = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 162: // scrollpointer
      {
        mzx_world->scroll_pointer_color = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 163: // scrollarrow
      {
        mzx_world->scroll_arrow_color = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 164: // viewport x y
      {
        int viewport_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int viewport_y = parse_param(mzx_world, p2, id);
        int viewport_width = src_board->viewport_width;
        int viewport_height = src_board->viewport_height;

        if((viewport_x + viewport_width) > 80)
          viewport_x = 80 - viewport_width;
        if((viewport_y + viewport_height) > 25)
          viewport_y = 25 - viewport_height;

        src_board->viewport_x = viewport_x;
        src_board->viewport_y = viewport_y;

        break;
      }

      case 165: // viewport width height
      {
        int viewport_width = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int viewport_height = parse_param(mzx_world, p2, id);
        int viewport_x = src_board->viewport_x;
        int viewport_y = src_board->viewport_y;

        if(viewport_width < 1)
          viewport_width = 1;

        if(viewport_height < 1)
          viewport_height = 1;

        if(viewport_width > 80)
          viewport_width = 80;

        if(viewport_height > 25)
          viewport_height = 25;

        if((viewport_x + viewport_width) > 80)
          src_board->viewport_x = 80 - viewport_width;

        if((viewport_y + viewport_height) > 25)
          src_board->viewport_y = 25 - viewport_height;

        src_board->viewport_width = viewport_width;
        src_board->viewport_height = viewport_height;

        break;
      }

      case 168: // save pos #
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        if((pos < 0) || (pos > 7))
          pos = 0;
        save_player_position(mzx_world, pos);
        break;
      }

      case 169: // restore pos #
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        if((pos < 0) || (pos > 7))
          pos = 0;
        restore_player_position(mzx_world, pos);
        done = 1;
        break;
      }

      case 170: // exchange pos #
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        if((pos < 0) || (pos > 7))
          pos = 0;
        restore_player_position(mzx_world, pos);
        save_player_position(mzx_world, pos);
        done = 1;
        break;
      }

      case 171: // restore pos # duplicate self
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        int duplicate_x = mzx_world->player_x;
        int duplicate_y = mzx_world->player_y;
        int duplicate_color, duplicate_id, dest_id;
        int dx, dy, offset, ldone = 0;

        if((pos < 0) || (pos > 7))
          pos = 0;
        restore_player_position(mzx_world, pos);
        // Duplicate the robot to where the player was

        if(id)
        {
          offset = x + (y * board_width);
          duplicate_color = level_color[offset];
          duplicate_id = level_id[offset];
        }
        else
        {
          duplicate_color = 7;
          duplicate_id = 124;
        }

        offset = duplicate_x + (duplicate_y * board_width);
        dest_id = duplicate_robot(src_board, cur_robot, duplicate_x, duplicate_y);

        if(dest_id != -1)
        {
          level_id[offset] = duplicate_id;
          level_color[offset] = duplicate_color;
          level_param[offset] = dest_id;

          replace_player(mzx_world);

          done = 1;
        }
        break;
      }

      case 172: // exchange pos # duplicate self
      {
        int pos = parse_param(mzx_world, cmd_ptr + 1, id) - 1;
        int duplicate_x = mzx_world->player_x;
        int duplicate_y = mzx_world->player_y;
        int duplicate_color, duplicate_id, dest_id;
        int dx, dy, offset, ldone = 0;

        if((pos < 0) || (pos > 7))
          pos = 0;
        restore_player_position(mzx_world, pos);
        save_player_position(mzx_world, pos);
        // Duplicate the robot to where the player was

        if(id)
        {
          offset = x + (y * board_width);
          duplicate_color = level_color[offset];
          duplicate_id = level_id[offset];
        }
        else
        {
          duplicate_color = 7;
          duplicate_id = 124;
        }

        offset = duplicate_x + (duplicate_y * board_width);
        dest_id = duplicate_robot(src_board, cur_robot, duplicate_x, duplicate_y);

        if(dest_id != -1)
        {
          level_id[offset] = duplicate_id;
          level_color[offset] = duplicate_color;
          level_param[offset] = dest_id;

          replace_player(mzx_world);

          done = 1;
        }
        break;
      }

      case 173: // Pl bulletn
      case 174: // Pl bullets
      case 175: // Pl bullete
      case 176: // Pl bulletw
      case 177: // Nu bulletn
      case 178: // Nu bullets
      case 179: // Nu bullete
      case 180: // Nu bulletw
      case 181: // En bulletn
      case 182: // En bullets
      case 183: // En bullete
      case 184: // En bulletw
      {
        // Id' make these all separate, but this is really too convenient..
        bullet_char[cmd - 173] = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 185: // Pl bcolor
      case 186: // Nu bcolor
      case 187: // En bcolor
      {
        bullet_color[cmd - 185] = parse_param(mzx_world, cmd_ptr + 1, id);
        break;
      }

      case 193: // Rel self first
      {
        if(id)
        {
          mzx_world->first_prefix = 5;
          lines_run--;
          goto next_cmd_prefix;
        }
        break;
      }

      case 194: // Rel self last
      {
        if(id)
        {
          mzx_world->last_prefix = 5;
          lines_run--;
          goto next_cmd_prefix;
        }
        break;
      }

      case 195: // Rel player first
      {
        mzx_world->first_prefix = 6;
        lines_run--;
        goto next_cmd_prefix;
      }

      case 196: // Rel player last
      {
        mzx_world->last_prefix = 6;
        lines_run--;
        goto next_cmd_prefix;
      }

      case 197: // Rel counters first
      {
        mzx_world->first_prefix = 7;
        lines_run--;
        goto next_cmd_prefix;
      }

      case 198: // Rel counters last
      {
        mzx_world->last_prefix = 7;
        lines_run--;
        goto next_cmd_prefix;
      }

      case 199: // Mod fade out
      {
        src_board->volume_inc = -8;
        src_board->volume_target = 0;
        break;
      }

      case 200: // Mod fade in
      {
        char name_buffer[128];
        src_board->volume_inc = 8;
        src_board->volume_target = 255;
        tr_msg(mzx_world, cmd_ptr + 2, id, name_buffer);

        magic_load_mod(name_buffer);
        strcpy(src_board->mod_playing, name_buffer);
        strcpy(mzx_world->real_mod_playing, name_buffer);
        src_board->volume = 0;
        volume_mod(0);
        break;
      }

      case 201: // Copy block sx sy width height dx dy
      {
        char *p1 = cmd_ptr + 1;
        char *p2 = next_param_pos(p1);
        char *p3 = next_param_pos(p2);
        char *p4 = next_param_pos(p3);
        char *p5 = next_param_pos(p4);
        char *p6 = next_param_pos(p5);
        int src_x, src_y, dest_x, dest_y;
        int width = parse_param(mzx_world, p3, id);
        int height = parse_param(mzx_world, p4, id);
        int src_width = board_width;
        int src_height = board_height;
        int dest_width = board_width;
        int dest_height = board_height;

        // 0 is board, 1 is overlay, 2 is vlayer
        // src_type cannot be overlay here...
        int src_type = 0, dest_type = 0;
        if((*p1) && (*(p1 + 1) == '#'))
        {
          char src_char_buffer[128];
          src_width = mzx_world->vlayer_width;
          src_height = mzx_world->vlayer_height;
          src_type = 2;
          tr_msg(mzx_world, p1 + 2, id, src_char_buffer);
          src_x = strtol(src_char_buffer, NULL, 10);
        }
        else
        {
          src_x = parse_param(mzx_world, p1, id);
        }

        if((*p2) && (*(p2 + 1) == '#'))
        {
          char src_char_buffer[128];
          src_width = mzx_world->vlayer_width;
          src_height = mzx_world->vlayer_height;
          src_type = 2;
          tr_msg(mzx_world, p2 + 2, id, src_char_buffer);
          src_y = strtol(src_char_buffer, NULL, 10);
        }
        else
        {
          src_y = parse_param(mzx_world, p2, id);
        }

        if(*p5)
        {
          if(*(p5 + 1) == '#')
          {
            dest_width = mzx_world->vlayer_width;
            dest_height = mzx_world->vlayer_height;
            dest_type = 2;
          }
          else

          if(*(p5 + 1) == '+')
          {
            dest_type = 1;
          }
          else

          if(*(p5 + 1) == '@')
          {
            int copy_type = parse_param(mzx_world, p6, id);
            char name_buffer[128];
            tr_msg(mzx_world, p5 + 2, id, name_buffer);
            prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
             src_width, src_height);
            save_mzm(mzx_world , name_buffer, src_x, src_y, width, height,
             src_type, copy_type);
            break;
          }
          else

          if(is_string(p5 + 1))
          {
            char str_buffer[128];
            int t_char = parse_param(mzx_world, p6, id);
            prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
             src_width, src_height);
            tr_msg(mzx_world, p5 + 1, id, str_buffer);
            switch(src_type)
            {
              case 0:
              {
                // Board to string
                load_string_board(mzx_world, str_buffer, width, height, t_char,
                 level_param + src_x + (src_y * board_width), board_width, id);
                break;
              }
              case 2:
              {
                // Vlayer to string
                int vlayer_width = mzx_world->vlayer_width;
                load_string_board(mzx_world, str_buffer, width, height, t_char,
                 mzx_world->vlayer_chars + src_x + (src_y * vlayer_width),
                 vlayer_width, id);
                break;
              }
            }
            break;
          }
        }

        if(*p6)
        {
          if(*(p6 + 1) == '#')
          {
            dest_width = mzx_world->vlayer_width;
            dest_height = mzx_world->vlayer_height;
            dest_type = 2;
          }
          else

          if(*(p6 + 1) == '+')
          {
            dest_type = 1;
          }
        }

        if(dest_type)
        {
          char dest_char_buffer[128];
          tr_msg(mzx_world, p5 + 1, id, dest_char_buffer);
          dest_x = strtol(dest_char_buffer + 1, NULL, 10);
          tr_msg(mzx_world, p6 + 1, id, dest_char_buffer);
          dest_y = strtol(dest_char_buffer + 1, NULL, 10);
        }
        else
        {
          dest_x = parse_param(mzx_world, p5, id);
          dest_y = parse_param(mzx_world, p6, id);
        }

        prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
         src_width, src_height);
        prefix_last_xy_var(mzx_world, &dest_x, &dest_y, x, y,
         dest_width, dest_height);

        // Clip and verify; the prefixers already handled the
        // base coordinates, so just handle the width/height
        if(width < 1)
          width = 1;
        if(height < 1)
          height = 1;
        if((src_x + width) > src_width)
          width = src_width - src_x;
        if((src_y + height) > src_height)
          height = src_height - src_y;
        if((dest_x + width) > dest_width)
          width = dest_width - dest_x;
        if((dest_y + height) > dest_height)
          height = dest_height - dest_y;

        switch((dest_type << 2) | (src_type))
        {
          // FIXME - These buffers are bad. Variable arrays are nice
          // but C99 only. alloca is nice but I can't seem to get it
          // working on mingw..
          // Board to board
          case 0:
          {
            char *id_buffer = (char *)malloc(width * height);
            char *param_buffer = (char *)malloc(width * height);
            char *color_buffer = (char *)malloc(width * height);
            char *under_id_buffer = (char *)malloc(width * height);
            char *under_param_buffer = (char *)malloc(width * height);
            char *under_color_buffer = (char *)malloc(width * height);
            copy_board_to_board_buffer(src_board, src_x, src_y, width,
             height, id_buffer, param_buffer, color_buffer,
             under_id_buffer, under_param_buffer, under_color_buffer);
            copy_board_buffer_to_board(src_board, dest_x, dest_y, width,
             height, id_buffer, param_buffer, color_buffer,
             under_id_buffer, under_param_buffer, under_color_buffer);
            update_blocked = 1;
            free(id_buffer);
            free(param_buffer);
            free(color_buffer);
            free(under_id_buffer);
            free(under_param_buffer);
            free(under_color_buffer);
            break;
          }

          // Vlayer to board
          case 2:
          {
            int vlayer_width = mzx_world->vlayer_width;
            int vlayer_offset = src_x + (src_y * vlayer_width);
            copy_layer_to_board(src_board, dest_x, dest_y, width, height,
             mzx_world->vlayer_chars + vlayer_offset,
             mzx_world->vlayer_colors + vlayer_offset, vlayer_width, 5);
            update_blocked = 1;
            break;
          }

          // Board to overlay
          case 4:
          {
            int overlay_offset = dest_x + (dest_y * board_width);
            copy_board_to_layer(src_board, src_x, src_y, width, height,
             src_board->overlay + overlay_offset,
             src_board->overlay_color + overlay_offset, board_width);
            break;
          }

          // Board to vlayer
          case 8:
          {
            int vlayer_width = mzx_world->vlayer_width;
            int vlayer_offset = dest_x + (dest_y * vlayer_width);
            copy_board_to_layer(src_board, src_x, src_y, width, height,
             mzx_world->vlayer_chars + vlayer_offset,
             mzx_world->vlayer_colors + vlayer_offset, vlayer_width);
            break;
          }

          // Vlayer to vlayer
          case 10:
          {
            char *char_buffer = (char *)malloc(width * height);
            char *color_buffer = (char *)malloc(width * height);
            copy_layer_to_buffer(src_x, src_y, width, height,
             mzx_world->vlayer_chars, mzx_world->vlayer_colors,
             char_buffer, color_buffer, mzx_world->vlayer_width);
            copy_buffer_to_layer(dest_x, dest_y, width, height,
             char_buffer, color_buffer, mzx_world->vlayer_chars,
             mzx_world->vlayer_colors, mzx_world->vlayer_width);
            free(char_buffer);
            free(color_buffer);
            break;
          }
        }
        break;
      }

      case 243: // Copy overlay block sx sy width height dx dy
      {
        char *p1 = cmd_ptr + 1;
        char *p2 = next_param_pos(p1);
        char *p3 = next_param_pos(p2);
        char *p4 = next_param_pos(p3);
        char *p5 = next_param_pos(p4);
        char *p6 = next_param_pos(p5);
        int src_x, src_y, dest_x, dest_y;
        int width = parse_param(mzx_world, p3, id);
        int height = parse_param(mzx_world, p4, id);
        int src_width = board_width;
        int src_height = board_height;
        int dest_width = board_width;
        int dest_height = board_height;

        // 0 is board, 1 is overlay, 2 is vlayer
        // src_type cannot be board here...
        int src_type = 1, dest_type = 1;
        if((*p1) && (*(p1 + 1) == '#'))
        {
          char src_char_buffer[128];
          src_width = mzx_world->vlayer_width;
          src_height = mzx_world->vlayer_height;
          src_type = 2;
          tr_msg(mzx_world, p1 + 2, id, src_char_buffer);
          src_x = strtol(src_char_buffer, NULL, 10);
        }
        else
        {
          src_x = parse_param(mzx_world, p1, id);
        }

        if((*p2) && (*(p2 + 1) == '#'))
        {
          char src_char_buffer[128];
          src_width = mzx_world->vlayer_width;
          src_height = mzx_world->vlayer_height;
          src_type = 2;
          tr_msg(mzx_world, p2 + 2, id, src_char_buffer);
          src_y = strtol(src_char_buffer, NULL, 10);
        }
        else
        {
          src_y = parse_param(mzx_world, p2, id);
        }

        if(*p5)
        {
          if(*(p5 + 1) == '#')
          {
            dest_width = mzx_world->vlayer_width;
            dest_type = 2;
          }
          else

          if(*(p5 + 1) == '+')
          {
            dest_type = 0;
          }
          else

          if(*(p5 + 1) == '@')
          {
            int copy_type = parse_param(mzx_world, p6, id);
            char name_buffer[128];
            tr_msg(mzx_world, p5 + 2, id, name_buffer);
            prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
             src_width, src_height);
            save_mzm(mzx_world , name_buffer, src_x, src_y, width, height,
             src_type, copy_type);
          }
          else

          if(is_string(p5 + 1))
          {
            char str_buffer[128];
            int t_char = parse_param(mzx_world, p6, id);
            tr_msg(mzx_world, p5 + 1, id, str_buffer);
            prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
             src_width, src_height);
            switch(src_type)
            {
              case 1:
              {
                // Overlay to string
                load_string_board(mzx_world, str_buffer, width, height, t_char,
                 src_board->overlay + src_x + (src_y * board_width),
                 board_width, id);
                break;
              }
              case 2:
              {
                // Vlayer to string
                int vlayer_width = mzx_world->vlayer_width;
                load_string_board(mzx_world, str_buffer, width, height, t_char,
                 mzx_world->vlayer_chars + src_x + (src_y * vlayer_width),
                 vlayer_width, id);
                break;
              }
            }
            break;
          }
        }

        if(*p6)
        {
          if(*(p6 + 1) == '#')
          {
            dest_width = mzx_world->vlayer_width;
            dest_type = 2;
          }
          else

          if(*(p6 + 1) == '+')
          {
            dest_type = 0;
          }
        }

        if((dest_type == 0) || (dest_type == 2))
        {
          char dest_char_buffer[128];
          tr_msg(mzx_world, p5 + 2, id, dest_char_buffer);
          dest_x = strtol(dest_char_buffer, NULL, 10);
          tr_msg(mzx_world, p6 + 2, id, dest_char_buffer);
          dest_y = strtol(dest_char_buffer, NULL, 10);
        }
        else
        {
          dest_x = parse_param(mzx_world, p5, id);
          dest_y = parse_param(mzx_world, p6, id);
        }

        prefix_first_xy_var(mzx_world, &src_x, &src_y, x, y,
         src_width, src_height);
        prefix_last_xy_var(mzx_world, &dest_x, &dest_y, x, y,
         dest_width, dest_height);

        // Clip and verify; the prefixers already handled the
        // base coordinates, so just handle the width/height
        if(width < 1)
          width = 1;
        if(height < 1)
          height = 1;
        if((src_x + width) > src_width)
          width = src_width - src_x;
        if((src_y + height) > src_height)
          height = src_height - src_y;
        if((dest_x + width) > dest_width)
          width = dest_width - dest_x;
        if((dest_y + height) > dest_height)
          height = dest_height - dest_y;

        switch((dest_type << 2) | (src_type))
        {
          // Overlay to board
          case 1:
          {
            int overlay_offset = src_x + (src_y * board_width);
            copy_layer_to_board(src_board, dest_x, dest_y, width, height,
             src_board->overlay + overlay_offset,
             src_board->overlay_color + overlay_offset, board_width, 5);
            update_blocked = 1;
            break;
          }

          // FIXME - These buffers are bad. Variable arrays are nice
          // but C99 only. alloca is nice but I can't seem to get it
          // working on mingw..
          // Overlay to overlay
          case 5:
          {
            char *char_buffer = (char *)malloc(width * height);
            char *color_buffer = (char *)malloc(width * height);
            copy_layer_to_buffer(src_x, src_y, width, height,
             src_board->overlay, src_board->overlay_color,
             char_buffer, color_buffer, board_width);
            copy_buffer_to_layer(dest_x, dest_y, width, height,
             char_buffer, color_buffer, src_board->overlay,
             src_board->overlay_color, board_width);
            free(char_buffer);
            free(color_buffer);
            break;
          }

          // Vlayer to overlay
          case 6:
          {
            int vlayer_width = mzx_world->vlayer_width;
            copy_layer_to_layer(src_x, src_y, dest_x, dest_y, width,
             height, mzx_world->vlayer_chars, mzx_world->vlayer_colors,
             src_board->overlay, src_board->overlay_color, vlayer_width,
             board_width);
            break;
          }

          // Overlay to vlayer
          case 9:
          {
            int vlayer_width = mzx_world->vlayer_width;
            copy_layer_to_layer(src_x, src_y, dest_x, dest_y, width,
             height, src_board->overlay, src_board->overlay_color,
             mzx_world->vlayer_chars, mzx_world->vlayer_colors, board_width,
             vlayer_width);
            break;
          }

          // Vlayer to vlayer
          case 10:
          {
            char *char_buffer = (char *)malloc(width * height);
            char *color_buffer = (char *)malloc(width * height);
            copy_layer_to_buffer(src_x, src_y, width, height,
             mzx_world->vlayer_chars, mzx_world->vlayer_colors,
             char_buffer, color_buffer, mzx_world->vlayer_width);
            copy_buffer_to_layer(dest_x, dest_y, width, height,
             char_buffer, color_buffer, mzx_world->vlayer_chars,
             mzx_world->vlayer_colors, mzx_world->vlayer_width);
            free(char_buffer);
            free(color_buffer);
            break;
          }
        }
        break;
      }

      case 202: // Clip input
      {
        char *input_string = src_board->input_string;
        int input_size = src_board->input_size;
        int i = 0;

        // Chop up to and through first section of whitespace.
        // First, until non space or end
        if(input_size)
        {
          do
          {
            if(input_string[i] == 32)
              break;
          } while((++i) < input_size);
        }

        if(input_string[i] == 32)
        {
          do
          {
            if(input_string[i] != 32)
              break;
          } while((++i) < input_size);
        }
        // Chop UNTIL i. (i points to first No-Chop)

        strcpy(input_string, input_string + i);
        src_board->input_size = strlen(input_string);
        src_board->num_input = atoi(input_string);
        break;
      }

      case 203: // Push dir
      {
        if(id)
        {
          int push_dir = parse_param(mzx_world, cmd_ptr + 1, id);
          push_dir = parsedir(push_dir, x, y, cur_robot->walk_dir) - 1;

          if((push_dir >= 0) && (push_dir <= 3))
          {
            int push_x = x;
            int push_y = y;
            if(!move_dir(src_board, &push_x, &push_y, push_dir))
            {
              int offset = push_x + (push_y * board_width);
              int d_id = level_id[offset];
              int d_flag = flags[d_id];

              if((d_id != 123) && (d_id != 127))
              {
                if(((push_dir < 2) && (d_flag & A_PUSHNS)) ||
                 ((push_dir >= 2) && (d_flag & A_PUSHEW)))

                push(mzx_world, x, y, push_dir, 0);
                update_blocked = 1;
              }
            }
          }
        }
        break;
      }

      case 204: // Scroll char dir
      {
        int char_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int scroll_dir = parse_param(mzx_world, p2, id);
        scroll_dir = parsedir(scroll_dir, x, y, cur_robot->walk_dir);

        if((scroll_dir >= 1) && (scroll_dir <= 4))
        {
          char char_buffer[14];
          int i;

          ec_read_char(char_num, char_buffer);

          switch(scroll_dir)
          {
            case 1:
            {
              char wrap_row = char_buffer[0];

              for(i = 0; i < 13; i++)
              {
                char_buffer[i] = char_buffer[i + 1];
              }
              char_buffer[13] = wrap_row;

              break;
            }

            case 2:
            {
              char wrap_row = char_buffer[13];

              for(i = 14; i > 0; i--)
              {
                char_buffer[i] = char_buffer[i - 1];
              }
              char_buffer[0] = wrap_row;

              break;
            }

            case 3:
            {
              char wrap_bit;

              for(i = 0; i < 14; i++)
              {
                wrap_bit = char_buffer[i] & 1;
                char_buffer[i] >>= 1;
                if(wrap_bit)
                  char_buffer[i] |= 0x80;
              }
              break;
            }

            case 4:
            {
              char wrap_bit;

              for(i = 0; i < 14; i++)
              {
                wrap_bit = char_buffer[i] & 0x80;
                char_buffer[i] <<= 1;
                if(wrap_bit)
                  char_buffer[i] |= 1;
              }
            }
          }

          ec_change_char(char_num, char_buffer);
        }
        break;
      }

      case 205: // Flip char dir
      {
        int char_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int flip_dir = parse_param(mzx_world, p2, id);
        char char_buffer[14];
        char current_row;
        int i;

        flip_dir = parsedir(flip_dir, x, y, cur_robot->walk_dir);

        if((flip_dir >= 1) && (flip_dir <= 4))
        {
          ec_read_char(char_num, char_buffer);

          switch(flip_dir)
          {
            case 1:
            case 2:
            {
              for(i = 0; i < 7; i++)
              {
                current_row = char_buffer[i];
                char_buffer[i] = char_buffer[13 - i];
                char_buffer[13 - i] = current_row;
              }
            }
            break;

            case 3:
            case 4:
            {
              for(i = 0; i < 14; i++)
              {
                current_row = char_buffer[i];
                current_row = (current_row << 4) | (current_row >> 4);
                current_row = ((current_row & 0xCC) >> 2) |
                 ((current_row & 0x33) << 2);
                current_row = ((current_row & 0xAA) >> 1) |
                 ((current_row & 0x55) << 1);

                char_buffer[i] = current_row;
              }
              break;
            }
          }

          ec_change_char(char_num, char_buffer);
        }
        break;
      }

      case 206: // copy char char
      {
        int src_char = parse_param(mzx_world, cmd_ptr + 1, id);
        char char_buffer[14];
        char *p2 = next_param_pos(cmd_ptr + 1);
        int dest_char = parse_param(mzx_world, p2, id);
        ec_read_char(src_char, char_buffer);
        ec_change_char(dest_char, char_buffer);
        break;
      }

      case 210: // change sfx
      {
        int fx_num = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);

        if(strlen(p2 + 1) > 68)
          p2[69] = 0;
        strcpy(mzx_world->custom_sfx + (fx_num * 69), p2 + 1);
        break;
      }

      case 211: // color intensity #%
      {
        int intensity = parse_param(mzx_world, cmd_ptr + 1, id);
        if(intensity < 0)
          intensity = 0;

        set_palette_intensity(intensity);
        pal_update = 1;
        break;
      }

      case 212: // color intensity # #%
      {
        int color = parse_param(mzx_world, cmd_ptr + 1, id) & 0xFF;
        char *p2 = next_param_pos(cmd_ptr + 1);
        int intensity = parse_param(mzx_world, p2, id);
        if(intensity < 0)
          intensity = 0;

        set_color_intensity(color, intensity);
        pal_update = 1;
        break;
      }

      case 213: // color fade out
      {
        vquick_fadeout();
        break;
      }

      case 214: // color fade in
      {
        vquick_fadein();
        break;
      }

      case 215: // set color # r g b
      {
        // Now you can set all 256 colors this way (for SMZX)
        int pal_number = parse_param(mzx_world, cmd_ptr + 1, id) & 0xFF;
        char *p2 = next_param_pos(cmd_ptr + 1);
        int r = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int g = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int b = parse_param(mzx_world, p4, id);

        // It's pretty sad that this is necessary, but some people don't
        // know how to use set color apparently (see SM4MZX for details)
        if(r > 63)
          r = 63;
        if(g > 63)
          g = 63;
        if(b > 63)
          b = 63;

        set_rgb(pal_number, r, g, b);
        pal_update = 1;
        break;
      }

      case 216: // Load char set ""
      {
        char charset_name[128];

        tr_msg(mzx_world, cmd_ptr + 2, id, charset_name);

        // This will load a charset to a different position - Exo
        if(charset_name[0] == '+')
        {
          char *next;
          char tempc = charset_name[3];
          int pos;

          charset_name[3] = 0;
          pos = (int)strtol(charset_name + 1, &next, 16);
          charset_name[3] = tempc;
          ec_load_set_var(next, pos);
        }
        else

        if(charset_name[0] == '@')
        {
          char *next;
          char tempc = charset_name[4];
          int pos;

          charset_name[4] = 0;
          pos = (int)strtol(charset_name + 1, &next, 10);
          charset_name[4] = tempc;
          ec_load_set_var(next, pos);
        }
        else
        {
          ec_load_set(charset_name);
        }
        break;
      }

      case 217: // multiply counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = cmd_ptr + next_param(cmd_ptr, 1);
        char src_buffer[128];
        char dest_buffer[128];
        int value = parse_param(mzx_world, src_string + 1, id);
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        mul_counter(mzx_world, dest_buffer, value, id);
        break;
      }

      case 218: // divide counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = cmd_ptr + next_param(cmd_ptr, 1);
        char src_buffer[128];
        char dest_buffer[128];
        int value = parse_param(mzx_world, src_string + 1, id);
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        div_counter(mzx_world, dest_buffer, value, id);
        break;
      }

      case 219: // mul counter #
      {
        char *dest_string = cmd_ptr + 2;
        char *src_string = cmd_ptr + next_param(cmd_ptr, 1);
        char src_buffer[128];
        char dest_buffer[128];
        int value = parse_param(mzx_world, src_string + 1, id);
        tr_msg(mzx_world, dest_string, id, dest_buffer);

        mod_counter(mzx_world, dest_buffer, value, id);
        break;
      }

      case 220: // Player char dir 'c'
      {
        int direction = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int new_char = parse_param(mzx_world, p2, id);
        direction = parsedir(direction, x, y, cur_robot->walk_dir);

        if((direction >= 1) && (direction <= 4))
        {
          player_char[direction - 1] = new_char;
        }
        break;
      }

      // Can load full 256 color ones too now. Will use file size to
      // determine how many to load.
      case 222: // Load palette
      {
        char name_buffer[128];

        tr_msg(mzx_world, cmd_ptr + 2, id, name_buffer);
        load_palette(name_buffer);
        pal_update = 1;

        // Done.
        break;
      }

      case 224: // Mod fade #t #s
      {
        int volume_target = parse_param(mzx_world, cmd_ptr + 1, id);;
        char *p2 = next_param_pos(cmd_ptr + 1);
        int volume = src_board->volume;
        int volume_inc = parse_param(mzx_world, p2, id);

        if(volume_target == volume)
        {
          src_board->volume_inc = 0;
          src_board->volume_target = 0;
        }
        else
        {
          if(volume_inc == 0)
            volume_inc = 1;
          if((volume < volume_target) && (volume_inc < 0))
            volume_inc = -volume_inc;
          if((volume > volume_target) && (volume_inc > 0))
            volume_inc = -volume_inc;

          src_board->volume_target = volume_target;
          src_board->volume_inc = volume_inc;
        }
        break;
      }

      case 225: // Scrollview x y
      {
        int scroll_x = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int scroll_y = parse_param(mzx_world, p2, id);
        int n_scroll_x, n_scroll_y;

        prefix_mid_xy(mzx_world, &scroll_x, &scroll_y, x, y);

        // scroll_x/scrolly target to put in upper left corner
        // offset off of player position
        src_board->scroll_x = 0;
        src_board->scroll_y = 0;
        calculate_xytop(mzx_world, &n_scroll_x, &n_scroll_y);
        src_board->scroll_x = scroll_x - n_scroll_x;
        src_board->scroll_y = scroll_y - n_scroll_y;
        break;
      }

      case 226: // Swap world str
      {
        int redo_load = 0;
        char name_buffer[128];
        tr_msg(mzx_world, cmd_ptr + 2, id, name_buffer);

        do
        {
          if(reload_swap(mzx_world, name_buffer, &fade))
          {
            redo_load = error("Error swapping to next world", 1, 23, 0x2C01);
          }
        } while(redo_load == 2);

        strcpy(curr_file, name_buffer);
        mzx_world->swapped = 1;
        return;
      }

      case 227: // If allignedrobot str str
      {
        if(id)
        {
          Robot *dest_robot;
          int first, last;
          char robot_name_buffer[128];
          tr_msg(mzx_world, cmd_ptr + 2, id, robot_name_buffer);

          find_robot(src_board, robot_name_buffer, &first, &last);
          while(first <= last)
          {
            dest_robot = src_board->robot_list_name_sorted[first];
            if(dest_robot &&
             ((dest_robot->xpos == x) || (dest_robot->ypos == y)))
            {
              char *p2 = next_param_pos(cmd_ptr + 1);
              gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
            }
            first++;
          }
        }
        break;
      }

      case 229: // Lockscroll
      {
        // The scrolling is locked at the current position.
        // Further scrollview cmds can change it.
        int scroll_x = src_board->scroll_x;
        int scroll_y = src_board->scroll_y;
        int n_scroll_x, n_scroll_y;
        src_board->scroll_x = 0;
        src_board->scroll_y = 0;
        calculate_xytop(mzx_world, &n_scroll_x, &n_scroll_y);
        src_board->locked_x = n_scroll_x;
        src_board->locked_y = n_scroll_y;
        src_board->scroll_x = scroll_x;
        src_board->scroll_y = scroll_y;
        break;
      }

      case 230: // Unlockscroll
      {
        src_board->locked_x = -1;
        src_board->locked_y = -1;
        break;
      }

      case 231: // If first string str str
      {
        char *input_string = src_board->input_string;
        char match_string_buffer[128];
        int i = 0;

        if(src_board->input_size)
        {
          do
          {
            if(input_string[i] == 32) break;
            i++;
          } while(i < src_board->input_size);
        }

        if(input_string[i] == 32)
          input_string[i] = 0;
        else
          i = -1;

        tr_msg(mzx_world, cmd_ptr + 2, id, match_string_buffer);

        // Compare
        if(!strcasecmp(input_string, match_string_buffer))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          gotoed = send_self_label_tr(mzx_world, p2 + 1, id);
        }

        if(i >= 0)
          input_string[i] = 32;

        break;
      }

      case 233: // Wait mod fade
      {
        if(src_board->volume != src_board->volume_target)
          goto breaker;
        break;
      }

      case 235: // Enable saving
      {
        src_board->save_mode = 0;
        break;
      }

      case 236: // Disable saving
      {
        src_board->save_mode = 1;
        break;
      }

      case 237: // Enable sensoronly saving
      {
        src_board->save_mode = 2;
        break;
      }

      case 238: // Status counter ## str
      {
        int counter_slot = parse_param(mzx_world, cmd_ptr + 1, id);
        if((counter_slot >= 1) && (counter_slot <= 6))
        {
          char *p2 = next_param_pos(cmd_ptr + 1);
          char counter_name[128];
          tr_msg(mzx_world, p2 + 1, id, counter_name);

          if(strlen(counter_name) >= COUNTER_NAME_SIZE)
            counter_name[COUNTER_NAME_SIZE - 1] = 0;

          strcpy(mzx_world->status_counters_shown[counter_slot], counter_name);
        }
        break;
      }

      case 239: // Overlay on
      {
        setup_overlay(src_board, 1);
        break;
      }

      case 240: // Overlay static
      {
        setup_overlay(src_board, 2);
        break;
      }

      case 241: // Overlay transparent
      {
        setup_overlay(src_board, 3);
        break;
      }

      case 242: // put col ch overlay x y
      {
        int put_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int put_char = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int put_x = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int put_y = parse_param(mzx_world, p4, id);
        int offset;

        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        prefix_mid_xy(mzx_world, &put_x, &put_y, x, y);
        offset = put_x + (put_y * board_width);

        put_color =
          fix_color(put_color, src_board->overlay_color[offset]);

        src_board->overlay[offset] = put_char;
        src_board->overlay_color[offset] = put_color;
        break;
      }

      case 245: // Change overlay col ch col ch
      {
        int src_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int src_char = parse_param(mzx_world, p2, id);
        char *p3 = next_param_pos(p2);
        int dest_color = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int dest_char = parse_param(mzx_world, p4, id);
        int src_fg, src_bg, i;
        char *overlay, *overlay_color;
        int d_color;

        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        overlay = src_board->overlay;
        overlay_color = src_board->overlay_color;

        split_colors(src_color, &src_fg, &src_bg);

        for(i = 0; i < (board_width * board_height); i++)
        {
          d_color = overlay_color[i];

          if((overlay[i] == src_char) &&
           ((src_bg == 16) || (src_bg == (d_color >> 4))) &&
           ((src_fg == 16) || (src_fg == (d_color & 0x0F))))
          {
            overlay[i] = dest_char;
            overlay_color[i] = fix_color(dest_color, d_color);
          }
        }
        break;
      }

      case 246: // Change overlay col col
      {
        int src_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        int dest_color = parse_param(mzx_world, p2, id);
        int src_fg, src_bg, i;
        char *overlay, *overlay_color;
        int d_color;

        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        overlay = src_board->overlay;
        overlay_color = src_board->overlay_color;

        split_colors(src_color, &src_fg, &src_bg);

        for(i = 0; i < (board_width * board_height); i++)
        {
          d_color = overlay_color[i];

          if(((src_bg == 16) || (src_bg == (d_color >> 4))) &&
           ((src_fg == 16) || (src_fg == (d_color & 0x0F))))
          {
            overlay_color[i] = fix_color(dest_color, d_color);
          }
        }
        break;
      }

      case 247: // Write overlay col str # #
      {
        int write_color = parse_param(mzx_world, cmd_ptr + 1, id);
        char *p2 = next_param_pos(cmd_ptr + 1);
        char *write_string = p2 + 1;
        char *p3 = next_param_pos(p2);
        int write_x = parse_param(mzx_world, p3, id);
        char *p4 = next_param_pos(p3);
        int write_y = parse_param(mzx_world, p4, id);
        int offset;
        char string_buffer[128];
        char current_char;
        char *overlay, *overlay_color;

        prefix_mid_xy(mzx_world, &write_x, &write_y, x, y);
        offset = write_x + (write_y * board_width);

        tr_msg(mzx_world, write_string, id, string_buffer);

        write_string = string_buffer;
        current_char = *write_string;

        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        overlay_color = src_board->overlay_color;
        overlay = src_board->overlay;

        while(current_char != 0)
        {
          overlay_color[offset] = write_color;
          overlay[offset] = current_char;
          write_x++;
          if(write_x == board_width)
            break;
          write_string++;
          current_char = *write_string;
          offset++;
        }
        break;
      }

      case 251: // Loop start
      {
        cur_robot->loop_count = 0;
        break;
      }

      case 252: // Loop #
      {
        int loop_amount = parse_param(mzx_world, cmd_ptr + 1, id);
        int loop_count = cur_robot->loop_count;
        int back_cmd;

        if(loop_count < loop_amount)
        {
          back_cmd = cur_robot->cur_prog_line;
          do
          {
            if((unsigned char)program[back_cmd - 1] == 0xFF) break;
            back_cmd -= program[back_cmd - 1] + 2;
          } while((unsigned char)program[back_cmd + 1] != 251);

          cur_robot->cur_prog_line = back_cmd;
        }

        cur_robot->loop_count = loop_count + 1;
        break;
      }

      case 253: // Abort loop
      {
        int loop_amount = parse_param(mzx_world, cmd_ptr + 1, id);
        int back_cmd;

        do
        {
          if((unsigned char)program[back_cmd - 1] == 0xFF) break;
          back_cmd -= program[back_cmd - 1] + 2;
        } while((unsigned char)program[back_cmd + 1] != 251);

        cur_robot->cur_prog_line = back_cmd;
        break;
      }

      case 254: // Disable mesg edge
      {
        mzx_world->mesg_edges = 0;
        break;
      }


      case 255: // Enable mesg edge
      {
        mzx_world->mesg_edges = 1;
        break;
      }
    }

    // Go to next command! First erase prefixes...
    next_cmd:

    mzx_world->first_prefix = 0;
    mzx_world->mid_prefix = 0;
    mzx_world->last_prefix = 0;

    next_cmd_prefix:
    // Next line
    cur_robot->pos_within_line = 0;
    first_cmd = 0;

    if(!cur_robot->cur_prog_line)
      break;

    if(!gotoed)
      cur_robot->cur_prog_line += program[cur_robot->cur_prog_line] + 2;

    if(!program[cur_robot->cur_prog_line])
    {
      //End of program
      end_prog:
      cur_robot->cur_prog_line = 0;
      break;
    }

    if(update_blocked)
    {
      calculate_blocked(mzx_world, x, y, id, _bl);
    }
    find_player(mzx_world);

  } while(((++lines_run) < commands) && (!done));

  breaker:

  cur_robot->cycle_count = 0; // In case a label changed it
  // Reset x/y (from movements)
  cur_robot->xpos = x;
  cur_robot->ypos = y;
}

