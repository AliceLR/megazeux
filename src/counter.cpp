/* $Id$
 * MegaZeux
 *
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

// New counter.cpp. Sorted lists make for faster searching.
// Builtins are also cleaned up by being put on a seperate list.

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "counter.h"
#include "data.h"
#include "idarray.h"
#include "idput.h"
#include "graphics.h"
#include "game.h"
#include "event.h"
#include "time.h"
#include "robot.h"
#include "audio.h"
#include "rasm.h"
#include "fsafeopen.h"
//#include "counter_first_letter.h"

typedef int (* builtin_read_function)(World *mzx_world,
 Function_counter *counter, char *name, int id);

int local_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int local_num = 0;

  if(name[5])
    local_num = (strtol(name + 5, NULL, 10) - 1) & 31;

  return (mzx_world->current_board->robot_list[id])->local[local_num];
}

void local_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int local_num = 0;

  if(name[5])
    local_num = (strtol(name + 5, NULL, 10) - 1) & 31;

  (mzx_world->current_board->robot_list[id])->local[local_num] = value;
}

int loopcount_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->loop_count;
}

void loopcount_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->loop_count = value;
}

int playerdist_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  Robot *cur_robot = src_board->robot_list[id];

  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int this_x = cur_robot->xpos;
  int this_y = cur_robot->ypos;

  return abs(player_x - this_x) + abs(player_y - this_y);
}

int score_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->score;
}

void score_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->score = value;
}

int sin_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(sin(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

int cos_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(cos(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

int tan_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(tan(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

int asin_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((asin((double)val / mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

int acos_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((acos((double)val / mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

int atan_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((atan2((double)val, mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

int c_divisions_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->c_divisions;
}

int divider_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->divider;
}

int multiplier_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->multiplier;
}

void c_divisions_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->c_divisions = value;
}

void divider_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->divider = value;
}

void multiplier_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->multiplier = value;
}

int thisx_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  if(mzx_world->mid_prefix == 2)
    return (mzx_world->current_board->robot_list[id])->xpos -
     mzx_world->player_x;

  return (mzx_world->current_board->robot_list[id])->xpos;
}

int thisy_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  if(mzx_world->mid_prefix == 2)
    return (mzx_world->current_board->robot_list[id])->ypos -
     mzx_world->player_y;

  return (mzx_world->current_board->robot_list[id])->ypos;
}

int playerx_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->player_x;
}

int playery_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->player_y;
}

int this_char_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->robot_char;
}

int this_color_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  Robot *cur_robot = src_board->robot_list[id];
  int x = cur_robot->xpos;
  int y = cur_robot->ypos;
  int offset = x + (y * src_board->board_width);

  return src_board->level_color[offset];
}

int sqrt_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)(sqrt(val));
}

int abs_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int val = strtol(name + 3, NULL, 10);
  return abs(val);
}

int playerfacedir_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->current_board->player_last_dir >> 4;
}

void playerfacedir_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  Board *src_board = mzx_world->current_board;

  if(value < 0)
    value = 0;

  if(value > 3)
    value = 3;

  src_board->player_last_dir =
   (src_board->player_last_dir & 0x0F) | (value << 4);
}

int playerlastdir_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->current_board->player_last_dir & 0x0F;
}

void playerlastdir_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  Board *src_board = mzx_world->current_board;

  if(value < 0)
    value = 0;

  if(value > 4)
    value = 4;

  src_board->player_last_dir =
   (src_board->player_last_dir & 0xF0) | value;
}

int horizpld_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return abs(mzx_world->player_x -
   (mzx_world->current_board->robot_list[id])->xpos);
}

int vertpld_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return abs(mzx_world->player_y -
   (mzx_world->current_board->robot_list[id])->ypos);
}

int board_char_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "board_x", id) +
   (get_counter(mzx_world, "board_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return get_id_char(src_board, offset);

  return -1;
}

int board_color_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "board_x", id) +
   (get_counter(mzx_world, "board_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return get_id_color(src_board, offset);

  return -1;
}

int board_w_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->current_board->board_width;
}

int board_h_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->current_board->board_height;
}

int board_id_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "board_x", id) +
   (get_counter(mzx_world, "board_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return src_board->level_id[offset];

  return -1;
}

void board_id_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  Board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "board_x", id) +
   (get_counter(mzx_world, "board_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if(((value < 122) || (value == 127)) &&
   (src_board->level_id[offset] < 122) && (offset >= 0) &&
   (offset < board_size))
    src_board->level_id[offset] = value;
}

int board_param_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "board_x", id) +
   (get_counter(mzx_world, "board_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return src_board->level_param[offset];

  return -1;
}

void board_param_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  Board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "board_x", id) +
   (get_counter(mzx_world, "board_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((src_board->level_id[offset] < 122) &&
   (src_board->level_id[offset] < 122) && (offset >= 0) &&
   (offset < board_size))
    src_board->level_param[offset] = value;
}

int red_value_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id);
  return get_red_component(cur_color);
}

int green_value_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  return get_green_component(cur_color);
}

int blue_value_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  return get_blue_component(cur_color);
}

void red_value_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  set_red_component(cur_color, value);
  pal_update = 1;
}

void green_value_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  set_green_component(cur_color, value);
  pal_update = 1;
}

void blue_value_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  set_blue_component(cur_color, value);
  pal_update = 1;
}

int overlay_mode_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->current_board->overlay_mode;
}

int mzx_speed_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->mzx_speed;
}

void mzx_speed_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->mzx_speed = value;
}

int overlay_char_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "overlay_x", id) +
   (get_counter(mzx_world, "overlay_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return src_board->overlay[offset];

  return -1;
}

int overlay_color_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "overlay_x", id) +
   (get_counter(mzx_world, "overlay_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return src_board->overlay_color[offset];

  return -1;
}

int smzx_mode_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_screen_mode();
}

void smzx_mode_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  set_screen_mode(value);
}

int smzx_r_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  return get_red_component(cur_color);
}

int smzx_g_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  return get_green_component(cur_color);
}

int smzx_b_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  return get_blue_component(cur_color);
}

void smzx_r_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  set_red_component(cur_color, value);
  pal_update = 1;
}

void smzx_g_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  set_green_component(cur_color, value);
  pal_update = 1;
}

void smzx_b_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  set_blue_component(cur_color, value);
  pal_update = 1;
}

int spr_clist_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int clist_num = strtol(name + 9, NULL, 10) & (MAX_SPRITES - 1);
  return mzx_world->collision_list[clist_num];
}

int spr_collisions_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->collision_count;
}

int spr_num_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->sprite_num;
}

int spr_cx_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->col_x;
}

int spr_cy_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->col_y;
}

int spr_width_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->width;
}

int spr_height_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->height;
}

int spr_refx_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->ref_x;
}

int spr_refy_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->ref_y;
}

int spr_x_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->x;
}

int spr_y_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->y;
}

int spr_cwidth_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->col_width;
}

int spr_cheight_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  return (mzx_world->sprite_list[spr_num])->col_height;
}

void spr_num_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  value &= 0xFF;

  mzx_world->sprite_num = (unsigned int)value;
}

void spr_yorder_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->sprite_y_order = value & 1;
}

void spr_ccheck_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  Sprite *cur_sprite = mzx_world->sprite_list[spr_num];

  value %= 3;
  switch(value)
  {
    case 0:
    {
      cur_sprite->flags &= ~(SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2);
      break;
    }

    case 1:
    {
      cur_sprite->flags |= SPRITE_CHAR_CHECK;
      break;
    }

    case 2:
    {
      cur_sprite->flags |= SPRITE_CHAR_CHECK2;
      break;
    }
  }
}

void spr_clist_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  Sprite *cur_sprite = mzx_world->sprite_list[spr_num];
  sprite_colliding_xy(mzx_world, cur_sprite, cur_sprite->x,
   cur_sprite->y);
}

void spr_cx_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->col_x = value;
}

void spr_cy_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->col_y = value;
}

void spr_height_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->height = value;
}

void spr_width_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->width = value;
}

void spr_refx_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;

  if(value < 0)
    value = 0;

  (mzx_world->sprite_list[spr_num])->ref_x = value;
}

void spr_refy_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;

  if(value < 0)
    value = 0;

  (mzx_world->sprite_list[spr_num])->ref_y = value;
}

void spr_x_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->x = value;
}

void spr_y_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->y = value;
}

void spr_vlayer_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_VLAYER;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_VLAYER;
}

void spr_static_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_STATIC;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_STATIC;
}

void spr_overlaid_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_OVER_OVERLAY;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_OVER_OVERLAY;
}

void spr_off_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_INITIALIZED;
}

void spr_swap_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  Sprite *src = mzx_world->sprite_list[spr_num];
  Sprite *dest = mzx_world->sprite_list[value];
  mzx_world->sprite_list[value] = src;
  mzx_world->sprite_list[spr_num] = dest;
}

void spr_cwidth_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->col_width = value;
}

void spr_cheight_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  (mzx_world->sprite_list[spr_num])->col_height = value;
}

void spr_setview_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  Board *src_board = mzx_world->current_board;
  int n_scroll_x, n_scroll_y;
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1) & 0xFF;
  Sprite *cur_sprite = mzx_world->sprite_list[spr_num];
  src_board->scroll_x = 0;
  src_board->scroll_y = 0;
  calculate_xytop(mzx_world, &n_scroll_x, &n_scroll_y);
  src_board->scroll_x =
   (cur_sprite->x + (cur_sprite->width >> 1)) -
   (src_board->viewport_width >> 1) - n_scroll_x;
  src_board->scroll_y =
   (cur_sprite->y + (cur_sprite->height >> 1)) -
   (src_board->viewport_height >> 1) - n_scroll_y;
  return;
}

int bullettype_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->bullet_type;
}

void bullettype_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->bullet_type = value;
}

int inputsize_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->current_board->input_size;
}

void inputsize_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->current_board->input_size = value;
}

int input_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->current_board->num_input;
}

void input_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->current_board->num_input = value;
  sprintf(mzx_world->current_board->input_string, "%d", value);
}

int key_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  if(name[3])
  {
    int key_num = strtol(name + 3, NULL, 10);
    return get_key_status(keycode_pc_xt, key_num);
  }
  else
  {
    return toupper(get_last_key(keycode_SDL));
  }
}

void key_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  if(!name[3])
    force_last_key(keycode_SDL, toupper(value));
}

int key_code_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_key(keycode_pc_xt);
}

int key_pressed_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_key(keycode_SDL);
}

int key_release_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_last_key_released(keycode_pc_xt);
}

int scrolledx_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return scroll_x;
}

int scrolledy_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return scroll_y;
}

int timerset_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->current_board->time_limit;
}

void timerset_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->current_board->time_limit = value;
}

int date_day_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  time_t e_time = time(NULL);
  tm *t = localtime(&e_time);
  return t->tm_mday;
}

int date_year_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  time_t e_time = time(NULL);
  tm *t = localtime(&e_time);
  return t->tm_year + 1900;
}

int date_month_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  time_t e_time = time(NULL);
  tm *t = localtime(&e_time);
  return t->tm_mon + 1;
}

int time_hours_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  time_t e_time = time(NULL);
  tm *t = localtime(&e_time);
  return t->tm_hour;
}

int time_minutes_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  time_t e_time = time(NULL);
  tm *t = localtime(&e_time);
  return t->tm_min;
}

int time_seconds_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  time_t e_time = time(NULL);
  tm *t = localtime(&e_time);
  return t->tm_sec;
}

int vch_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  unsigned int x, y;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    return mzx_world->vlayer_chars[offset];
  }

  return -1;
}

void vch_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  unsigned int x, y;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    mzx_world->vlayer_chars[offset] = value;
  }
}

int vco_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  unsigned int x, y;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    return mzx_world->vlayer_colors[offset];
  }

  return -1;
}

void vco_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  unsigned int x, y;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    mzx_world->vlayer_colors[offset] = value;
  }
}

int char_byte_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return ec_read_byte(get_counter(mzx_world, "CHAR", id),
   get_counter(mzx_world, "BYTE", id));
}

void char_byte_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  ec_change_byte(get_counter(mzx_world, "CHAR", id),
   get_counter(mzx_world, "BYTE", id), value);
}

int pixel_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int pixel_mask;
  int pixel_x = get_counter(mzx_world, "CHAR_X", id);
  int pixel_y = get_counter(mzx_world, "CHAR_Y", id);

  char sub_x, sub_y, current_byte;
  unsigned char current_char;
  sub_x = pixel_x % 8;
  sub_y = pixel_y % 14;
  current_char = ((pixel_y / 14) * 32) + (pixel_x / 8);
  current_byte = ec_read_byte(current_char, sub_y);
  pixel_mask = (128 >> sub_x);
  return (current_byte & pixel_mask) >> (7 - sub_x);
}

void pixel_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  char sub_x, sub_y, current_byte;
  unsigned char current_char;
  int pixel_x = get_counter(mzx_world, "CHAR_X", id) & 255;
  int pixel_y = get_counter(mzx_world, "CHAR_Y", id) % 112;

  sub_x = pixel_x & 7;
  sub_y = pixel_y % 14;
  current_char = ((pixel_y / 14) << 5) + (pixel_x >> 3);
  current_byte = ec_read_byte(current_char, sub_y);

  switch(value)
  {
    case 0:
    {
      //clear
      current_byte &= ~(128 >> sub_x);
      ec_change_byte(current_char, sub_y, current_byte);
      break;
    }
    case 1:
    {
      //set
      current_byte |= (128 >> sub_x);
      ec_change_byte(current_char, sub_y, current_byte);
      break;
    }
    case 2:
    {
      //toggle
      current_byte ^= (128 >> sub_x);
      ec_change_byte(current_char, sub_y, current_byte);
      break;
    }
  }
  return;
}

int int2bin_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int place = get_counter(mzx_world, "BIT_PLACE", id) & 15;
  return ((get_counter(mzx_world, "INT", id) >> place) & 1);
}

void int2bin_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  unsigned int integer;
  int place;
  place = (get_counter(mzx_world, "BIT_PLACE", id) & 15);
  integer = get_counter(mzx_world, "INT", id);

  switch(value & 2)
  {
    case 0:
    {
      //clear
      integer &= ~(1 << place);
      break;
    }

    case 1:
    {
      //set
      integer |= (1 << place);
      break;
    }

    case 2:
    {
      //toggle
      integer ^= (1 << place);
      break;
    }
  }

  set_counter(mzx_world, "INT", integer, id);
}

int commands_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->commands;
}

void commands_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->commands = value;
}

int fread_open_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FREAD;
  return 0;
}

int fwrite_open_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FWRITE;
  return 0;
}

int fwrite_append_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FAPPEND;
  return 0;
}

int fwrite_modify_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FMODIFY;
  return 0;
}

int smzx_palette_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SMZX_PALETTE;
  return 0;
}

int load_game_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_GAME;
  return 0;
}

int save_game_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_GAME;
  return 0;
}

int save_world_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_WORLD;
  return 0;
}

int save_robot_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_ROBOT;
  if(name[10])
    return strtol(name + 10, NULL, 10);

  return -1;
}

int load_robot_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_ROBOT;
  if(name[10])
    return strtol(name + 10, NULL, 10);

  return -1;
}

int load_bc_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_BC;

  if(name[7])
    return strtol(name + 7, NULL, 10);

  return -1;
}

int save_bc_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_BC;
  if(name[7])
    return strtol(name + 7, NULL, 10);

  return -1;
}

int fread_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  if(mzx_world->input_file)
    return fgetc(mzx_world->input_file);
  else
    return -1;
}

int fread_counter_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  if(mzx_world->input_file)
    return fgetw(mzx_world->input_file);
  else
    return -1;
}

int fread_pos_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  if(mzx_world->input_file)
    return ftell(mzx_world->input_file);
  else
    return -1;
}

void fread_pos_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  if(mzx_world->input_file)
  {
    if(value == -1)
      fseek(mzx_world->input_file, 0, SEEK_END);
    else
      fseek(mzx_world->input_file, value, SEEK_SET);
  }
}

int fwrite_pos_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  if(mzx_world->output_file)
    return ftell(mzx_world->output_file);
  else
    return -1;
}

void fwrite_pos_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  if(mzx_world->output_file)
  {
    if(value == -1)
      fseek(mzx_world->output_file, 0, SEEK_END);
    else
      fseek(mzx_world->output_file, value, SEEK_SET);
  }
}

void fwrite_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  if(mzx_world->output_file)
    fputc(value, mzx_world->output_file);
}

void fwrite_counter_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  if(mzx_world->output_file)
    fputw(value, mzx_world->output_file);
}

int lava_walk_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->can_lavawalk;
}

void lava_walk_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->can_lavawalk = value;
}

int robot_id_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return id;
}

int robot_id_n_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_robot_id(mzx_world->current_board, name + 9);
}

int rid_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_robot_id(mzx_world->current_board, name + 3);
}

int r_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  Board *src_board = mzx_world->current_board;
  char *next;
  int robot_num = strtol(name + 1, &next, 10);

  if((robot_num <= src_board->num_robots) &&
   (src_board->robot_list[robot_num] != NULL))
    return get_counter(mzx_world, next + 1, robot_num);

  return -1;
}

void r_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  Board *src_board = mzx_world->current_board;
  char *next;
  int robot_num = strtol(name + 1, &next, 10);
  if((robot_num <= src_board->num_robots) &&
   (src_board->robot_list[robot_num] != NULL))
    set_counter(mzx_world, next + 1, value, robot_num);
}

int vlayer_height_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->vlayer_height;
}

int vlayer_width_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return mzx_world->vlayer_width;
}

void vlayer_height_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->vlayer_height = value;
  mzx_world->vlayer_width = 32768 / value;
}

void vlayer_width_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->vlayer_width = value;
  mzx_world->vlayer_height = 32768 / value;
}

int buttons_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int buttons_current, buttons_formatted = 0;
  buttons_current = get_mouse_status();
  if(buttons_current & (1 << (SDL_BUTTON_LEFT - 1)))
    buttons_formatted |= 1;

  if(buttons_current & (1 << (SDL_BUTTON_RIGHT - 1)))
    buttons_formatted |= 2;

  if(buttons_current & (1 << (SDL_BUTTON_MIDDLE - 1)))
    buttons_formatted |= 4;

  return buttons_formatted;
}

int mousex_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_mouse_x();
}

int mousey_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_mouse_y();
}

void mousex_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  if(value > 79)
    value = 79;

  if(value < 0)
    value = 0;

  warp_mouse_x(value);
}

void mousey_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  if(value > 25)
    value = 25;

  if(value < 0)
    value = 0;

  warp_mouse_y(value);
}

int mboardx_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return get_mouse_x() +
   scroll_x - mzx_world->current_board->viewport_x;
}

int mboardy_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return get_mouse_y() +
   scroll_y - mzx_world->current_board->viewport_y;
}

void bimesg_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  mzx_world->bi_mesg_status = value & 1;
}

int str_num_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  // Is it a single char?
  int dot_position = 0;
  int name_position = 1;
  char cur_char = name[1];
  char str_buffer[64];

  while(cur_char)
  {
    if(cur_char == '.')
      dot_position = name_position;
    name_position++;
    cur_char = name[name_position];
  }

  if(dot_position)
  {
    // A number
    int str_num = strtol(name + dot_position + 1, NULL, 10);
    if(str_num < 63)
    {
      name[dot_position] = 0;
      get_string(mzx_world, name, id, str_buffer);
      return str_buffer[str_num];
    }
  }

  get_string(mzx_world, name, id, str_buffer);
  return strtol(str_buffer, NULL, 10);
}

void str_num_write(World *mzx_world, Function_counter *counter,
 char *name, int value, int id)
{
  // Is it a single char?
  int dot_position = 0;
  int name_position = 1;
  int str_num;
  char cur_char = name[1];

  while(cur_char)
  {
    if(cur_char == '.')
      dot_position = name_position;
    name_position++;
    cur_char = name[name_position];
  }

  // A number
  str_num = strtol(name + dot_position + 1, NULL, 10);
  if(str_num < 63)
  {
    String *sdest;
    int next;

    name[dot_position] = 0;

    if(strlen(name) > 14)
      name[14] = 0;

    sdest = find_string(mzx_world, name, &next);
    if(sdest)
    {
      sdest->string_contents[str_num] = value;
    }
    else
    {
      char str_buffer[64];
      str_buffer[str_num] = value;
      add_string(mzx_world, name, str_buffer, next);
    }
  }
}

int mod_order_read(World *mzx_world, Function_counter *counter,
 char *name, int id)
{
  return get_order();
}

// Make sure this is in the right alphabetical (non-case sensitive) order
// ?A specifies that 0 through A numbers will match at this point
// ?A-B specifies A through B numbers
// * skips any characters at or after this point, including a null terminator
// This must be sorted in a case insensitive way, that has special applications
// for non-letters:
// _ comes after letters
// ? comes before letters
// numbers come before letters (and before ?)

Function_counter builtin_counters[] =
{
  { "$*", 0, str_num_read, str_num_write },
  { "abs?1-9", 0, abs_read, NULL },
  { "acos?1-9", 0, acos_read, NULL },
  { "asin?1-9", 0, asin_read, NULL },
  { "atan?1-9", 0, atan_read, NULL },
  { "bimesg", 0, NULL, bimesg_write },
  { "blue_value", 0, blue_value_read, blue_value_write },
  { "board_char", 0, board_char_read, NULL },
  { "board_color", 0, board_color_read, NULL },
  { "board_h", 0, board_h_read, NULL },
  { "board_id", 0, board_id_read, board_id_write },
  { "board_param", 0, board_param_read, board_param_write },
  { "board_w", 0, board_w_read, NULL },
  { "bullettype", 0, bullettype_read, bullettype_write },
  { "buttons", 0, buttons_read, NULL },
  { "char_byte", 0, char_byte_read, char_byte_write },
  { "commands", 0, commands_read, commands_write },
  { "cos?1-9", 0, cos_read, NULL },
  { "c_divisions", 0, c_divisions_read, c_divisions_write },
  { "date_day", 0, date_day_read, NULL },
  { "date_month", 0, date_month_read, NULL },
  { "date_year", 0, date_year_read, NULL },
  { "divider", 0, divider_read, divider_write },
  { "fread", 0, fread_read, NULL },
  { "fread_counter", 0, fread_counter_read, NULL },
  { "fread_open", 0, fread_open_read, NULL },
  { "fread_pos", 0, fread_pos_read, fread_pos_write },
  { "fwrite", 0, NULL, fwrite_write },
  { "fwrite_append", 0, fwrite_append_read, NULL },
  { "fwrite_counter", 0, NULL, fwrite_counter_write },
  { "fwrite_modify", 0, fwrite_modify_read, NULL },
  { "fwrite_open", 0, fwrite_open_read, NULL },
  { "fwrite_pos", 0, fwrite_pos_read, fwrite_pos_write },
  { "green_value", 0, green_value_read, green_value_write },
  { "horizpld", 0, horizpld_read, NULL },
  { "input", 0, input_read, input_write },
  { "inputsize", 0, inputsize_read, inputsize_write },
  { "int2bin", 0, int2bin_read, int2bin_write },
  { "key?3", 0, key_read, key_write },
  { "key_code", 0, key_code_read, NULL },
  { "key_pressed", 0, key_pressed_read, NULL },
  { "key_release", 0, key_release_read, NULL },
  { "lava_walk", 0, lava_walk_read, lava_walk_write },
  { "load_bc?3", 0, load_bc_read, NULL },
  { "load_game", 0, load_game_read, NULL },
  { "load_robot?3", 0, load_robot_read, NULL },
  { "local?2", 0, local_read, local_write },
  { "loopcount", 0, loopcount_read, loopcount_write },
  { "mboardx", 0, mboardx_read, NULL },
  { "mboardy", 0, mboardy_read, NULL },
  { "mod_order", 0, mod_order_read, NULL },
  { "mousex", 0, mousex_read, mousex_write },
  { "mousey", 0, mousey_read, mousey_write },
  { "multiplier", 0, multiplier_read, multiplier_write },
  { "mzx_speed", 0, mzx_speed_read, mzx_speed_write },
  { "overlay_char", 0, overlay_char_read, NULL },
  { "overlay_color", 0, overlay_color_read, NULL },
  { "overlay_mode", 0, overlay_mode_read, NULL },
  { "pixel", 0, pixel_read, pixel_write },
  { "playerdist", 0, playerdist_read, NULL },
  { "playerfacedir", 0, playerfacedir_read, playerfacedir_write },
  { "playerlastdir", 0, playerlastdir_read, playerlastdir_write },
  { "playerx", 0, playerx_read, NULL },
  { "playery", 0, playery_read, NULL },
  { "r?1-5.*", 0, r_read, r_write },
  { "red_value", 0, red_value_read, red_value_write },
  { "rid*", 0, rid_read, NULL },
  { "robot_id", 0, robot_id_read, NULL },
  { "robot_id_*", 0, robot_id_n_read, NULL },
  { "save_bc?3", 0, save_bc_read, NULL },
  { "save_game", 0, save_game_read, NULL },
  { "save_robot?3", 0, save_robot_read, NULL },
  { "save_world", 0, save_world_read, NULL },
  { "score", 0, score_read, score_write },
  { "scrolledx", 0, scrolledx_read, NULL },
  { "scrolledy", 0, scrolledy_read, NULL },
  { "sin?1-9", 0, sin_read, NULL },
  { "smzx_b?1-3", 0, smzx_b_read, smzx_b_write },
  { "smzx_g?1-3", 0, smzx_g_read, smzx_g_write },
  { "smzx_mode", 0, smzx_mode_read, smzx_mode_write },
  { "smzx_palette", 0, smzx_palette_read, NULL },
  { "smzx_r?1-3", 0, smzx_r_read, smzx_r_write },
  { "spr?1-3_ccheck", 0, NULL, spr_ccheck_write },
  { "spr?1-3_cheight", 0, spr_cheight_read, spr_cheight_write },
  { "spr?1-3_clist", 0, NULL, spr_clist_write },
  { "spr?1-3_cwidth", 0, spr_cwidth_read, spr_cwidth_write },
  { "spr?1-3_cx", 0, spr_cx_read, spr_cx_write },
  { "spr?1-3_cy", 0, spr_cy_read, spr_cy_write },
  { "spr?1-3_height", 0, spr_height_read, spr_height_write },
  { "spr?1-3_off", 0, NULL, spr_off_write },
  { "spr?1-3_overlaid", 0, NULL, spr_overlaid_write },
  { "spr?1-3_overlay", 0, NULL, spr_overlaid_write },
  { "spr?1-3_refx", 0, spr_refx_read, spr_refx_write },
  { "spr?1-3_refy", 0, spr_refy_read, spr_refy_write },
  { "spr?1-3_setview", 0, NULL, spr_setview_write },
  { "spr?1-3_static", 0, NULL, spr_static_write },
  { "spr?1-3_swap", 0, NULL, spr_swap_write },
  { "spr?1-3_vlayer", 0, NULL, spr_vlayer_write },
  { "spr?1-3_width", 0, spr_width_read, spr_width_write },
  { "spr?1-3_x", 0, spr_x_read, spr_x_write },
  { "spr?1-3_y", 0, spr_y_read, spr_y_write },
  { "spr_clist?1-3", 0, spr_clist_read, NULL },
  { "spr_collisions", 0, spr_collisions_read, NULL },
  { "spr_num", 0, spr_num_read, spr_num_write },
  { "spr_yorder", 0, NULL, spr_yorder_write },
  { "sqrt?1-9", 0, sqrt_read, NULL },
  { "tan?1-9", 0, tan_read, NULL },
  { "thisx", 0, thisx_read, NULL },
  { "thisy", 0, thisy_read, NULL },
  { "this_char", 0, this_char_read, NULL },
  { "this_color", 0, this_color_read, NULL },
  { "timerset", 0, timerset_read, timerset_write },
  { "time_hours", 0, time_hours_read, NULL },
  { "time_minutes", 0, time_minutes_read, NULL },
  { "time_seconds", 0, time_seconds_read, NULL },
  { "vch?1-5,?1-5", 0, vch_read, vch_write },
  { "vco?1-5,?1-5", 0, vco_read, vco_write },
  { "vertpld", 0, vertpld_read, NULL },
  { "vlayer_height", 0, vlayer_height_read, vlayer_height_write },
  { "vlayer_width", 0, vlayer_width_read, vlayer_width_write },
};

int counter_first_letter[512];

int set_counter_special(World *mzx_world, int spec_type,
 char *char_value, int value, int id)
{
  switch(spec_type)
  {
    case FOPEN_FREAD:
    {
      if(char_value[0])
      {
        if(mzx_world->input_file)
          fclose(mzx_world->input_file);

        mzx_world->input_file = fsafeopen(char_value, "rb");
      }
      else
      {
        if(mzx_world->input_file)
          fclose(mzx_world->input_file);

        mzx_world->input_file = NULL;
      }

      strcpy(mzx_world->input_file_name, char_value);
      return 0;
    }

    case FOPEN_FWRITE:
    {
      if(char_value[0])
      {
        if(mzx_world->output_file)
          fclose(mzx_world->output_file);

        mzx_world->output_file = fsafeopen(char_value, "wb");
      }
      else
      {
        if(mzx_world->output_file)
          fclose(mzx_world->output_file);

        mzx_world->output_file = NULL;
      }

      strcpy(mzx_world->output_file_name, char_value);
      return 0;
    }

    case FOPEN_FAPPEND:
    {
      if(char_value[0])
      {
        char_value[12] = 0;

        if(mzx_world->output_file)
          fclose(mzx_world->output_file);

        mzx_world->output_file = fsafeopen(char_value, "ab");
      }
      else
      {
        if(mzx_world->output_file)
          fclose(mzx_world->output_file);

        mzx_world->output_file = NULL;
      }

      strcpy(mzx_world->output_file_name, char_value);
      return 0;
    }

    case FOPEN_FMODIFY:
    {
      if(char_value[0])
      {
        char_value[12] = 0;

        if(mzx_world->output_file)
          fclose(mzx_world->output_file);

        mzx_world->output_file = fsafeopen(char_value, "r+b");
      }
      else
      {
        if(mzx_world->output_file)
          fclose(mzx_world->output_file);

        mzx_world->output_file = NULL;
      }

      strcpy(mzx_world->output_file_name, char_value);
      return 0;
    }

    case FOPEN_SMZX_PALETTE:
    {
      load_palette(char_value, 0);
      pal_update = 1;
      return 0;
    }

    case FOPEN_SAVE_GAME:
    {
      int faded = get_fade_status();
      save_world(mzx_world, char_value, 1, faded);
      return 0;
    }

    case FOPEN_SAVE_WORLD:
    {
      save_world(mzx_world, char_value, 0, 0);
      return 0;
    }

    case FOPEN_LOAD_GAME:
    {
      int faded;
      char translated_name[MAX_PATH];

      if(!fsafetranslate(char_value, translated_name))
      {
        reload_savegame(mzx_world, translated_name, &faded);

        if(faded)
          insta_fadeout();

        mzx_world->swapped = 1;
      }
      return 0;
    }

    case FOPEN_LOAD_ROBOT:
    {
      int new_size;
      char *new_program = assemble_file(char_value, &new_size);

      if(new_program)
      {
        Board *src_board = mzx_world->current_board;
        Robot *cur_robot;
        int robot_id = id;

        if(value != -1)
          robot_id = value;

        if(robot_id <= src_board->num_robots)
        {
          cur_robot = mzx_world->current_board->robot_list[robot_id];

          if(cur_robot != NULL)
          {
            reallocate_robot(cur_robot, new_size);
            clear_label_cache(cur_robot->label_list, cur_robot->num_labels);

            memcpy(cur_robot->program, new_program, new_size);
            free(new_program);
            cur_robot->cur_prog_line = 1;
            cur_robot->label_list =
             cache_robot_labels(cur_robot, &(cur_robot->num_labels));

            if(value == -1)
              return 1;
          }
        }
      }

      return 0;
    }

    case FOPEN_LOAD_BC:
    {
      FILE *bc_file = fsafeopen(char_value, "rb");
      Board *src_board = mzx_world->current_board;
      int new_size;

      if(bc_file)
      {
        Robot *cur_robot;
        int robot_id = id;

        if(value != -1)
          robot_id = value;

        if(robot_id <= src_board->num_robots)
        {
          cur_robot = mzx_world->current_board->robot_list[robot_id];

          if(cur_robot != NULL)
          {
            fseek(bc_file, 0, SEEK_END);
            new_size = ftell(bc_file);
            fseek(bc_file, 0, SEEK_SET);

            reallocate_robot(cur_robot, new_size);
            clear_label_cache(cur_robot->label_list, cur_robot->num_labels);

            fread(cur_robot->program, new_size, 1, bc_file);
            cur_robot->cur_prog_line = 1;
            cur_robot->label_list =
             cache_robot_labels(cur_robot, &(cur_robot->num_labels));

            fclose(bc_file);

            if(value == -1)
              return 1;
          }
        }

        fclose(bc_file);
      }

      return 0;
    }

    case FOPEN_SAVE_ROBOT:
    {
      Robot *cur_robot;
      Board *src_board = mzx_world->current_board;
      int robot_id = id;
      int allow_extras = mzx_world->conf.disassemble_extras;
      int base = mzx_world->conf.disassemble_base;

      if(value != -1)
        robot_id = value;

      cur_robot = mzx_world->current_board->robot_list[robot_id];

      if(robot_id <= src_board->num_robots)
      {
        cur_robot = mzx_world->current_board->robot_list[robot_id];

        if(cur_robot != NULL)
        {
          disassemble_file(char_value, cur_robot->program,
           allow_extras, base);
        }
      }

      return 0;
    }

    case FOPEN_SAVE_BC:
    {
      FILE *bc_file = fsafeopen(char_value, "wb");

      if(bc_file)
      {
        Robot *cur_robot;
        Board *src_board = mzx_world->current_board;
        int robot_id = id;

        if(value != -1)
          robot_id = value;

        if(robot_id <= src_board->num_robots)
        {
          cur_robot = mzx_world->current_board->robot_list[robot_id];

          if(cur_robot != NULL)
          {
            fwrite(cur_robot->program, cur_robot->program_length, 1, bc_file);
          }
        }

        fclose(bc_file);
      }

      return 0;
    }
  }

  return 0;
}

// I don't know yet if this works in pure C
const int num_builtin_counters =
 sizeof(builtin_counters) / sizeof(Function_counter);

int health_gateway(World *mzx_world, Counter *counter, char *name,
 int value, int id)
{
  // Make sure health is within 0 and max health
  if(value > mzx_world->health_limit)
  {
    value = mzx_world->health_limit;
  }
  if(value < 0)
  {
    value = 0;
  }

  // Trying to decrease health?
  if(value < counter->counter_value)
  {
    // Must not be invincible
    if(get_counter(mzx_world, "INVINCO", 0) <= 0)
    {
      Board *src_board = mzx_world->current_board;
      send_robot_def(mzx_world, 0, 13);
      if(src_board->restart_if_zapped)
      {
        int player_restart_x = mzx_world->player_restart_x;
        int player_restart_y = mzx_world->player_restart_y;
        int player_x = mzx_world->player_x;
        int player_y = mzx_world->player_y;

        //Restart since we were hurt
        if((player_restart_x != player_x) || (player_restart_y != player_y))
        {
          id_remove_top(mzx_world, player_x, player_y);
          player_x = player_restart_x;
          player_y = player_restart_y;
          id_place(mzx_world, player_x, player_y, 127, 0, 0);
          mzx_world->was_zapped = 1;
          mzx_world->player_x = player_x;
          mzx_world->player_y = player_y;
        }
      }
    }
    else
    {
      value = counter->counter_value;
    }
  }

  return value;
}

int lives_gateway(World *mzx_world, Counter *counter, char *name,
 int value, int id)
{
  // Make sure lives is within 0 and max lives
  if(value > mzx_world->lives_limit)
  {
    value = mzx_world->lives_limit;
  }
  if(value < 0)
  {
    value = 0;
  }

  return value;
}

int invinco_gateway(World *mzx_world, Counter *counter, char *name,
 int value, int id)
{
  if(!counter->counter_value)
  {
    mzx_world->saved_pl_color = *player_color;
  }
  else
  {
    if(!value)
    {
      clear_sfx_queue();
      play_sfx(mzx_world, 18);
      *player_color = mzx_world->saved_pl_color;
    }
  }

  return value;
}

// The other builtins simply can't go below 0

int builtin_gateway(World *mzx_world, Counter *counter, char *name,
 int value, int id)
{
  if(value < 0)
    return 0;
  else
    return value;
}

// Setup builtin gateway functions
void set_gateway(World *mzx_world, char *name,
 gateway_write_function function)
{
  int next;
  Counter *cdest = find_counter(mzx_world, name, &next);
  if(cdest)
  {
    cdest->counter_gateway_write = function;
  }
}

void initialize_gateway_functions(World *mzx_world)
{
  set_gateway(mzx_world, "HEALTH", health_gateway);
  set_gateway(mzx_world, "LIVES", lives_gateway);
  set_gateway(mzx_world, "INVINCO", invinco_gateway);
  set_gateway(mzx_world, "LOBOMBS", builtin_gateway);
  set_gateway(mzx_world, "HIBOMBS", builtin_gateway);
  set_gateway(mzx_world, "COINS", builtin_gateway);
  set_gateway(mzx_world, "GEMS", builtin_gateway);
  set_gateway(mzx_world, "TIME", builtin_gateway);
}

int match_function_counter(char *dest, char *src)
{
  int min_num_chars, max_num_chars, char_count;
  int difference;
  char cur_src, cur_dest;

  while(1)
  {
    cur_src = *src;
    cur_dest = *dest;
    // Skip wildcards

    if(cur_src == '?')
    {
      max_num_chars = *(src + 1) - '0';
      if(*(src + 2) == '-')
      {
        min_num_chars = max_num_chars;
        max_num_chars = *(src + 3) - '0';
        src += 4;
        cur_src = *src;
      }
      else
      {
        min_num_chars = 0;
        src += 2;
        cur_src = *src;
      }

      char_count = 0;

      while(((cur_dest >= '0') && (cur_dest <= '9')) ||
       (cur_dest == '-'))
      {
        char_count++;
        dest++;
        cur_dest = *dest;
      }

      if((char_count < min_num_chars) || (char_count > max_num_chars))
      {
        return 1;
      }
    }
    else

    if(cur_src == '*')
    {
      // Automatic match
      return 0;
    }

    if((cur_src | cur_dest) == 0)
    {
      // Both hit the null terminator
      return 0;
    }

    difference = (int)((cur_dest & 0xDF) - (cur_src & 0xDF));

    if(difference)
      return difference;

    src++;
    dest++;
  }

  return 0;
}

Counter *find_counter(World *mzx_world, char *name, int *next)
{
  int bottom = 0, top = (mzx_world->num_counters) - 1, middle = 0;
  int cmpval = 0;
  Counter **base = mzx_world->counter_list;
  Counter *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->counter_name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return current;
  }

  if(cmpval > 0)
    *next = middle + 1;
  else
    *next = middle;

  return NULL;
}

String *find_string(World *mzx_world, char *name, int *next)
{
  int bottom = 0, top = (mzx_world->num_strings) - 1, middle = 0;
  int cmpval = 0;
  String **base = mzx_world->string_list;
  String *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->string_name);

    if(cmpval > 0)
      bottom = middle + 1;
    else

    if(cmpval < 0)
      top = middle - 1;
    else
      return current;
  }

  if(cmpval > 0)
    *next = middle + 1;
  else
    *next = middle;

  return NULL;
}

Function_counter *find_function_counter(char *name)
{
  int bottom, top, middle;
  int first_letter = tolower(name[0]) * 2;
  int cmpval;
  Function_counter *base = builtin_counters;

  bottom = counter_first_letter[first_letter];
  top = counter_first_letter[first_letter + 1];

  if(bottom != -1)
  {
    while(bottom <= top)
    {
      middle = (top + bottom) / 2;
      cmpval = match_function_counter(name + 1, (base[middle]).counter_name + 1);

      if(cmpval > 0)
        bottom = middle + 1;
      else

      if(cmpval < 0)
        top = middle - 1;
      else
        return base + middle;
    }
  }

  return NULL;
}

void add_counter(World *mzx_world, char *name, int value, int position)
{
  int count = mzx_world->num_counters;
  int allocated = mzx_world->num_counters_allocated;
  Counter **base = mzx_world->counter_list;
  Counter *cdest;

  // Need a reallocation?
  if(count == allocated)
  {
    if(allocated)
      allocated *= 2;
    else
      allocated = MIN_COUNTER_ALLOCATE;

    mzx_world->counter_list =
     (Counter **)realloc(base, sizeof(Counter *) * allocated);
    base = mzx_world->counter_list;
    mzx_world->num_counters_allocated = allocated;
  }

  // Doesn't exist, so create it
  // First make space for it if it's not at the top
  if(position != count)
  {
    base += position;
    memmove((char *)(base + 1), (char *)base,
     (count - position) * sizeof(Counter *));
  }

  cdest = (Counter *)malloc(sizeof(Counter));
  // Allocate a string
  strcpy(cdest->counter_name, name);
  cdest->counter_value = value;
  cdest->counter_gateway_write = NULL;

  mzx_world->counter_list[position] = cdest;
  mzx_world->num_counters = count + 1;
}

void add_string(World *mzx_world, char *name, char *value, int position)
{
  int count = mzx_world->num_strings;
  int allocated = mzx_world->num_strings_allocated;
  String **base = mzx_world->string_list;
  String *sdest;

  // Need a reallocation?
  if(count == allocated)
  {
    if(allocated)
      allocated *= 2;
    else
      allocated = MIN_STRING_ALLOCATE;

    mzx_world->string_list =
     (String **)realloc(base, sizeof(String *) * allocated);
    mzx_world->num_strings_allocated = allocated;
    base = mzx_world->string_list;
  }

  // Doesn't exist, so create it
  // First make space for it if it's not at the top
  if(position != count)
  {
    base += position;
    memmove((char *)(base + 1), (char *)base,
     (count - position) * sizeof(String *));
  }

  sdest = (String *)malloc(sizeof(String));
  // Allocate a string
  strcpy(sdest->string_name, name);
  strcpy(sdest->string_contents, value);

  mzx_world->string_list[position] = (String *)sdest;
  mzx_world->num_strings = count + 1;
}

void set_counter(World *mzx_world, char *name, int value, int id)
{
  Function_counter *fdest;
  Counter *cdest;
  int next;

  fdest = find_function_counter(name);
  if(fdest && fdest->counter_function_write)
  {
    // Call write function
    fdest->counter_function_write(mzx_world, fdest, name, value, id);
  }
  else
  {
    if(strlen(name) > 14)
      name[14] = 0;

    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      // See if there's a gateway
      if(cdest->counter_gateway_write)
      {
        value = cdest->counter_gateway_write(mzx_world, cdest, name, value, id);
      }
      cdest->counter_value = value;
    }
    else
    {
      add_counter(mzx_world, name, value, next);
    }
  }
}

void get_string_size_offset(char *name, int *ssize, int *soffset)
{
  // First must strip off/encode offset/size values
  int offset_position = -1, size_position = -1;
  int current_position = 0;
  int offset = 0, size = 63;
  char *str_next;
  char current_char = name[0];

  while(current_char)
  {
    if(current_char == '#')
    {
      size_position = current_position;
    }
    else

    if(current_char == '+')
    {
      offset_position = current_position;
    }

    current_position++;
    current_char = name[current_position];
  }

  if(size_position != -1)
  {
    size = strtol(name + size_position + 1, NULL, 10);
    // Cut off the name here
    name[size_position] = 0;
  }

  if(offset_position != -1)
  {
    offset = strtol(name + offset_position + 1, &str_next, 10);
    name[offset_position] = 0;
  }

  if(offset > 62)
  {
    offset = 0;
  }

  if((offset + size) > 63)
  {
    size = 63 - offset;
  }

  *soffset = offset;
  *ssize = size;
}

void set_string(World *mzx_world, char *name, char *value, int id)
{
  String *sdest;
  int next;
  int size, offset;

  get_string_size_offset(name, &size, &offset);

  if(strlen(name) > 14)
    name[14] = 0;

  value[63] = 0;
  // Finding a string is like finding a normal counter, but it has to be casted
  sdest = find_string(mzx_world, name, &next);

  if(sdest)
  {
    FILE *output_file = mzx_world->output_file;
    // This is pretty awful, but necessary to have here, since this is actually
    // more of a read than a write
    if(!strncasecmp(value, "fwrite", 6) && output_file)
    {
      char *string_contents = sdest->string_contents;

      if(value[6])
      {
        int write_count = strtol(value + 6, NULL, 10);
        fwrite(string_contents, write_count, 1, output_file);
      }
      else
      {
        char terminate_char = '*';
        char current_char = string_contents[0];
        int write_pos = 0;

        while(current_char && (write_pos < 63))
        {
          fputc(current_char, output_file);
          write_pos++;
          current_char = string_contents[write_pos];
        }
        fputc(terminate_char, output_file);
      }
    }

    if(!set_function_string(mzx_world, value, id, sdest->string_contents))
    {
      // Copy the value, but make sure it doesn't run over first
      memcpy(sdest->string_contents + offset, value, size);
    }
  }
  else
  {
    char buffer[64];
    if(set_function_string(mzx_world, value, id, buffer))
      add_string(mzx_world, name, buffer, next);
    else
      add_string(mzx_world, name, value, next);
  }
}

int get_counter(World *mzx_world, char *name, int id)
{
  Function_counter *fdest;
  Counter *cdest;
  int next;

  fdest = find_function_counter(name);
  if(fdest && fdest->counter_function_read)
  {
    // Call read function
    if(fdest->counter_function_read)
      return fdest->counter_function_read(mzx_world, fdest, name, id);
    else
      return 0;
  }

  cdest = find_counter(mzx_world, name, &next);

  if(cdest)
    return cdest->counter_value;

  return 0;
}

// There's only a few "function strings" so they'll be handled manually
// here, for now.
char *set_function_string(World *mzx_world, char *name, int id, char *buffer)
{
  Board *src_board = mzx_world->current_board;
  FILE *input_file = mzx_world->input_file;

  // FIXME - Make terminating chars variable
  if(!strncasecmp(name, "fread", 5) && input_file)
  {
    if(name[5])
    {
      int read_count = strtol(name + 5, NULL, 10);

      if(read_count > 63)
        read_count = 63;

      fread(buffer, read_count, 1, input_file);
      buffer[read_count] = 0;
    }
    else
    {
      char terminate_char = '*';
      char current_char;
      int read_pos = 0;

      do
      {
        current_char = fgetc(input_file);
        if(current_char == terminate_char)
          buffer[read_pos] = 0;
        else
          buffer[read_pos] = current_char;

        read_pos++;
      } while((current_char != terminate_char) && (read_pos < 63));

    }

    return buffer;
  }

  if(!strcasecmp(name, "board_name"))
  {
    strcpy(buffer, src_board->board_name);
    return buffer;
  }

  if(!strcasecmp(name, "robot_name"))
  {
    strcpy(buffer, (src_board->robot_list[id])->robot_name);
    return buffer;
  }

  if(!strcasecmp(name, "mod_name"))
  {
    strcpy(buffer, mzx_world->real_mod_playing);
    return buffer;
  }

  if(!strcasecmp(name, "input"))
  {
    strcpy(buffer, src_board->input_string);
    return buffer;
  }

  // Okay, I implemented this stupid thing, you can all die.
  if(!strcasecmp(name, "board_scan"))
  {
    int board_width = src_board->board_width;

    load_string_board_direct(mzx_world, buffer, 63, 1, '*',
     src_board->level_param +
     get_counter(mzx_world, "board_x", id) +
     (get_counter(mzx_world, "board_y", id) * board_width),
     board_width, id);

    return buffer;
  }

  return NULL;
}

char *get_string(World *mzx_world, char *name, int id, char *buffer)
{
  String *sdest;
  int next;
  int size, offset;

  get_string_size_offset(name, &size, &offset);

  sdest = find_string(mzx_world, name, &next);

  if(sdest)
  {
    memcpy(buffer, sdest->string_contents + offset, size);
    buffer[size] = 0;
    return buffer;
  }

  strcpy(buffer, "(empty)");
  return NULL;
}

void inc_counter(World *mzx_world, char *name, int value, int id)
{
  Function_counter *fdest;
  Counter *cdest;
  int current_value;
  int next;

  fdest = find_function_counter(name);
  if(fdest && fdest->counter_function_read && fdest->counter_function_write)
  {
    current_value =
     fdest->counter_function_read(mzx_world, fdest, name, id);
    current_value += value;
    fdest->counter_function_write(mzx_world, fdest, name, current_value, id);
  }
  else
  {
    if(strlen(name) > 14)
      name[14] = 0;

    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      value += cdest->counter_value;
      if(cdest->counter_gateway_write)
      {
        value = cdest->counter_gateway_write(mzx_world, cdest, name, value, id);
      }
      cdest->counter_value = value;
    }
    else
    {
      add_counter(mzx_world, name, value, next);
    }
  }
}

void inc_string(World *mzx_world, char *name, char *value, int id)
{
  String *sdest;
  int next;

  value[63] = 0;

  if(strlen(name) > 14)
    name[14] = 0;

  sdest = find_string(mzx_world, name, &next);

  if(sdest)
  {
    // Concatenate
    int len_src = strlen(sdest->string_contents);
    int len_dest = strlen(value);

    if(len_dest + len_src > 63)
    {
      len_dest = 63 - len_src;
    }

    memcpy(sdest->string_contents + len_src, value, len_dest);
    sdest->string_contents[len_src + len_dest] = 0;
  }
  else
  {
    add_string(mzx_world, name, value, next);
  }
}

void dec_counter(World *mzx_world, char *name, int value, int id)
{
  Function_counter *fdest;
  Counter *cdest;
  int current_value;
  int next;

  fdest = find_function_counter(name);
  if(fdest && fdest->counter_function_read && fdest->counter_function_write)
  {
    current_value =
     fdest->counter_function_read(mzx_world, fdest, name, id);
    current_value -= value;
    fdest->counter_function_write(mzx_world, fdest, name, current_value, id);
  }
  else
  {
    if(strlen(name) > 14)
      name[14] = 0;

    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      value = cdest->counter_value - value;
      if(cdest->counter_gateway_write)
      {
        value = cdest->counter_gateway_write(mzx_world, cdest, name, value, id);
      }
      cdest->counter_value = value;
    }
    else
    {
      add_counter(mzx_world, name, -value, next);
    }
  }
}

void dec_string_int(World *mzx_world, char *name, int value, int id)
{
  String *sdest;
  int next;

  sdest = find_string(mzx_world, name, &next);

  if(sdest)
  {
    // Move null terminator down N chars
    int len_src = strlen(sdest->string_contents);
    if(len_src < value)
    {
      sdest->string_contents[0] = 0;
    }
    else
    {
      sdest->string_contents[len_src - value] = 0;
    }
  }
}

void mul_counter(World *mzx_world, char *name, int value, int id)
{
  Function_counter *fdest;
  Counter *cdest;
  int current_value;
  int next;

  fdest = find_function_counter(name);
  if(fdest && fdest->counter_function_read && fdest->counter_function_write)
  {
    current_value =
     fdest->counter_function_read(mzx_world, fdest, name, id);
    current_value *= value;
    fdest->counter_function_write(mzx_world, fdest, name, current_value, id);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      value *= cdest->counter_value;
      if(cdest->counter_gateway_write)
      {
        value = cdest->counter_gateway_write(mzx_world, cdest, name, value, id);
      }
      cdest->counter_value = value;
    }
  }
}

void div_counter(World *mzx_world, char *name, int value, int id)
{
  Function_counter *fdest;
  Counter *cdest;
  int current_value;
  int next;

  if(value == 0)
    return;

  fdest = find_function_counter(name);
  if(fdest && fdest->counter_function_read && fdest->counter_function_write)
  {
    current_value =
     fdest->counter_function_read(mzx_world, fdest, name, id);
    current_value /= value;
    fdest->counter_function_write(mzx_world, fdest, name, current_value, id);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      value = cdest->counter_value / value;
      if(cdest->counter_gateway_write)
      {
        value = cdest->counter_gateway_write(mzx_world, cdest, name, value, id);
      }
      cdest->counter_value = value;
    }
  }
}

void mod_counter(World *mzx_world, char *name, int value, int id)
{
  Function_counter *fdest;
  Counter *cdest;
  int current_value;
  int next;

  if(value == 0)
    return;

  fdest = find_function_counter(name);
  if(fdest && fdest->counter_function_read && fdest->counter_function_write)
  {
    current_value =
     fdest->counter_function_read(mzx_world, fdest, name, id);
    current_value %= value;
    fdest->counter_function_write(mzx_world, fdest, name, current_value, id);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      cdest->counter_value %= value;
    }
  }
}

void load_string_board(World *mzx_world, char *expression,
 int w, int h, char l, char *src, int width, int id)
{
  char t_buf[64];
  load_string_board_direct(mzx_world, t_buf, w, h, l,
   src, width, id);
  set_string(mzx_world, expression, t_buf, id);
}

void load_string_board_direct(World *mzx_world,
 char *buffer, int w, int h, char l, char *src, int width, int id)
{
  int i;
  int wl = w;
  buffer[63] = '\0';
  char *t_pos = buffer;

  if((w * h) > 63)
  {
    if(w > 63)
    {
      w = 63;
      h = 1;
    }
    else
    {
      h = (63 / w) + 1;
      wl = 63 % w;
    }
  }

  for(i = 0; i < (h - 1); i++)
  {
    memcpy(t_pos, src, w);
    t_pos += w;
    src += width;
  }
  memcpy(t_pos, src, wl);
  t_pos += wl;
  *t_pos = '\0';
  if(l)
  {
    for(i = 0; i < ((w * h) + wl); i++)
    {
      if(buffer[i] == l)
      {
        buffer[i] = '\0';
        break;
      }
    }
  }
}

int translate_coordinates(char *src, unsigned int *x, unsigned int *y)
{
  char *next;

  *x = strtol(src, &next, 10);
  if(*next == ',')
  {
    *y = strtol(next + 1, NULL, 10);
    return 0;
  }
  else
  {
    return -1;
  }
}

int is_string(char *buffer)
{
  // For something to be a string it has to start with a $ and
  // not have a . in its name
  if(buffer[0] == '$')
  {
    int pos = 1;
    char current_char = buffer[1];

    while(current_char)
    {
      if(current_char == '.')
      {
        return 0;
      }
      pos++;
      current_char = buffer[pos];
    }
    return 1;
  }
  return 0;
}
