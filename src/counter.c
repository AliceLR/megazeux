/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

// New counter.cpp. Sorted lists make for faster searching.
// Builtins are also cleaned up by being put on a seperate list.

#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
#include "audio.h"
#include "rasm.h"
#include "fsafeopen.h"
#include "intake.h"
#include "robot.h"
#include "sprite.h"
#include "world.h"
#include "util.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Please only use string literals with this, thanks.

#define special_name(n)                                         \
  ((src_length == (sizeof(n) - 1)) &&                           \
   !strncasecmp(src_value, n, sizeof(n) - 1))                   \

#define special_name_partial(n)                                 \
  ((src_length >= (int)(sizeof(n) - 1)) &&                      \
   !strncasecmp(src_value, n, sizeof(n) - 1))                   \

struct function_counter
{
  const char *const name;
  int minimum_version;
  int (*const function_read)(struct world *mzx_world,
   const struct function_counter *counter, const char *name, int id);
  void (*const function_write)(struct world *mzx_world,
   const struct function_counter *counter, const char *name, int value,
   int id);
};

static unsigned int get_board_x_board_y_offset(struct world *mzx_world, int id)
{
  int board_x = get_counter(mzx_world, "board_x", id);
  int board_y = get_counter(mzx_world, "board_y", id);

  board_x = CLAMP(board_x, 0, mzx_world->current_board->board_width);
  board_y = CLAMP(board_y, 0, mzx_world->current_board->board_height);

  return board_y * mzx_world->current_board->board_width + board_x;
}

static int local_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int local_num = 0;

  if(name[5])
    local_num = (strtol(name + 5, NULL, 10) - 1) & 31;

  return (mzx_world->current_board->robot_list[id])->local[local_num];
}

static void local_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int local_num = 0;

  if(name[5])
    local_num = (strtol(name + 5, NULL, 10) - 1) & 31;

  (mzx_world->current_board->robot_list[id])->local[local_num] = value;
}

static int loopcount_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->loop_count;
}

static void loopcount_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->loop_count = value;
}

static int playerdist_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  struct robot *cur_robot = src_board->robot_list[id];

  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int this_x = cur_robot->xpos;
  int this_y = cur_robot->ypos;

  return abs(player_x - this_x) + abs(player_y - this_y);
}

static int sin_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(sin(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

static int cos_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(cos(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

static int tan_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int theta = strtol(name + 3, NULL, 10);
  return (int)(tan(theta * (2 * M_PI) / mzx_world->c_divisions)
   * mzx_world->multiplier);
}

static int asin_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((asinf((float)val / mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

static int acos_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((acosf((float)val / mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

static int atan_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)((atan2f((float)val, (float)mzx_world->divider) *
   mzx_world->c_divisions) / (2 * M_PI));
}

static int c_divisions_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->c_divisions;
}

static int divider_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->divider;
}

static int multiplier_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->multiplier;
}

static void c_divisions_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->c_divisions = value;
}

static void divider_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->divider = value;
}

static void multiplier_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->multiplier = value;
}

static int thisx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->mid_prefix == 2)
    return (mzx_world->current_board->robot_list[id])->xpos -
     mzx_world->player_x;

  return (mzx_world->current_board->robot_list[id])->xpos;
}

static int thisy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->mid_prefix == 2)
    return (mzx_world->current_board->robot_list[id])->ypos -
     mzx_world->player_y;

  return (mzx_world->current_board->robot_list[id])->ypos;
}

static int playerx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->player_x;
}

static int playery_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->player_y;
}

static int this_char_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->robot_char;
}

static int this_color_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  struct robot *cur_robot = src_board->robot_list[id];
  int x = cur_robot->xpos;
  int y = cur_robot->ypos;
  int offset = x + (y * src_board->board_width);

  return src_board->level_color[offset];
}

static int sqrt_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 4, NULL, 10);
  return (int)(sqrt(val));
}

static int abs_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int val = strtol(name + 3, NULL, 10);
  return abs(val);
}

static int playerfacedir_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->player_last_dir >> 4;
}

static void playerfacedir_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *src_board = mzx_world->current_board;

  if(value < 0)
    value = 0;

  if(value > 3)
    value = 3;

  src_board->player_last_dir =
   (src_board->player_last_dir & 0x0F) | (value << 4);
}

static int playerlastdir_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->player_last_dir & 0x0F;
}

static void playerlastdir_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *src_board = mzx_world->current_board;

  if(value < 0)
    value = 0;

  if(value > 4)
    value = 4;

  src_board->player_last_dir =
   (src_board->player_last_dir & 0xF0) | value;
}

static int horizpld_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return abs(mzx_world->player_x -
   (mzx_world->current_board->robot_list[id])->xpos);
}

static int vertpld_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return abs(mzx_world->player_y -
   (mzx_world->current_board->robot_list[id])->ypos);
}

static int board_char_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  return get_id_char(mzx_world->current_board, offset);
}

static int board_color_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  return get_id_color(mzx_world->current_board, offset);
}

static int board_w_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->board_width;
}

static int board_h_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->board_height;
}

static int board_id_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  return mzx_world->current_board->level_id[offset];
}

static void board_id_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  struct board *src_board = mzx_world->current_board;
  char cvalue = value;

  if((cvalue < SENSOR) && (src_board->level_id[offset] < SENSOR))
    src_board->level_id[offset] = cvalue;
}

static int board_param_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  return mzx_world->current_board->level_param[offset];
}

static void board_param_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int offset = get_board_x_board_y_offset(mzx_world, id);
  struct board *src_board = mzx_world->current_board;

  if(src_board->level_id[offset] < 122)
    src_board->level_param[offset] = value;
}

static int red_value_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  return get_red_component(cur_color);
}

static int green_value_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  return get_green_component(cur_color);
}

static int blue_value_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  return get_blue_component(cur_color);
}

static void red_value_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  set_red_component(cur_color, value);
  pal_update = true;
}

static void green_value_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  set_green_component(cur_color, value);
  pal_update = true;
}

static void blue_value_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = get_counter(mzx_world, "current_color", id) & 0xFF;
  set_blue_component(cur_color, value);
  pal_update = true;
}

static int overlay_mode_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->overlay_mode;
}

static int mzx_speed_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->mzx_speed;
}

static void mzx_speed_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value == 0)
  {
    mzx_world->lock_speed = 0;
  }
  else if(value >= 1 && value <= 9)
  {
    mzx_world->mzx_speed = value;
    mzx_world->lock_speed = 1;
  }
}

static int overlay_char_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "overlay_x", id) +
   (get_counter(mzx_world, "overlay_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return src_board->overlay[offset];

  return -1;
}

static int overlay_color_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  int offset = get_counter(mzx_world, "overlay_x", id) +
   (get_counter(mzx_world, "overlay_y", id) * src_board->board_width);
  int board_size = src_board->board_width * src_board->board_height;

  if((offset >= 0) && (offset < board_size))
    return src_board->overlay_color[offset];

  return -1;
}

static int smzx_mode_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_screen_mode();
}

static void smzx_mode_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  set_screen_mode(value);
}

static int smzx_r_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  return get_red_component(cur_color);
}

static int smzx_g_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  return get_green_component(cur_color);
}

static int smzx_b_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  return get_blue_component(cur_color);
}

static void smzx_r_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  set_red_component(cur_color, value);
  pal_update = true;
}

static void smzx_g_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  set_green_component(cur_color, value);
  pal_update = true;
}

static void smzx_b_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int cur_color = strtol(name + 6, NULL, 10);
  set_blue_component(cur_color, value);
  pal_update = true;
}

static int spr_clist_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int clist_num = strtol(name + 9, NULL, 10) & (MAX_SPRITES - 1);
  return mzx_world->collision_list[clist_num];
}

static int spr_collisions_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->collision_count;
}

static int spr_num_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->sprite_num;
}

static int spr_cx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->col_x;
}

static int spr_cy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->col_y;
}

static int spr_width_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->width;
}

static int spr_height_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->height;
}

static int spr_refx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->ref_x;
}

static int spr_refy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->ref_y;
}

static int spr_x_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->x;
}

static int spr_y_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->y;
}

static int spr_cwidth_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->col_width;
}

static int spr_cheight_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  return (mzx_world->sprite_list[spr_num])->col_height;
}

static void spr_num_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  value &= 0xFF;

  mzx_world->sprite_num = (unsigned int)value;
}

static void spr_yorder_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->sprite_y_order = value & 1;
}

static void spr_ccheck_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *cur_sprite = mzx_world->sprite_list[spr_num];

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

static void spr_clist_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *cur_sprite = mzx_world->sprite_list[spr_num];
  sprite_colliding_xy(mzx_world, cur_sprite, cur_sprite->x,
   cur_sprite->y);
}

static void spr_cx_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->col_x = value;
}

static void spr_cy_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->col_y = value;
}

static void spr_height_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->height = value;
}

static void spr_width_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->width = value;
}

static void spr_refx_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);

  if(value < 0)
    value = 0;

  (mzx_world->sprite_list[spr_num])->ref_x = value;
}

static void spr_refy_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);

  if(value < 0)
    value = 0;

  (mzx_world->sprite_list[spr_num])->ref_y = value;
}

static void spr_x_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->x = value;
}

static void spr_y_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->y = value;
}

static void spr_vlayer_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_VLAYER;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_VLAYER;
}

static void spr_static_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_STATIC;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_STATIC;
}

static void spr_overlaid_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  if(value)
    (mzx_world->sprite_list[spr_num])->flags |= SPRITE_OVER_OVERLAY;
  else
    (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_OVER_OVERLAY;
}

static void spr_off_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->flags &= ~SPRITE_INITIALIZED;
}

static void spr_swap_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *src = mzx_world->sprite_list[spr_num];
  struct sprite *dest = mzx_world->sprite_list[value];
  mzx_world->sprite_list[value] = src;
  mzx_world->sprite_list[spr_num] = dest;
}

static void spr_cwidth_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->col_width = value;
}

static void spr_cheight_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  (mzx_world->sprite_list[spr_num])->col_height = value;
}

static void spr_setview_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *src_board = mzx_world->current_board;
  int n_scroll_x, n_scroll_y;
  int spr_num = strtol(name + 3, NULL, 10) & (MAX_SPRITES - 1);
  struct sprite *cur_sprite = mzx_world->sprite_list[spr_num];
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

static int bullettype_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->bullet_type;
}

static void bullettype_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->bullet_type = value;
}

static int inputsize_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (int)mzx_world->current_board->input_size;
}

static void inputsize_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->current_board->input_size = value;
}

static int input_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->num_input;
}

static void input_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->current_board->num_input = value;
  sprintf(mzx_world->current_board->input_string, "%d", value);
}

static int key_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return toupper(get_last_key(keycode_internal));
}

static void key_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  force_last_key(keycode_internal, toupper(value));
}

static int keyn_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int key_num = strtol(name + 3, NULL, 10);
  return get_key_status(keycode_pc_xt, key_num);
}

static int key_code_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_key(keycode_pc_xt);
}

static int key_pressed_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_key(keycode_internal);
}

static int key_release_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_last_key_released(keycode_pc_xt);
}

static int scrolledx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return scroll_x;
}

static int scrolledy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return scroll_y;
}

static int timereset_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->current_board->time_limit;
}

static void timereset_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->current_board->time_limit = CLAMP(value, 0, 32767);
}

static int date_day_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  time_t e_time = time(NULL);
  struct tm *t = localtime(&e_time);
  return t->tm_mday;
}

static int date_year_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  time_t e_time = time(NULL);
  struct tm *t = localtime(&e_time);
  return t->tm_year + 1900;
}

static int date_month_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  time_t e_time = time(NULL);
  struct tm *t = localtime(&e_time);
  return t->tm_mon + 1;
}

static int time_hours_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  time_t e_time = time(NULL);
  struct tm *t = localtime(&e_time);
  return t->tm_hour;
}

static int time_minutes_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  time_t e_time = time(NULL);
  struct tm *t = localtime(&e_time);
  return t->tm_min;
}

static int time_seconds_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  time_t e_time = time(NULL);
  struct tm *t = localtime(&e_time);
  return t->tm_sec;
}

static int translate_coordinates(const char *src, unsigned int *x,
                                 unsigned int *y)
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

static int vch_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int x = 0, y = 0;
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

static void vch_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int x = 0, y = 0;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    mzx_world->vlayer_chars[offset] = value;
  }
}

static int vco_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  unsigned int x = 0, y = 0;
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

static void vco_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int x = 0, y = 0;
  unsigned int vlayer_width = mzx_world->vlayer_width;
  unsigned int vlayer_height = mzx_world->vlayer_height;
  translate_coordinates(name + 3, &x, &y);

  if((x < vlayer_width) && (y < vlayer_height))
  {
    int offset = x + (y * vlayer_width);
    mzx_world->vlayer_colors[offset] = value;
  }
}

static int char_byte_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return ec_read_byte(get_counter(mzx_world, "CHAR", id),
   get_counter(mzx_world, "BYTE", id));
}

static void char_byte_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  ec_change_byte(get_counter(mzx_world, "CHAR", id),
   get_counter(mzx_world, "BYTE", id), value);
}

static int pixel_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int pixel_x = CLAMP(get_counter(mzx_world, "CHAR_X", id), 0, 256);
  int pixel_y = CLAMP(get_counter(mzx_world, "CHAR_Y", id), 0, 112);
  char sub_x, sub_y, current_byte, current_char;
  int pixel_mask;

  sub_x = pixel_x % 8;
  sub_y = pixel_y % 14;
  current_char = ((pixel_y / 14) * 32) + (pixel_x / 8);
  current_byte = ec_read_byte(current_char, sub_y);
  pixel_mask = (128 >> sub_x);

  return (current_byte & pixel_mask) >> (7 - sub_x);
}

static void pixel_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  int pixel_x = CLAMP(get_counter(mzx_world, "CHAR_X", id), 0, 256);
  int pixel_y = CLAMP(get_counter(mzx_world, "CHAR_Y", id), 0, 112);
  char sub_x, sub_y, current_byte, current_char;

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

static int int2bin_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int place = get_counter(mzx_world, "BIT_PLACE", id) & 15;
  return ((get_counter(mzx_world, "INT", id) >> place) & 1);
}

static void int2bin_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  unsigned int integer;
  int place;
  place = (get_counter(mzx_world, "BIT_PLACE", id) & 15);
  integer = get_counter(mzx_world, "INT", id);

  switch(value & 2)
  {
    case 0:
    {
      // clear
      integer &= ~(1 << place);
      break;
    }

    case 1:
    {
      // set
      integer |= (1 << place);
      break;
    }

    case 2:
    {
      // toggle
      integer ^= (1 << place);
      break;
    }
  }

  set_counter(mzx_world, "INT", integer, id);
}

static int commands_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->commands;
}

static void commands_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->commands = value;
}

static int fread_open_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FREAD;
  return 0;
}

static int fwrite_open_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FWRITE;
  return 0;
}

static int fwrite_append_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FAPPEND;
  return 0;
}

static int fwrite_modify_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_FMODIFY;
  return 0;
}

static int smzx_palette_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SMZX_PALETTE;
  return 0;
}

static int load_game_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_GAME;
  return 0;
}

static int save_game_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_GAME;
  return 0;
}

static int save_world_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_WORLD;
  return 0;
}

static int load_robot_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_ROBOT;
  if(name[10])
    return strtol(name + 10, NULL, 10);

  return -1;
}

static int load_bc_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_LOAD_BC;
  if(name[7])
    return strtol(name + 7, NULL, 10);

  return -1;
}

#ifndef CONFIG_DEBYTECODE

static int save_robot_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_ROBOT;
  if(name[10])
    return strtol(name + 10, NULL, 10);

  return -1;
}

static int save_bc_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  mzx_world->special_counter_return = FOPEN_SAVE_BC;
  if(name[7])
    return strtol(name + 7, NULL, 10);

  return -1;
}

#endif // CONFIG_DEBYTECODE

static int fread_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->input_file)
    return fgetc(mzx_world->input_file);
  return -1;
}

static int fread_counter_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->input_file)
  {
    if(mzx_world->version < 0x0252)
      return fgetw(mzx_world->input_file);
    else
      return fgetd(mzx_world->input_file);
  }
  return -1;
}

static int fread_pos_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->input_file)
    return ftell(mzx_world->input_file);
  else
    return -1;
}

static void fread_pos_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(mzx_world->input_file)
  {
    if(value == -1)
      fseek(mzx_world->input_file, 0, SEEK_END);
    else
      fseek(mzx_world->input_file, value, SEEK_SET);
  }
}

static int fwrite_pos_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  if(mzx_world->output_file)
    return ftell(mzx_world->output_file);
  else
    return -1;
}

static void fwrite_pos_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(mzx_world->output_file)
  {
    if(value == -1)
      fseek(mzx_world->output_file, 0, SEEK_END);
    else
      fseek(mzx_world->output_file, value, SEEK_SET);
  }
}

static void fwrite_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(mzx_world->output_file)
    fputc(value, mzx_world->output_file);
}

static void fwrite_counter_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(mzx_world->output_file)
  {
    if(mzx_world->version < 0x0252)
      fputw(value, mzx_world->output_file);
    else
      fputd(value, mzx_world->output_file);
  }
}

static int lava_walk_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return (mzx_world->current_board->robot_list[id])->can_lavawalk;
}

static void lava_walk_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  (mzx_world->current_board->robot_list[id])->can_lavawalk = value;
}

static int robot_id_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return id;
}

static int robot_id_n_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_robot_id(mzx_world->current_board, name + 9);
}

static int rid_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_robot_id(mzx_world->current_board, name + 3);
}

static int r_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  struct board *src_board = mzx_world->current_board;
  char *next;
  unsigned int robot_num = strtol(name + 1, &next, 10);

  if((robot_num <= (unsigned int)src_board->num_robots) &&
   (src_board->robot_list[robot_num] != NULL))
  {
    return get_counter(mzx_world, next + 1, robot_num);
  }

  return -1;
}

static void r_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  struct board *src_board = mzx_world->current_board;
  char *next;
  unsigned int robot_num = strtol(name + 1, &next, 10);

  if((robot_num <= (unsigned int)src_board->num_robots) &&
   (src_board->robot_list[robot_num] != NULL))
  {
    set_counter(mzx_world, next + 1, value, robot_num);
  }
}

static int vlayer_height_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->vlayer_height;
}

static int vlayer_size_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->vlayer_size;
}

static int vlayer_width_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return mzx_world->vlayer_width;
}

static void vlayer_height_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value <= 0)
    value = 1;
  if(value > mzx_world->vlayer_size)
    value = mzx_world->vlayer_size;
  mzx_world->vlayer_height = value;
  mzx_world->vlayer_width = mzx_world->vlayer_size / value;
}

static void vlayer_size_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value <= 0)
    value = 1;

  if(value <= MAX_BOARD_SIZE)
  {
    int vlayer_width = mzx_world->vlayer_width;
    int vlayer_height = mzx_world->vlayer_height;

    mzx_world->vlayer_chars = crealloc(mzx_world->vlayer_chars, value);
    mzx_world->vlayer_colors = crealloc(mzx_world->vlayer_colors, value);
    mzx_world->vlayer_size = value;

    if(vlayer_width * vlayer_height > value)
    {
      vlayer_height = value / vlayer_width;
      mzx_world->vlayer_height = vlayer_height;

      if(vlayer_height == 0)
      {
        mzx_world->vlayer_width = value;
        mzx_world->vlayer_height = 1;
      }
    }
  }
}

static void vlayer_width_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value <= 0)
    value = 1;
  if(value > mzx_world->vlayer_size)
    value = mzx_world->vlayer_size;
  mzx_world->vlayer_width = value;
  mzx_world->vlayer_height = mzx_world->vlayer_size / value;
}

static int buttons_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int buttons_formatted, raw_status = get_mouse_status();

  // Initially just grab the left button status
  buttons_formatted = raw_status & MOUSE_BUTTON(MOUSE_BUTTON_LEFT);

  /* For legacy reasons, MegaZeux wants the second bit to be RIGHT, and
   * the third bit to be MIDDLE, but SDL has these swapped around.
   */
  if (raw_status & MOUSE_BUTTON(MOUSE_BUTTON_MIDDLE))
    buttons_formatted |= MOUSE_BUTTON(MOUSE_BUTTON_RIGHT);
  if (raw_status & MOUSE_BUTTON(MOUSE_BUTTON_RIGHT))
    buttons_formatted |= MOUSE_BUTTON(MOUSE_BUTTON_MIDDLE);

  return buttons_formatted;
}

static int mousex_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_mouse_x();
}

static int mousey_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_mouse_y();
}

static void mousex_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value > 79)
    value = 79;

  if(value < 0)
    value = 0;

  warp_mouse_x(value);
}

static void mousey_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value > 24)
    value = 24;

  if(value < 0)
    value = 0;

  warp_mouse_y(value);
}

static int mousepx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_real_mouse_x();
}

static int mousepy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_real_mouse_y();
}

static void mousepx_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value > 639)
    value = 639;

  if(value < 0)
    value = 0;

  warp_real_mouse_x(value);
}

static void mousepy_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if(value > 349)
    value = 349;

  if(value < 0)
    value = 0;

  warp_real_mouse_y(value);
}

static int mboardx_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return get_mouse_x() +
   scroll_x - mzx_world->current_board->viewport_x;
}

static int mboardy_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  int scroll_x, scroll_y;
  calculate_xytop(mzx_world, &scroll_x, &scroll_y);
  return get_mouse_y() +
   scroll_y - mzx_world->current_board->viewport_y;
}

static void bimesg_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  mzx_world->bi_mesg_status = value & 1;
}

static struct string *find_string(struct world *mzx_world, const char *name,
 int *next)
{
  int bottom = 0, top = (mzx_world->num_strings) - 1, middle = 0;
  int cmpval = 0;
  struct string **base = mzx_world->string_list;
  struct string *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else

    if(cmpval < 0)
    {
      top = middle - 1;
    }
    else
    {
      *next = middle;
      return current;
    }
  }

  if(cmpval > 0)
    *next = middle + 1;
  else
    *next = middle;

  return NULL;
}

static int str_num_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  char *dot_ptr = (char *)strrchr(name + 1, '.');
  struct string src;

  // User may have provided $str.N or $str.length explicitly
  if(dot_ptr)
  {
    *dot_ptr = 0;
    dot_ptr++;

    if(!strncasecmp(dot_ptr, "length", 6))
    {
      // str.length handler
      int next;

      // If the user's string is found, return the length of it
      struct string *src = find_string(mzx_world, name, &next);

      *(dot_ptr - 1) = '.';

      if(src)
        return (int)src->length;
    }
    else
    {
      // char offset handler
      unsigned int str_num = strtol(dot_ptr, NULL, 10);
      bool found_string = get_string(mzx_world, name, &src, id);

      *(dot_ptr - 1) = '.';

      if(found_string)
      {
        // If we're in bounds return the char at this offset
        if(str_num < src.length)
          return (int)src.value[str_num];
      }
    }
  }
  else

  // Otherwise fall back to looking up a regular string
  if(get_string(mzx_world, name, &src, id))
  {
    char *n_buffer = cmalloc(src.length + 1);
    long ret;

    memcpy(n_buffer, src.value, src.length);
    n_buffer[src.length] = 0;
    ret = strtol(n_buffer, NULL, 10);

    free(n_buffer);
    return (int)ret;
  }

  // The string wasn't found or the request was out of bounds
  return 0;
}

static struct string *add_string_preallocate(struct world *mzx_world,
 const char *name, size_t length, int position)
{
  int count = mzx_world->num_strings;
  int allocated = mzx_world->num_strings_allocated;
  size_t name_length = strlen(name);
  struct string **base = mzx_world->string_list;
  struct string *dest;

  // Need a reallocation?
  if(count == allocated)
  {
    if(allocated)
      allocated *= 2;
    else
      allocated = MIN_STRING_ALLOCATE;

    mzx_world->string_list =
     crealloc(base, sizeof(struct string *) * allocated);
    mzx_world->num_strings_allocated = allocated;
    base = mzx_world->string_list;
  }

  // Doesn't exist, so create it
  // First make space for it if it's not at the top
  if(position != count)
  {
    base += position;
    memmove((char *)(base + 1), (char *)base,
     (count - position) * sizeof(struct string *));
  }

  // Allocate a string with room for the name and initial value
  dest = cmalloc(sizeof(struct string) + name_length + length);

  // Copy in the name, including NULL terminator.
  strcpy(dest->name, name);

  // Copy in the value. It is NOT null terminated!
  dest->allocated_length = length;
  dest->length = length;

  dest->value = dest->storage_space + name_length;
  if(length > 0)
    memset(dest->value, ' ', length);

  mzx_world->string_list[position] = dest;
  mzx_world->num_strings = count + 1;

  return dest;
}

static struct string *reallocate_string(struct world *mzx_world,
 struct string *src, int pos, size_t length)
{
  // Find the base length (take out the current length)
  int base_length = (int)(src->value - (char *)src);

  src = crealloc(src, base_length + length);
  src->value = (char *)src + base_length;

  // any new bits of the string should be space filled
  // versions up to and including 2.81h used to fill this with garbage
  if(src->allocated_length < length)
  {
    memset(&src->value[src->allocated_length], ' ',
     length - src->allocated_length);
  }

  src->allocated_length = length;

  mzx_world->string_list[pos] = src;
  return src;
}

static void force_string_length(struct world *mzx_world, const char *name,
 int next, struct string **str, size_t *length)
{
  if(*length > MAX_STRING_LEN)
    *length = MAX_STRING_LEN;

  if(!*str)
    *str = add_string_preallocate(mzx_world, name, *length, next);
  else if(*length > (*str)->allocated_length)
    *str = reallocate_string(mzx_world, *str, next, *length);
}

static void force_string_splice(struct world *mzx_world, const char *name,
 int next, struct string **str, size_t s_length, size_t offset,
 bool offset_specified, size_t *size, bool size_specified)
{
  force_string_length(mzx_world, name, next, str, &s_length);

  if((*size == 0 && !size_specified) || *size > s_length)
    *size = s_length;

  if((offset == 0 && !offset_specified) || (offset + *size > (*str)->length))
  {
    if(offset + *size > (*str)->length)
    {
      size_t length = offset + *size;
      force_string_length(mzx_world, name, next, str, &length);
      (*str)->length = length;
    }
    else
      (*str)->length = offset + *size;
  }
}

static void force_string_copy(struct world *mzx_world, const char *name,
 int next, struct string **str, size_t s_length, size_t offset,
 bool offset_specified, size_t *size, bool size_specified, char *src)
{
  force_string_splice(mzx_world, name, next, str, s_length,
   offset, offset_specified, size, size_specified);
  if(offset <= (*str)->length - *size)
    memcpy((*str)->value + offset, src, *size);
}

static void force_string_move(struct world *mzx_world, const char *name,
 int next, struct string **str, size_t s_length, size_t offset,
 bool offset_specified, size_t *size, bool size_specified, char *src)
{
  bool src_dest_match = false;
  ssize_t off = 0;

  if(*str)
  {
    off = (ssize_t)(src - (*str)->value);
    if(off >= 0 && (unsigned int)off <= (*str)->length)
      src_dest_match = true;
  }

  force_string_splice(mzx_world, name, next, str, s_length,
   offset, offset_specified, size, size_specified);

  if(src_dest_match)
    src = (*str)->value + off;

  if(offset <= (*str)->length - *size)
    memmove((*str)->value + offset, src, *size);
}

static void str_num_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  char *dot_ptr = strrchr(name + 1, '.');

  // User may have provided $str.N notation "write char at offset"
  if(dot_ptr)
  {
    size_t old_length, new_length;
    int next, write_value = false;
    struct string *src;
    int str_num = 0;

    *dot_ptr = 0;
    dot_ptr++;

    /* As a special case, alter the string's length if '$str.length'
     * is written to.
     */
    if(!strncasecmp(dot_ptr, "length", 6))
    {
      // Ignore impossible lengths
      if(value < 0)
        return;

      // Writing to length from a non-existent string has no effect
      src = find_string(mzx_world, name, &next);
      if(!src)
        return;

      new_length = value + 1;
    }
    else
    {
      // Extract the offset within the string to write to
      str_num = strtol(dot_ptr, NULL, 10);
      if(str_num < 0)
        return;

      // Tentatively increase the string's length to cater for this write
      new_length = str_num + 1;

      src = find_string(mzx_world, name, &next);
      write_value = true;
    }

    if(new_length > MAX_STRING_LEN)
      return;

    /* As a kind of unnecessary optimisation, if the string already exists
     * and we're asking to extend its length, increase the length by a power
     * of two rather than just by the amount necessary.
     */
    if(src != NULL)
    {
      old_length = src->allocated_length;

      if(new_length > old_length)
      {
        unsigned int i;

        for(i = 1 << 31; i != 0; i >>= 1)
          if(new_length & i)
            break;

        new_length = i << 1;
      }
    }

    force_string_length(mzx_world, name, next, &src, &new_length);

    if(write_value)
    {
      src->value[str_num] = value;
      if(src->length <= (unsigned int)str_num)
        src->length = str_num + 1;
    }
    else
    {
      src->value[value] = 0;
      src->length = new_length;
    }
  }
  else
  {
    struct string src_str;
    char n_buffer[16];
    snprintf(n_buffer, 16, "%d", value);

    src_str.value = n_buffer;
    src_str.length = strlen(n_buffer);
    set_string(mzx_world, name, &src_str, id);
  }
}

static int mod_order_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_order();
}

static void mod_order_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  jump_module(value);
}

static int mod_position_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_position();
}

static void mod_position_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  set_position(value);
}

static int mod_freq_read(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int id)
{
  return get_frequency();
}

static void mod_freq_write(struct world *mzx_world,
 const struct function_counter *counter, const char *name, int value, int id)
{
  if((value == 0) || ((value >= 16) && (value <= (2 << 20))))
    shift_frequency(value);
}

// Make sure this is in the right alphabetical (non-case sensitive) order
// ? specifies 0 or more numbers
// ! specifies 1 or more numbers
// * skips any characters at or after this point, including a null terminator
// This must be sorted in a case insensitive way, that has special applications
// for non-letters:
// _ comes after letters
// ? comes before letters
// numbers come before letters (and before ?)

/* FIXME: "MOD *" will still work, even in worlds < 2.51s1 */

/* NOTE: ABS_VALUE, R_PLAYERDIST, SQRT_VALUE, WRAP, VALUE were removed in
 *       2.68 and no compatibility layer was implemented.
 *       FREAD_PAGE, FWRITE_PAGE were removed in 2.80 and no compatibility
 *       layer was implemented.
 */

/* NOTE: fread_counter/fwrite_counter read only a single DOS word (16bit)
 *       in MZX versions prior to 2.82. Newer versions read in a single
 *       DOS dword (32bit). Since the counter was introduced in versions
 *       prior to 2.82, it is not versioned as such below (see the read/write
 *       functions for the exact semantics).
 */

/* NOTE: Compatibility with worlds made between 2.51s2 and 2.61 (inclusive)
 *       is only partial, due to the MZX/SAV magic not being incremented
 *       in these versions. This is unfortunately not fixable.
 */

static const struct function_counter builtin_counters[] =
{
  { "$*", 0x023E, str_num_read, str_num_write },                     // 2.62
  { "abs!", 0x0244, abs_read, NULL },                                // 2.68
  { "acos!", 0x0244, acos_read, NULL },                              // 2.68
  { "asin!", 0x0244, asin_read, NULL },                              // 2.68
  { "atan!", 0x0244, atan_read, NULL },                              // 2.68
  { "bimesg", 0x0209, NULL, bimesg_write },                          // 2.51s3.2
  { "blue_value", 0x0209, blue_value_read, blue_value_write },       // 2.60
  { "board_char", 0x0209, board_char_read, NULL },                   // 2.60
  { "board_color", 0x0209, board_color_read, NULL },                 // 2.60
  { "board_h", 0x0241, board_h_read, NULL },                         // 2.65
  { "board_id", 0x0241, board_id_read, board_id_write },             // 2.65
  { "board_param", 0x0241, board_param_read, board_param_write },    // 2.65
  { "board_w", 0x0241, board_w_read, NULL },                         // 2.65
  { "bullettype", 0, bullettype_read, bullettype_write },            // <=2.51
  { "buttons", 0x0208, buttons_read, NULL },                         // 2.51s1
  { "char_byte", 0x0209, char_byte_read, char_byte_write },          // 2.60
  { "commands", 0x0209, commands_read, commands_write },             // 2.60
  { "cos!", 0x0244, cos_read, NULL },                                // 2.68
  { "c_divisions", 0x0244, c_divisions_read, c_divisions_write },    // 2.68
  { "date_day", 0x0209, date_day_read, NULL },                       // 2.60
  { "date_month", 0x0209, date_month_read, NULL },                   // 2.60
  { "date_year", 0x0209, date_year_read, NULL },                     // 2.60
  { "divider", 0x0244, divider_read, divider_write },                // 2.68
  { "fread", 0x0209, fread_read, NULL },                             // 2.60
  { "fread_counter", 0x0241, fread_counter_read, NULL },             // 2.65
  { "fread_open", 0x0209, fread_open_read, NULL },                   // 2.60
  { "fread_pos", 0x0209, fread_pos_read, fread_pos_write },          // 2.60
  { "fwrite", 0x0209, NULL, fwrite_write },                          // 2.60
  { "fwrite_append", 0x0209, fwrite_append_read, NULL },             // 2.60
  { "fwrite_counter", 0x0241, NULL, fwrite_counter_write },          // 2.65
  { "fwrite_modify", 0x0248, fwrite_modify_read, NULL },             // 2.69c
  { "fwrite_open", 0x0209, fwrite_open_read, NULL },                 // 2.60
  { "fwrite_pos", 0x0209, fwrite_pos_read, fwrite_pos_write },       // 2.60
  { "green_value", 0x0209, green_value_read, green_value_write },    // 2.60
  { "horizpld", 0, horizpld_read, NULL },                            // <=2.51
  { "input", 0, input_read, input_write },                           // <=2.51
  { "inputsize", 0, inputsize_read, inputsize_write },               // <=2.51
  { "int2bin", 0x0209, int2bin_read, int2bin_write },                // 2.60
  { "key", 0, key_read, key_write },                                 // <=2.51
  { "key?", 0x0245, keyn_read, NULL },                               // 2.69
  { "key_code", 0x0245, key_code_read, NULL },                       // 2.69
  { "key_pressed", 0x0209, key_pressed_read, NULL },                 // 2.60
  { "key_release", 0x0245, key_release_read, NULL },                 // 2.69
  { "lava_walk", 0x0209, lava_walk_read, lava_walk_write },          // 2.60
  { "load_bc?", 0x0249, load_bc_read, NULL },                        // 2.70
  { "load_game", 0x0244, load_game_read, NULL },                     // 2.68
  { "load_robot?", 0x0249, load_robot_read, NULL },                  // 2.70
  { "local?", 0x0208, local_read, local_write },                     // 2.51s1
  { "loopcount", 0, loopcount_read, loopcount_write },               // <=2.51
  { "mboardx", 0x0208, mboardx_read, NULL },                         // 2.51s1
  { "mboardy", 0x0208, mboardy_read, NULL },                         // 2.51s1
  { "mod_frequency", 0x0251, mod_freq_read, mod_freq_write },        // 2.81
  { "mod_order", 0x023E, mod_order_read, mod_order_write },          // 2.62
  { "mod_position", 0x0251, mod_position_read, mod_position_write }, // 2.81
  { "mousepx", 0x0252, mousepx_read, mousepx_write },                // 2.82
  { "mousepy", 0x0252, mousepy_read, mousepy_write },                // 2.82
  { "mousex", 0x0208, mousex_read, mousex_write },                   // 2.51s1
  { "mousey", 0x0208, mousey_read, mousey_write },                   // 2.51s1
  { "multiplier", 0x0244, multiplier_read, multiplier_write },       // 2.68
  { "mzx_speed", 0x0209, mzx_speed_read, mzx_speed_write },          // 2.60
  { "overlay_char", 0x0209, overlay_char_read, NULL },               // 2.60
  { "overlay_color", 0x0209, overlay_color_read, NULL },             // 2.60
  { "overlay_mode", 0x0209, overlay_mode_read, NULL },               // 2.60
  { "pixel", 0x0209, pixel_read, pixel_write },                      // 2.60
  { "playerdist", 0, playerdist_read, NULL },                        // <=2.51
  { "playerfacedir", 0, playerfacedir_read, playerfacedir_write },   // <=2.51
  { "playerlastdir", 0, playerlastdir_read, playerlastdir_write },   // <=2.51
  { "playerx", 0x0208, playerx_read, NULL },                         // 2.51s1
  { "playery", 0x0208, playery_read, NULL },                         // 2.51s1
  { "r!.*", 0x0241, r_read, r_write },                               // 2.65
  { "red_value", 0x0209, red_value_read, red_value_write },          // 2.60
  { "rid*", 0x0246, rid_read, NULL },                                // 2.69b
  { "robot_id", 0x0209, robot_id_read, NULL },                       // 2.60
  { "robot_id_*", 0x0241, robot_id_n_read, NULL },                   // 2.65
#ifndef CONFIG_DEBYTECODE
  { "save_bc?", 0x0249, save_bc_read, NULL },                        // 2.70
#endif
  { "save_game", 0x0244, save_game_read, NULL },                     // 2.68
#ifndef CONFIG_DEBYTECODE
  { "save_robot?", 0x0249, save_robot_read, NULL },                  // 2.70
#endif
  { "save_world", 0x0248, save_world_read, NULL },                   // 2.69c
  { "scrolledx", 0x0208, scrolledx_read, NULL },                     // 2.51s1
  { "scrolledy", 0x0208, scrolledy_read, NULL },                     // 2.51s1
  { "sin!", 0x0244, sin_read, NULL },                                // 2.68
  { "smzx_b!", 0x0245, smzx_b_read, smzx_b_write },                  // 2.69
  { "smzx_g!", 0x0245, smzx_g_read, smzx_g_write },                  // 2.69
  { "smzx_mode", 0x0245, smzx_mode_read, smzx_mode_write },          // 2.69
  { "smzx_palette", 0x0245, smzx_palette_read, NULL },               // 2.69
  { "smzx_r!", 0x0245, smzx_r_read, smzx_r_write },                  // 2.69
  { "spr!_ccheck", 0x0241, NULL, spr_ccheck_write },                 // 2.65
  { "spr!_cheight", 0x0241, spr_cheight_read, spr_cheight_write },   // 2.65
  { "spr!_clist", 0x0241, NULL, spr_clist_write },                   // 2.65
  { "spr!_cwidth", 0x0241, spr_cwidth_read, spr_cwidth_write },      // 2.65
  { "spr!_cx", 0x0241, spr_cx_read, spr_cx_write },                  // 2.65
  { "spr!_cy", 0x0241, spr_cy_read, spr_cy_write },                  // 2.65
  { "spr!_height", 0x0241, spr_height_read, spr_height_write },      // 2.65
  { "spr!_off", 0x0241, NULL, spr_off_write },                       // 2.65
  { "spr!_overlaid", 0x0241, NULL, spr_overlaid_write },             // 2.65
  { "spr!_overlay", 0x0248, NULL, spr_overlaid_write },              // 2.69c
  { "spr!_refx", 0x0241, spr_refx_read, spr_refx_write },            // 2.65
  { "spr!_refy", 0x0241, spr_refy_read, spr_refy_write },            // 2.65
  { "spr!_setview", 0x0241, NULL, spr_setview_write },               // 2.65
  { "spr!_static", 0x0241, NULL, spr_static_write },                 // 2.65
  { "spr!_swap", 0x0241, NULL, spr_swap_write },                     // 2.65
  { "spr!_vlayer", 0x0248, NULL, spr_vlayer_write },                 // 2.69c
  { "spr!_width", 0x0241, spr_width_read, spr_width_write },         // 2.65
  { "spr!_x", 0x0241, spr_x_read, spr_x_write },                     // 2.65
  { "spr!_y", 0x0241, spr_y_read, spr_y_write },                     // 2.65
  { "spr_clist!", 0x0241, spr_clist_read, NULL },                    // 2.65
  { "spr_collisions", 0x0241, spr_collisions_read, NULL },           // 2.65
  { "spr_num", 0x0241, spr_num_read, spr_num_write },                // 2.65
  { "spr_yorder", 0x0241, NULL, spr_yorder_write },                  // 2.65
  { "sqrt!", 0x0244, sqrt_read, NULL },                              // 2.68
  { "tan!", 0x0244, tan_read, NULL },                                // 2.68
  { "thisx", 0, thisx_read, NULL },                                  // <=2.51
  { "thisy", 0, thisy_read, NULL },                                  // <=2.51
  { "this_char", 0x0209, this_char_read, NULL },                     // 2.60
  { "this_color", 0x0209, this_color_read, NULL },                   // 2.60
  { "timereset", 0, timereset_read, timereset_write },               // <=2.51
  { "time_hours", 0x0209, time_hours_read, NULL },                   // 2.60
  { "time_minutes", 0x0209, time_minutes_read, NULL },               // 2.60
  { "time_seconds", 0x0209, time_seconds_read, NULL },               // 2.60
  { "vch!,!", 0x0248, vch_read, vch_write },                         // 2.69c
  { "vco!,!", 0x0248, vco_read, vco_write },                         // 2.69c
  { "vertpld", 0, vertpld_read, NULL },                              // <=2.51
  { "vlayer_height", 0x0248, vlayer_height_read, vlayer_height_write },// 2.69c
  { "vlayer_size", 0x0251, vlayer_size_read, vlayer_size_write },    // 2.81
  { "vlayer_width", 0x0248, vlayer_width_read, vlayer_width_write }, // 2.69c
};

static int counter_first_letter[512];

static struct robot *get_robot_by_id(struct world *mzx_world, int id)
{
  if(id >= 0 && id <= mzx_world->current_board->num_robots)
    return mzx_world->current_board->robot_list[id];
  else
    return NULL;
}

int set_counter_special(struct world *mzx_world, char *char_value,
 int value, int id)
{
  struct robot *cur_robot = get_robot_by_id(mzx_world, id);

  switch(mzx_world->special_counter_return)
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
      break;
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
      break;
    }

    case FOPEN_FAPPEND:
    {
      if(char_value[0])
      {
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
      break;
    }

    case FOPEN_FMODIFY:
    {
      if(char_value[0])
      {
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
      break;
    }

    case FOPEN_SMZX_PALETTE:
    {
      load_palette(char_value);
      pal_update = true;
      break;
    }

    case FOPEN_SAVE_GAME:
    {
      char *translated_path = cmalloc(MAX_PATH);
      int err;

      if(cur_robot)
      {
        // Advance the program so that loading a SAV doesn't re-run this line
        cur_robot->cur_prog_line +=
         cur_robot->program_bytecode[cur_robot->cur_prog_line] + 2;
      }

      err = fsafetranslate(char_value, translated_path);
      if(err == -FSAFE_SUCCESS || err == -FSAFE_MATCH_FAILED)
        save_world(mzx_world, translated_path, 1);

      free(translated_path);
      break;
    }

    case FOPEN_SAVE_WORLD:
    {
      char *translated_path = cmalloc(MAX_PATH);
      int err;

      if(cur_robot)
      {
        // Advance the program so that loading a SAV doesn't re-run this line
        cur_robot->cur_prog_line +=
         cur_robot->program_bytecode[cur_robot->cur_prog_line] + 2;
      }

      err = fsafetranslate(char_value, translated_path);
      if(err == -FSAFE_SUCCESS || err == -FSAFE_MATCH_FAILED)
        save_world(mzx_world, translated_path, 0);

      free(translated_path);
      break;
    }

    case FOPEN_LOAD_GAME:
    {
      char *translated_path = cmalloc(MAX_PATH);
      int faded;

      if(!fsafetranslate(char_value, translated_path))
      {
        if(reload_savegame(mzx_world, translated_path, &faded))
        {
          if(faded)
            insta_fadeout();
          else
            insta_fadein();
          mzx_world->swapped = 1;
        }
      }

      free(translated_path);
      break;
    }

#ifdef CONFIG_DEBYTECODE

    // TODO: Implement new one: right now load_robot loads legacy bots.
    case FOPEN_LOAD_ROBOT:
    {
      if(value >= 0)
        cur_robot = get_robot_by_id(mzx_world, value);

      if(cur_robot)
      {
        cur_robot->program_source = legacy_convert_file(char_value,
         &(cur_robot->program_source_length),
         mzx_world->conf.disassemble_extras,
         mzx_world->conf.disassemble_base);

        // TODO: Move this outside of here.
        free(cur_robot->program_bytecode);
        cur_robot->program_bytecode = NULL;
        cur_robot->stack_pointer = 0;
        cur_robot->cur_prog_line = 1;

        prepare_robot_bytecode(cur_robot);

        // Restart this robot if either it was just a LOAD_ROBOT
        // OR LOAD_ROBOTn was used where n is &robot_id&.
        if(value == -1 || value == id)
          return 1;
      }
      break;
    }

    case FOPEN_LOAD_BC:
    {
      // Although this pseudo-command is still usable, it'll no longer be
      // possible to actually export bytecode. So this means that the bytecode
      // is always from a version that legacy_disassemble_program

      // It's basically like LOAD_ROBOT except that it first has to disassmble
      // the bytecode.

      FILE *bc_file = fsafeopen(char_value, "rb");

      if(bc_file)
      {
        if(value >= 0)
          cur_robot = get_robot_by_id(mzx_world, value);

        if(cur_robot)
        {
          int program_bytecode_length = ftell_and_rewind(bc_file);
          char *program_legacy_bytecode = malloc(program_bytecode_length + 1);

          fread(program_legacy_bytecode, program_bytecode_length, 1,
           bc_file);

          cur_robot->program_source =
           legacy_disassemble_program(program_legacy_bytecode,
           program_bytecode_length, &(cur_robot->program_source_length),
           mzx_world->conf.disassemble_extras,
           mzx_world->conf.disassemble_base);
          free(program_legacy_bytecode);

          // TODO: Move this outside of here.
          if(cur_robot->program_bytecode)
          {
            free(cur_robot->program_bytecode);
            cur_robot->program_bytecode = NULL;
          }

          cur_robot->cur_prog_line = 1;
          cur_robot->stack_pointer = 0;
          prepare_robot_bytecode(cur_robot);

          // Restart this robot if either it was just a LOAD_BC
          // OR LOAD_BCn was used where n is &robot_id&.
          if(value == -1 || value == id)
          {
            fclose(bc_file);
            return 1;
          }
        }

        fclose(bc_file);
      }
      break;
    }

    // TODO: show some kind of error - we can't support this. That's
    // because there probably won't be source code present to save.
    // If any games use it then they just can't be supported. There just
    // isn't an easy fix for this.
    /* case FOPEN_SAVE_ROBOT: */

    // TODO: This can't really exist anymore... do something here to send
    // out an error. In practice I doubt any games use it, but they can't
    // be supported - there's no really easy fix for this.
    /* case FOPEN_SAVE_BC: */

#else // !CONFIG_DEBYTECODE

    case FOPEN_LOAD_ROBOT:
    {
      char *new_program;
      int new_size;

      new_program = assemble_file(char_value, &new_size);
      if(new_program)
      {
        if(value >= 0)
          cur_robot = get_robot_by_id(mzx_world, value);

        if(cur_robot)
        {
          reallocate_robot(cur_robot, new_size);
          clear_label_cache(cur_robot->label_list, cur_robot->num_labels);

          memcpy(cur_robot->program_bytecode, new_program, new_size);
          cur_robot->stack_pointer = 0;
          cur_robot->cur_prog_line = 1;
          cur_robot->label_list =
           cache_robot_labels(cur_robot, &(cur_robot->num_labels));

          // Restart this robot if either it was just a LOAD_ROBOT
          // OR LOAD_ROBOTn was used where n is &robot_id&.
          if(value == -1 || value == id)
          {
            free(new_program);
            return 1;
          }
        }

        free(new_program);
      }
      break;
    }

    case FOPEN_LOAD_BC:
    {
      FILE *bc_file = fsafeopen(char_value, "rb");
      int new_size;

      if(bc_file)
      {
        if(value >= 0)
          cur_robot = get_robot_by_id(mzx_world, value);

        if(cur_robot)
        {
          new_size = ftell_and_rewind(bc_file);

          reallocate_robot(cur_robot, new_size);
          clear_label_cache(cur_robot->label_list, cur_robot->num_labels);

          fread(cur_robot->program_bytecode, new_size, 1, bc_file);
          cur_robot->cur_prog_line = 1;
          cur_robot->stack_pointer = 0;
          cur_robot->label_list =
            cache_robot_labels(cur_robot, &(cur_robot->num_labels));

          // Restart this robot if either it was just a LOAD_BC
          // OR LOAD_BCn was used where n is &robot_id&.
          if(value == -1 || value == id)
          {
            fclose(bc_file);
            return 1;
          }
        }

        fclose(bc_file);
      }
      break;
    }

    case FOPEN_SAVE_ROBOT:
    {
      int allow_extras = mzx_world->conf.disassemble_extras;
      int base = mzx_world->conf.disassemble_base;

      if(value >= 0)
        cur_robot = get_robot_by_id(mzx_world, value);

      if(cur_robot)
      {
        disassemble_file(char_value, cur_robot->program_bytecode,
         cur_robot->program_bytecode_length, allow_extras, base);
      }
      break;
    }

    case FOPEN_SAVE_BC:
    {
      FILE *bc_file = fsafeopen(char_value, "wb");

      if(bc_file)
      {
        if(value >= 0)
          cur_robot = get_robot_by_id(mzx_world, value);

        if(cur_robot)
        {
          fwrite(cur_robot->program_bytecode,
           cur_robot->program_bytecode_length, 1, bc_file);
        }

        fclose(bc_file);
      }
      break;
    }

#endif // !CONFIG_DEBYTECODE

    default:
      debug("Invalid special_counter_return!\n");
      break;
  }

  return 0;
}

// I don't know yet if this works in pure C
static const int num_builtin_counters =
 sizeof(builtin_counters) / sizeof(struct function_counter);

static int hurt_player(struct world *mzx_world, int value)
{
  // Must not be invincible
  if(get_counter(mzx_world, "INVINCO", 0) <= 0)
  {
    struct board *src_board = mzx_world->current_board;
    send_robot_def(mzx_world, 0, 13);
    if(src_board->restart_if_zapped)
    {
      int player_restart_x = mzx_world->player_restart_x;
      int player_restart_y = mzx_world->player_restart_y;
      int player_x = mzx_world->player_x;
      int player_y = mzx_world->player_y;
     //Restart since we were hurt
      if((player_restart_x != player_x) ||
       (player_restart_y != player_y))
      {
        id_remove_top(mzx_world, player_x, player_y);
        player_x = player_restart_x;
        player_y = player_restart_y;
        id_place(mzx_world, player_x, player_y, PLAYER, 0, 0);
        mzx_world->was_zapped = 1;
        mzx_world->player_x = player_x;
        mzx_world->player_y = player_y;
      }
    }
  }
  else
  {
    value = 0;
  }
  return value;
}

static int health_dec_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id)
{
  // Trying to decrease health? (legacy support)
  // Only handle here if MZX world version <= 2.70
  // Otherwise, it will be handled in health_gateway()
  if((value > 0) && (mzx_world->version <= 0x0249))
  {
    value = hurt_player(mzx_world, value);
  }
  return value;
}

static int health_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id)
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
  // Only handle here if MZX world version > 2.70
  // Otherwise, it will be handled in health_dec_gateway()
  if((value < counter->value) && (mzx_world->version > 0x0249))
  {
    value = counter->value - hurt_player(mzx_world, counter->value - value);
  }

  return value;
}

static int lives_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id)
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

static int invinco_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id)
{
  if(!counter->value)
  {
    mzx_world->saved_pl_color = id_chars[player_color];
  }
  else
  {
    if(!value)
    {
      clear_sfx_queue();
      play_sfx(mzx_world, 18);
      id_chars[player_color] = mzx_world->saved_pl_color;
    }
  }

  return value;
}

static int score_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id)
{
  // Protection for score < 0, as per the behavior in DOS MZX.
  if((value < 0) && (mzx_world->version <= 0x249))
    return 0;

  return value;
}

static int time_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id)
{
  return CLAMP(value, 0, 32767);
}

// The other builtins simply can't go below 0

static int builtin_gateway(struct world *mzx_world, struct counter *counter,
 const char *name, int value, int id)
{
  return MAX(value, 0);
}

static struct counter *find_counter(struct world *mzx_world, const char *name,
 int *next)
{
  int bottom = 0, top = (mzx_world->num_counters) - 1, middle = 0;
  int cmpval = 0;
  struct counter **base = mzx_world->counter_list;
  struct counter *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

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

// Setup builtin gateway functions
static void set_gateway(struct world *mzx_world, const char *name,
 gateway_write_function function)
{
  int next;
  struct counter *cdest = find_counter(mzx_world, name, &next);
  if(cdest)
    cdest->gateway_write = function;
}

static void set_dec_gateway(struct world *mzx_world, const char *name,
 gateway_dec_function function)
{
  int next;
  struct counter *cdest = find_counter(mzx_world, name, &next);
  if(cdest)
    cdest->gateway_dec = function;
}

void initialize_gateway_functions(struct world *mzx_world)
{
  set_gateway(mzx_world, "AMMO", builtin_gateway);
  set_gateway(mzx_world, "COINS", builtin_gateway);
  set_gateway(mzx_world, "GEMS", builtin_gateway);
  set_gateway(mzx_world, "HEALTH", health_gateway);
  set_dec_gateway(mzx_world, "HEALTH", health_dec_gateway);
  set_gateway(mzx_world, "HIBOMBS", builtin_gateway);
  set_gateway(mzx_world, "INVINCO", invinco_gateway);
  set_gateway(mzx_world, "LIVES", lives_gateway);
  set_gateway(mzx_world, "LOBOMBS", builtin_gateway);
  set_gateway(mzx_world, "SCORE", score_gateway);
  set_gateway(mzx_world, "TIME", time_gateway);
}

int match_function_counter(const char *dest, const char *src)
{
  int difference;
  char cur_src, cur_dest;

  while(1)
  {
    cur_src = *src;
    cur_dest = *dest;

    // Skip 1 or more letters
    switch(cur_src)
    {
      // Make sure the first character is a number
      case '!':
      {
        if(((cur_dest < '0') || (cur_dest > '9')) &&
         (cur_dest != '-'))
        {
          return 1;
        }

        dest++;
        cur_dest = *dest;

        // Fall through, skip remaining numbers
      }

      // Skip 0 or more number characters
      case '?':
      {
        src++;
        cur_src = *src;

        while(((cur_dest >= '0') && (cur_dest <= '9')) ||
         (cur_dest == '-'))
        {
          dest++;
          cur_dest = *dest;
        }
        break;
      }

      // Match anything, instant winner
      case '*':
      {
        return 0;
      }
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

static const struct function_counter *find_function_counter(const char *name)
{
  const struct function_counter *base = builtin_counters;
  int first_letter = tolower((int)name[0]) * 2;
  int bottom, top, middle;
  int cmpval;

  bottom = counter_first_letter[first_letter];
  top = counter_first_letter[first_letter + 1];

  if(bottom != -1)
  {
    while(bottom <= top)
    {
      middle = (top + bottom) / 2;
      cmpval = match_function_counter(name + 1, (base[middle]).name + 1);

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

static void add_counter(struct world *mzx_world, const char *name,
 int value, int position)
{
  int count = mzx_world->num_counters;
  int allocated = mzx_world->num_counters_allocated;
  struct counter **base = mzx_world->counter_list;
  struct counter *cdest;

  // Need a reallocation?
  if(count == allocated)
  {
    if(allocated)
      allocated *= 2;
    else
      allocated = MIN_COUNTER_ALLOCATE;

    mzx_world->counter_list =
     crealloc(base, sizeof(struct counter *) * allocated);
    base = mzx_world->counter_list;
    mzx_world->num_counters_allocated = allocated;
  }

  // Doesn't exist, so create it
  // First make space for it if it's not at the top
  if(position != count)
  {
    base += position;
    memmove((char *)(base + 1), (char *)base,
     (count - position) * sizeof(struct counter *));
  }

  cdest = cmalloc(sizeof(struct counter) + strlen(name));
  strcpy(cdest->name, name);
  cdest->value = value;
  cdest->gateway_write = NULL;
  cdest->gateway_dec = NULL;

  mzx_world->counter_list[position] = cdest;
  mzx_world->num_counters = count + 1;
}

static void add_string(struct world *mzx_world, const char *name,
 struct string *src, int position)
{
  struct string *dest = add_string_preallocate(mzx_world, name,
   src->length, position);

  memcpy(dest->value, src->value, src->length);
}

void set_counter(struct world *mzx_world, const char *name, int value, int id)
{
  const struct function_counter *fdest;
  struct counter *cdest;
  int next = 0;

  fdest = find_function_counter(name);

  if(fdest && (mzx_world->version >= fdest->minimum_version))
  {
    // If we're a function counter and we have a write method,
    // use it. However, if we don't have a write method, this is
    // intentionally read-only counter and no storage space should
    // be allocated for it. Fall through without error in this case.
    if(fdest->function_write)
      fdest->function_write(mzx_world, fdest, name, value, id);
    else
      assert(fdest->function_read);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      // See if there's a gateway
      if(cdest->gateway_write)
      {
        value = cdest->gateway_write(mzx_world, cdest, name, value, id);
      }

      cdest->value = value;
    }
    else
    {
      add_counter(mzx_world, name, value, next);
    }
  }
}

static void get_string_size_offset(char *name, size_t *ssize,
 bool *size_specified, size_t *soffset, bool *offset_specified)
{
  int offset_position = -1, size_position = -1;
  char current_char = name[0];
  int current_position = 0;
  unsigned int ret;
  char *error;

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
    name[size_position] = 0;

  if(offset_position != -1)
    name[offset_position] = 0;

  if(size_position != -1)
  {
    ret = strtoul(name + size_position + 1, &error, 10);
    if(!error[0])
    {
      *size_specified = true;
      *ssize = ret;
    }
  }

  if(offset_position != -1)
  {
    ret = strtoul(name + offset_position + 1, &error, 10);
    if(!error[0])
    {
      *offset_specified = true;
      *soffset = ret;
    }
  }
}

static int load_string_board_direct(struct world *mzx_world,
 struct string *str, int next, int w, int h, char l, char *src, int width)
{
  int i;
  int size = w * h;
  char *str_value;
  char *t_pos;

  str_value = str->value;
  t_pos = str_value;

  for(i = 0; i < h; i++)
  {
    memcpy(t_pos, src, w);
    t_pos += w;
    src += width;
  }

  if(l)
  {
    for(i = 0; i < size; i++)
    {
      if(str_value[i] == l)
        break;
    }

    str->length = i;
    return i;
  }
  else
  {
    str->length = size;
    return size;
  }
}

void set_string(struct world *mzx_world, const char *name, struct string *src,
 int id)
{
  bool offset_specified = false, size_specified = false;
  size_t src_length = src->length;
  char *src_value = src->value;
  size_t size = 0, offset = 0;
  struct string *dest;
  int next = 0;

  // this generates an unfixable warning at -O3
  get_string_size_offset((char *)name, &size, &size_specified,
   &offset, &offset_specified);

  dest = find_string(mzx_world, name, &next);

  // TODO - Make terminating chars variable, but how..
  if(special_name_partial("fread") && mzx_world->input_file)
  {
    FILE *input_file = mzx_world->input_file;

    if(src_length > 5)
    {
      unsigned int read_count = strtoul(src_value + 5, NULL, 10);
      long current_pos, file_size;
      size_t actual_read;

      /* This is hacky, but we don't want to prematurely allocate more space
       * to the string than can possibly be read from the file. So we save the
       * old file pointer, figure out the length of the current input file,
       * and put it back where we found it.
       */
      current_pos = ftell(input_file);
      file_size = ftell_and_rewind(input_file);
      fseek(input_file, current_pos, SEEK_SET);

      /* We then truncate the user read to the maximum difference between the
       * current position and the file end; this won't affect normal reads,
       * it just prevents people from crashing MZX by reading 2G from a 3K file.
       */
      if(current_pos + read_count > (unsigned int)file_size)
        read_count = file_size - current_pos;

      force_string_splice(mzx_world, name, next, &dest,
       read_count, offset, offset_specified, &size, size_specified);

      actual_read = fread(dest->value + offset, 1, read_count, input_file);
      if(offset == 0 && !offset_specified)
        dest->length = actual_read;
    }
    else
    {
      const char terminate_char = '*';
      char *dest_value;
      int current_char = 0;
      unsigned int read_pos = 0;
      unsigned int read_allocate;
      unsigned int allocated = 32;
      unsigned int new_allocated = allocated;

      force_string_splice(mzx_world, name, next, &dest,
       allocated, offset, offset_specified, &size, size_specified);
      dest_value = dest->value;

      while(1)
      {
        for(read_allocate = 0; read_allocate < new_allocated;
         read_allocate++, read_pos++)
        {
          current_char = fgetc(input_file);

          if((current_char == terminate_char) || (current_char == EOF))
          {
            if(offset == 0 && !offset_specified)
              dest->length = read_pos;
            return;
          }

          dest_value[read_pos + offset] = current_char;
        }

        new_allocated = allocated;
        allocated *= 2;

        dest = reallocate_string(mzx_world, dest, next, allocated);
        dest_value = dest->value;
      }
    }
  }
  else

  if(special_name("board_name"))
  {
    char *board_name = mzx_world->current_board->board_name;
    size_t str_length = strlen(board_name);
    force_string_copy(mzx_world, name, next, &dest, str_length,
     offset, offset_specified, &size, size_specified, board_name);
  }
  else

  if(special_name("robot_name"))
  {
    char *robot_name = (mzx_world->current_board->robot_list[id])->robot_name;
    size_t str_length = strlen(robot_name);
    force_string_copy(mzx_world, name, next, &dest, str_length,
     offset, offset_specified, &size, size_specified, robot_name);
  }
  else

  if(special_name("mod_name"))
  {
    char *mod_name = mzx_world->real_mod_playing;
    size_t str_length = strlen(mod_name);
    force_string_copy(mzx_world, name, next, &dest, str_length,
     offset, offset_specified, &size, size_specified, mod_name);
  }
  else

  if(special_name("input"))
  {
    char *input_string = mzx_world->current_board->input_string;
    size_t str_length = strlen(input_string);
    force_string_copy(mzx_world, name, next, &dest, str_length,
     offset, offset_specified, &size, size_specified, input_string);
  }
  else

  // Okay, I implemented this stupid thing, you can all die.
  if(special_name("board_scan"))
  {
    struct board *src_board = mzx_world->current_board;
    unsigned int board_width, board_size, board_pos;
    size_t read_length = 63;

    board_width = src_board->board_width;
    board_size = board_width * src_board->board_height;
    board_pos = get_board_x_board_y_offset(mzx_world, id);

    force_string_length(mzx_world, name, next, &dest, &read_length);

    if(board_pos < board_size)
    {
      if((board_pos + read_length) > board_size)
        read_length = board_size - board_pos;

      load_string_board_direct(mzx_world, dest, next,
       (int)read_length, 1, '*', src_board->level_param + board_pos,
       board_width);
    }
  }
  else

  // This isn't a read (set), it's a write but it fits here now.

  if(special_name_partial("fwrite") && mzx_world->output_file)
  {
    /* You can't write a string that doesn't exist, or a string
     * of zero length (the file will still be created, of course).
     */
    if(dest != NULL && dest->length > 0)
    {
      FILE *output_file = mzx_world->output_file;
      char *dest_value = dest->value;
      size_t dest_length = dest->length;

      if(src_length > 6)
        size = strtol(src_value + 6, NULL, 10);

      if(size == 0)
        size = dest_length;

      if(offset >= dest_length)
        offset = dest_length - 1;

      if(offset + size > dest_length)
        size = src_length - offset;

      fwrite(dest_value + offset, size, 1, output_file);

      if(src_length == 6)
        fputc('*', output_file);
    }
  }
  else
  {
    // Just a normal string here.
    force_string_move(mzx_world, name, next, &dest, src_length,
     offset, offset_specified, &size, size_specified, src_value);
  }
}

int get_counter(struct world *mzx_world, const char *name, int id)
{
  const struct function_counter *fdest;
  struct counter *cdest;
  int next;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read &&
   (mzx_world->version >= fdest->minimum_version))
  {
    // Call read function
    if(fdest->function_read)
      return fdest->function_read(mzx_world, fdest, name, id);
    else
      return 0;
  }

  cdest = find_counter(mzx_world, name, &next);

  if(cdest)
    return cdest->value;

  return 0;
}

int get_string(struct world *mzx_world, const char *name, struct string *dest,
 int id)
{
  bool offset_specified = false, size_specified = false;
  size_t size = 0, offset = 0;
  struct string *src;
  char *trimmed_name;
  int next;

  trimmed_name = malloc(strlen(name) + 1);
  memcpy(trimmed_name, name, strlen(name) + 1);

  get_string_size_offset(trimmed_name, &size, &size_specified,
   &offset, &offset_specified);

  src = find_string(mzx_world, trimmed_name, &next);
  free(trimmed_name);

  if(src)
  {
    if((size == 0 && !size_specified) || size > src->length)
      size = src->length;

    if(offset > src->length)
      offset = src->length;

    if(offset + size > src->length)
      size = src->length - offset;

    dest->value = src->value + offset;
    dest->length = size;
    return 1;
  }

  dest->length = 0;
  return 0;
}

void inc_counter(struct world *mzx_world, const char *name, int value, int id)
{
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next = 0;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);
    fdest->function_write(mzx_world, fdest, name,
     current_value + value, id);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      value += cdest->value;

      if(cdest->gateway_write)
      {
        value = cdest->gateway_write(mzx_world, cdest, name, value, id);
      }

      cdest->value = value;
    }
    else
    {
      add_counter(mzx_world, name, value, next);
    }
  }
}

// You can't increment spliced strings (it's not really useful and
// would introduce a world of problems..)

void inc_string(struct world *mzx_world, const char *name, struct string *src,
 int id)
{
  struct string *dest;
  int next;

  dest = find_string(mzx_world, name, &next);

  if(dest)
  {
    size_t new_length = src->length + dest->length;
    // Concatenate
    if(new_length > dest->allocated_length)
    {
      // Handle collisions, for incrementing something by a splice
      // of itself, which could relocate the string and the value...
      char *src_end = src->value + src->length;
      if((src_end <= (dest->value + dest->length)) &&
       (src_end >= dest->value))
      {
        char *old_dest_value = dest->value;
        dest = reallocate_string(mzx_world, dest, next, new_length);
        src->value += (int)(dest->value - old_dest_value);
      }
      else
      {
        dest = reallocate_string(mzx_world, dest, next, new_length);
      }
    }

    memcpy(dest->value + dest->length, src->value, src->length);
    dest->length = new_length;
  }
  else
  {
    add_string(mzx_world, name, src, next);
  }
}

void dec_counter(struct world *mzx_world, const char *name, int value, int id)
{
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next = 0;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);
    fdest->function_write(mzx_world, fdest, name,
     current_value - value, id);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      if(cdest->gateway_dec)
      {
        gateway_dec_function gateway_dec =
         (gateway_dec_function)cdest->gateway_dec;
        value = gateway_dec(mzx_world, cdest, name, value, id);
      }

      value = cdest->value - value;

      if(cdest->gateway_write)
      {
        value = cdest->gateway_write(mzx_world, cdest, name, value, id);
      }

      cdest->value = value;
    }
    else
    {
      add_counter(mzx_world, name, -value, next);
    }
  }
}

void dec_string_int(struct world *mzx_world, const char *name, int value,
 int id)
{
  struct string *dest;
  int next;

  dest = find_string(mzx_world, name, &next);

  if(dest)
  {
    // Simply decrease the length
    if((int)dest->length - value < 0)
      dest->length = 0;
    else
      dest->length -= value;
  }
}

void mul_counter(struct world *mzx_world, const char *name, int value, int id)
{
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);
    fdest->function_write(mzx_world, fdest, name,
     current_value * value, id);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      value *= cdest->value;
      if(cdest->gateway_write)
      {
        value = cdest->gateway_write(mzx_world, cdest, name, value, id);
      }

      cdest->value = value;
    }
  }
}

void div_counter(struct world *mzx_world, const char *name, int value, int id)
{
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next;

  if(value == 0)
    return;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);
    fdest->function_write(mzx_world, fdest, name,
     current_value / value, id);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
    {
      value = cdest->value / value;

      if(cdest->gateway_write)
      {
        value = cdest->gateway_write(mzx_world, cdest, name, value, id);
      }

      cdest->value = value;
    }
  }
}

void mod_counter(struct world *mzx_world, const char *name, int value, int id)
{
  const struct function_counter *fdest;
  struct counter *cdest;
  int current_value;
  int next;

  if(value == 0)
    return;

  fdest = find_function_counter(name);

  if(fdest && fdest->function_read && fdest->function_write &&
   (mzx_world->version >= fdest->minimum_version))
  {
    current_value =
     fdest->function_read(mzx_world, fdest, name, id);

    fdest->function_write(mzx_world, fdest, name,
     current_value % value, id);
  }
  else
  {
    cdest = find_counter(mzx_world, name, &next);

    if(cdest)
      cdest->value %= value;
  }
}

int compare_strings(struct string *dest, struct string *src)
{
  char *src_value;
  char *dest_value;
  Uint32 *src_value_32b;
  Uint32 *dest_value_32b;
  Uint32 val_src_32b, val_dest_32b;
  Uint32 difference;
  char val_src, val_dest;
  size_t cmp_length = dest->length;
  size_t length_32b, i;

  if(src->length < cmp_length)
    return 1;

  if(src->length > cmp_length)
    return -1;

  // Compare 32bits at a time, should be mostly portable now
  length_32b = cmp_length / 4;

  src_value = src->value;
  dest_value = dest->value;

  src_value_32b = (Uint32 *)src_value;
  dest_value_32b = (Uint32 *)dest_value;

  for(i = 0; i < length_32b; i++)
  {
    val_src_32b = src_value_32b[i];
    val_dest_32b = dest_value_32b[i];

    if(val_src_32b != val_dest_32b)
    {
      difference =
       toupper(val_dest_32b >> 24) - toupper(val_src_32b >> 24);

      if(difference)
        return difference;

      difference = toupper((val_dest_32b >> 16) & 0xFF) -
       toupper((val_src_32b >> 16) & 0xFF);

      if(difference)
        return difference;

      difference = toupper((val_dest_32b >> 8) & 0xFF) -
       toupper((val_src_32b >> 8) & 0xFF);

      if(difference)
        return difference;

      difference = toupper(val_dest_32b & 0xFF) -
       toupper(val_src_32b & 0xFF);

      if(difference)
        return difference;
    }
  }

  for(i = length_32b * 4; i < cmp_length; i++)
  {
    val_src = src_value[i];
    val_dest = dest_value[i];

    if(val_src != val_dest)
    {
      difference = toupper((int)val_dest) - toupper((int)val_src);

      if(difference)
        return difference;
    }
  }

  return 0;
}

void load_string_board(struct world *mzx_world, const char *name, int w, int h,
 char l, char *src, int width)
{
  size_t length = (size_t)(w * h);
  struct string *src_str;
  int next;

  src_str = find_string(mzx_world, name, &next);
  force_string_length(mzx_world, name, next, &src_str, &length);

  src_str->length = load_string_board_direct(mzx_world, src_str, next,
   w, h, l, src, width);
}

int is_string(char *buffer)
{
  size_t namelen, i;

  // String doesn't start with $, that's an immediate reject
  if(buffer[0] != '$')
    return 0;

  // We need to stub out any part of the buffer that describes a
  // string offset or size constraint. This is because after the
  // offset or size characters, there may be an expression which
  // may use the .length operator on the same (or different)
  // string. We must not consider such composites to be invalid.
  namelen = strcspn(buffer, "#+");

  // For something to be a string it must not have a . in its name
  for(i = 0; i < namelen; i++)
    if(buffer[i] == '.')
      return 0;

  // Valid string
  return 1;
}

void counter_fsg(void)
{
  char cur_char = (builtin_counters[0]).name[0];
  char old_char;
  int i, i2;

  for(i = 0, i2 = 0; i < 256; i++)
  {
    if(i != cur_char)
    {
      counter_first_letter[i * 2] = -1;
      counter_first_letter[(i * 2) + 1] = -1;
    }
    else
    {
      counter_first_letter[i * 2] = i2;
      old_char = cur_char;

      while(cur_char == old_char)
      {
        i2++;
        if(i2 == num_builtin_counters)
          break;
        cur_char = builtin_counters[i2].name[0];
      }

      counter_first_letter[(i * 2) + 1] = i2 - 1;
    }
  }
}

struct counter *load_counter(FILE *fp)
{
  int value = fgetd(fp);
  int name_length = fgetd(fp);

  struct counter *src_counter =
   cmalloc(sizeof(struct counter) + name_length);
  fread(src_counter->name, name_length, 1, fp);
  src_counter->name[name_length] = 0;
  src_counter->value = value;

  src_counter->gateway_write = NULL;
  src_counter->gateway_dec = NULL;

  return src_counter;
}

struct string *load_string(FILE *fp)
{
  int name_length = fgetd(fp);
  int str_length = fgetd(fp);

  struct string *src_string =
   cmalloc(sizeof(struct string) + name_length + str_length - 1);

  fread(src_string->name, name_length, 1, fp);

  src_string->value = src_string->storage_space + name_length;

  fread(src_string->value, str_length, 1, fp);
  src_string->name[name_length] = 0;

  src_string->length = str_length;
  src_string->allocated_length = str_length;

  return src_string;
}

void save_counter(FILE *fp, struct counter *src_counter)
{
  size_t name_length = strlen(src_counter->name);

  fputd(src_counter->value, fp);
  fputd((int)name_length, fp);
  fwrite(src_counter->name, name_length, 1, fp);
}

void save_string(FILE *fp, struct string *src_string)
{
  size_t name_length = strlen(src_string->name);
  size_t str_length = src_string->length;

  fputd((int)name_length, fp);
  fputd((int)str_length, fp);
  fwrite(src_string->name, name_length, 1, fp);
  fwrite(src_string->value, str_length, 1, fp);
}
