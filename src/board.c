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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"

#include "const.h"
#include "error.h"
#include "legacy_board.h"
#include "robot.h"
#include "world.h"
#include "world_prop.h"
#include "util.h"
#include "zip.h"


#define COUNT_BOARD_PROPS (              1 +  7 +        1 + 22)
#define BOUND_BOARD_PROPS (BOARD_NAME_SIZE + 10 + MAX_PATH + 22)
#define BOARD_PROPS_SIZE \
 (COUNT_BOARD_PROPS * PROP_HEADER_SIZE + BOUND_BOARD_PROPS + 2)

#define COUNT_BOARD_SAVE_PROPS (             2 + 20)
#define BOUND_BOARD_SAVE_PROPS (2*ROBOT_MAX_TR + 28)
#define BOARD_SAVE_PROPS_SIZE \
 (COUNT_BOARD_SAVE_PROPS * PROP_HEADER_SIZE + BOUND_BOARD_SAVE_PROPS)

enum board_prop {
  BPROP_EOF                   = 0x0000,

  // Essential                       8     10 + BOARD_NAME_SIZE
  BPROP_BOARD_NAME            = 0x0001, // BOARD_NAME_SIZE
  BPROP_BOARD_WIDTH           = 0x0002, // 2
  BPROP_BOARD_HEIGHT          = 0x0003, // 2
  BPROP_OVERLAY_MODE          = 0x0004, // 1
  BPROP_NUM_ROBOTS            = 0x0005, // 1
  BPROP_NUM_SCROLLS           = 0x0006, // 1
  BPROP_NUM_SENSORS           = 0x0007, // 1
  BPROP_FILE_VERSION          = 0x0008, // 2

  // Non-essential                  22     22 + MAX_PATH
  BPROP_MOD_PLAYING           = 0x0010, // MAX_PATH
  BPROP_VIEWPORT_X            = 0x0011, // 1
  BPROP_VIEWPORT_Y            = 0x0012, // 1
  BPROP_VIEWPORT_WIDTH        = 0x0013, // 1
  BPROP_VIEWPORT_HEIGHT       = 0x0014, // 1
  BPROP_CAN_SHOOT             = 0x0015, // 1
  BPROP_CAN_BOMB              = 0x0016, // 1
  BPROP_FIRE_BURN_BROWN       = 0x0017, // 1
  BPROP_FIRE_BURN_SPACE       = 0x0018, // 1
  BPROP_FIRE_BURN_FAKES       = 0x0019, // 1
  BPROP_FIRE_BURN_TREES       = 0x001A, // 1
  BPROP_EXPLOSIONS_LEAVE      = 0x001B, // 1
  BPROP_SAVE_MODE             = 0x001C, // 1
  BPROP_FOREST_BECOMES        = 0x001D, // 1
  BPROP_COLLECT_BOMBS         = 0x001E, // 1
  BPROP_FIRE_BURNS            = 0x001F, // 1
  BPROP_BOARD_N               = 0x0020, // 1
  BPROP_BOARD_S               = 0x0021, // 1
  BPROP_BOARD_E               = 0x0022, // 1
  BPROP_BOARD_W               = 0x0023, // 1
  BPROP_RESTART_IF_ZAPPED     = 0x0024, // 1
  BPROP_TIME_LIMIT            = 0x0025, // 2
  BPROP_PLAYER_NS_LOCKED      = 0x0026, // 1
  BPROP_PLAYER_EW_LOCKED      = 0x0027, // 1
  BPROP_PLAYER_ATTACK_LOCKED  = 0x0028, // 1

  // Save                           20     28 + 2 ROBOT_MAX_TR
  BPROP_SCROLL_X              = 0x0100, // 2
  BPROP_SCROLL_Y              = 0x0101, // 2
  BPROP_LOCKED_X              = 0x0102, // 2
  BPROP_LOCKED_Y              = 0x0103, // 2
  BPROP_PLAYER_LAST_DIR       = 0x0104, // 1
  BPROP_LAZWALL_START         = 0x010A, // 1
  BPROP_LAST_KEY              = 0x010B, // 1
  BPROP_NUM_INPUT             = 0x010C, // 4
  BPROP_INPUT_SIZE            = 0x010D, // 4 (IS SEPARATE FROM INPUT_STRING)
  BPROP_INPUT_STRING          = 0x010E, // ROBOT_MAX_TR
  BRPOP_BOTTOM_MESG           = 0x0110, // ROBOT_MAX_TR
  BPROP_BOTTOM_MESG_TIMER     = 0x0111, // 1
  BPROP_BOTTOM_MESG_ROW       = 0x0112, // 1
  BPROP_BOTTOM_MESG_COL       = 0x0113, // 1
  BPROP_VOLUME                = 0x0114, // 1
  BPROP_VOLUME_INC            = 0x0115, // 1
  BPROP_VOLUME_TARGET         = 0x0116, // 1
};

static void save_board_info(struct board *cur_board, struct zip_archive *zp,
 int savegame, int file_version, const char *name, int file_id, int board_id)
{
  char *buffer;
  struct memfile mf;

  unsigned int size = BOARD_PROPS_SIZE;
  int length;

  if(savegame)
    size += BOARD_SAVE_PROPS_SIZE;

  buffer = cmalloc(size);

  mfopen_static(buffer, size, &mf);

  save_prop_s(BPROP_BOARD_NAME, cur_board->board_name, BOARD_NAME_SIZE, 1, &mf);
  save_prop_w(BPROP_BOARD_WIDTH, cur_board->board_width, &mf);
  save_prop_w(BPROP_BOARD_HEIGHT, cur_board->board_height, &mf);
  save_prop_c(BPROP_OVERLAY_MODE, cur_board->overlay_mode, &mf);
  save_prop_c(BPROP_NUM_ROBOTS, cur_board->num_robots, &mf);
  save_prop_c(BPROP_NUM_SCROLLS, cur_board->num_scrolls, &mf);
  save_prop_c(BPROP_NUM_SENSORS, cur_board->num_sensors, &mf);
  save_prop_w(BPROP_FILE_VERSION, file_version, &mf);

  length = strlen(cur_board->mod_playing);
  save_prop_s(BPROP_MOD_PLAYING, cur_board->mod_playing, length, 1, &mf);
  save_prop_c(BPROP_VIEWPORT_X, cur_board->viewport_x, &mf);
  save_prop_c(BPROP_VIEWPORT_Y, cur_board->viewport_y, &mf);
  save_prop_c(BPROP_VIEWPORT_WIDTH, cur_board->viewport_width, &mf);
  save_prop_c(BPROP_VIEWPORT_HEIGHT, cur_board->viewport_height, &mf);
  save_prop_c(BPROP_CAN_SHOOT, cur_board->can_shoot, &mf);
  save_prop_c(BPROP_CAN_BOMB, cur_board->can_bomb, &mf);
  save_prop_c(BPROP_FIRE_BURN_BROWN, cur_board->fire_burn_brown, &mf);
  save_prop_c(BPROP_FIRE_BURN_SPACE, cur_board->fire_burn_space, &mf);
  save_prop_c(BPROP_FIRE_BURN_FAKES, cur_board->fire_burn_fakes, &mf);
  save_prop_c(BPROP_FIRE_BURN_TREES, cur_board->fire_burn_trees, &mf);
  save_prop_c(BPROP_EXPLOSIONS_LEAVE, cur_board->explosions_leave, &mf);
  save_prop_c(BPROP_SAVE_MODE, cur_board->save_mode, &mf);
  save_prop_c(BPROP_FOREST_BECOMES, cur_board->forest_becomes, &mf);
  save_prop_c(BPROP_COLLECT_BOMBS, cur_board->collect_bombs, &mf);
  save_prop_c(BPROP_FIRE_BURNS, cur_board->fire_burns, &mf);
  save_prop_c(BPROP_BOARD_N, cur_board->board_dir[0], &mf);
  save_prop_c(BPROP_BOARD_S, cur_board->board_dir[1], &mf);
  save_prop_c(BPROP_BOARD_E, cur_board->board_dir[2], &mf);
  save_prop_c(BPROP_BOARD_W, cur_board->board_dir[3], &mf);
  save_prop_c(BPROP_RESTART_IF_ZAPPED, cur_board->restart_if_zapped, &mf);
  save_prop_c(BPROP_TIME_LIMIT, cur_board->time_limit, &mf);
  save_prop_c(BPROP_PLAYER_NS_LOCKED, cur_board->player_ns_locked, &mf);
  save_prop_c(BPROP_PLAYER_EW_LOCKED, cur_board->player_ew_locked, &mf);
  save_prop_c(BPROP_PLAYER_ATTACK_LOCKED, cur_board->player_attack_locked, &mf);

  if(savegame)
  {
    save_prop_w(BPROP_SCROLL_X, cur_board->scroll_x, &mf);
    save_prop_w(BPROP_SCROLL_Y, cur_board->scroll_y, &mf);
    save_prop_w(BPROP_LOCKED_X, cur_board->locked_x, &mf);
    save_prop_w(BPROP_LOCKED_Y, cur_board->locked_y, &mf);
    save_prop_c(BPROP_PLAYER_LAST_DIR, cur_board->player_last_dir, &mf);
    save_prop_c(BPROP_LAZWALL_START, cur_board->lazwall_start, &mf);
    save_prop_c(BPROP_LAST_KEY, cur_board->last_key, &mf);
    save_prop_d(BPROP_NUM_INPUT, cur_board->num_input, &mf);
    save_prop_d(BPROP_INPUT_SIZE, cur_board->input_size, &mf);

    length = strlen(cur_board->input_string);
    save_prop_s(BPROP_INPUT_STRING, cur_board->input_string, length, 1, &mf);

    length = strlen(cur_board->bottom_mesg);
    save_prop_s(BRPOP_BOTTOM_MESG, cur_board->bottom_mesg, length, 1, &mf);
    save_prop_c(BPROP_BOTTOM_MESG_TIMER, cur_board->b_mesg_timer, &mf);
    save_prop_c(BPROP_BOTTOM_MESG_ROW, cur_board->b_mesg_row, &mf);
    save_prop_c(BPROP_BOTTOM_MESG_COL, cur_board->b_mesg_col, &mf);
    save_prop_c(BPROP_VOLUME, cur_board->volume, &mf);
    save_prop_c(BPROP_VOLUME_INC, cur_board->volume_inc, &mf);
    save_prop_c(BPROP_VOLUME_TARGET, cur_board->volume_target, &mf);
  }

  save_prop_eof(&mf);

  size = mftell(&mf);

  zip_write_file(zp, name, buffer, size, ZIP_M_NONE, file_id, board_id, 0);

  free(buffer);
}

int save_board(struct world *mzx_world, struct board *cur_board,
 struct zip_archive *zp, int savegame, int file_version, int board_id)
{
  int board_size = cur_board->board_width * cur_board->board_height;
  char name[10];
  int count;
  int i;

  sprintf(name, "b%2.2X", (unsigned char)board_id);

  save_board_info(cur_board, zp, savegame, file_version,
   name, FPROP_BOARD_INFO, board_id);

  sprintf(name+3, "bid");
  zip_write_file(zp, name, cur_board->level_id,
   board_size, ZIP_M_DEFLATE, FPROP_BOARD_BID, board_id, 0);

  sprintf(name+3, "bpr");
  zip_write_file(zp, name, cur_board->level_param,
   board_size, ZIP_M_DEFLATE, FPROP_BOARD_BPR, board_id, 0);

  sprintf(name+3, "bco");
  zip_write_file(zp, name, cur_board->level_color,
   board_size, ZIP_M_DEFLATE, FPROP_BOARD_BCO, board_id, 0);

  sprintf(name+3, "uid");
  zip_write_file(zp, name, cur_board->level_under_id,
   board_size, ZIP_M_DEFLATE, FPROP_BOARD_UID, board_id, 0);

  sprintf(name+3, "upr");
  zip_write_file(zp, name, cur_board->level_under_param,
   board_size, ZIP_M_DEFLATE, FPROP_BOARD_UPR, board_id, 0);

  sprintf(name+3, "uco");
  zip_write_file(zp, name, cur_board->level_under_color,
   board_size, ZIP_M_DEFLATE, FPROP_BOARD_UCO, board_id, 0);

  if(cur_board->overlay_mode)
  {
    sprintf(name+3, "och");
    zip_write_file(zp, name, cur_board->overlay,
     board_size, ZIP_M_DEFLATE, FPROP_BOARD_OCH, board_id, 0);

    sprintf(name+3, "oco");
    zip_write_file(zp, name, cur_board->overlay_color,
     board_size, ZIP_M_DEFLATE, FPROP_BOARD_OCO, board_id, 0);
  }

  name[3] = 'r';
  count = cur_board->num_robots;
  if(count)
  {
    struct robot *cur_robot;

    for(i = 1; i <= count; i++)
    {
      cur_robot = cur_board->robot_list[i];
      if(cur_robot)
      {
        sprintf(name+4, "%2.2X", i);
        save_robot(mzx_world, cur_robot, zp, savegame, file_version,
         name, FPROP_ROBOT, board_id, i);
      }
    }
  }

  sprintf(name+3, "sc");
  count = cur_board->num_scrolls;
  if(count)
  {
    struct scroll *cur_scroll;

    for(i = 1; i <= count; i++)
    {
      cur_scroll = cur_board->scroll_list[i];
      if(cur_scroll)
      {
        sprintf(name+5, "%2.2X", i);
        save_scroll(cur_scroll, zp, name, FPROP_SCROLL, board_id, i);
      }
    }
  }

  sprintf(name+3, "se");
  count = cur_board->num_sensors;
  if(count)
  {
    struct sensor *cur_sensor;

    for(i = 1; i <= count; i++)
    {
      cur_sensor = cur_board->sensor_list[i];
      if(cur_sensor)
      {
        sprintf(name+5, "%2.2X", i);
        save_sensor(cur_sensor, zp, name, FPROP_SENSOR, board_id, i);
      }
    }
  }

  return 0;
}

static void default_board(struct board *cur_board)
{
  cur_board->mod_playing[0] = 0;
  cur_board->viewport_x = 0;
  cur_board->viewport_y = 0;
  cur_board->viewport_width = 80;
  cur_board->viewport_height = 25;
  cur_board->can_shoot = 0;
  cur_board->can_bomb = 0;
  cur_board->fire_burn_brown = 0;
  cur_board->fire_burn_space = 0;
  cur_board->fire_burn_fakes = 0;
  cur_board->fire_burn_trees = 0;
  cur_board->explosions_leave = 0;
  cur_board->save_mode = 1;
  cur_board->forest_becomes = 0;
  cur_board->collect_bombs = 0;
  cur_board->fire_burns = 0;
  cur_board->board_dir[0] = 0;
  cur_board->board_dir[1] = 0;
  cur_board->board_dir[2] = 0;
  cur_board->board_dir[3] = 0;
  cur_board->restart_if_zapped = 0;
  cur_board->time_limit = 0;

  cur_board->scroll_x = 0;
  cur_board->scroll_y = 0;
  cur_board->locked_x = -1;
  cur_board->locked_y = -1;

  cur_board->player_last_dir = 0x10;
  cur_board->player_ns_locked = 0;
  cur_board->player_ew_locked = 0;
  cur_board->player_attack_locked = 0;

  cur_board->volume = 255;
  cur_board->volume_inc = 0;
  cur_board->volume_target = 255;

  cur_board->lazwall_start = 7;
  cur_board->last_key = '?';
  cur_board->num_input = 0;
  cur_board->input_size = 0;
  cur_board->input_string[0] = 0;
  cur_board->bottom_mesg[0] = 0;
  cur_board->b_mesg_timer = 0;
  cur_board->b_mesg_row = 24;
  cur_board->b_mesg_col = -1;
}

void dummy_board(struct board *cur_board)
{
  // Allocate placeholder data for broken boards so they will run
  int size = 2000;
  cur_board->overlay_mode = 0;
  cur_board->board_width = 80;
  cur_board->board_height = 25;

  default_board(cur_board);

  cur_board->level_id = ccalloc(1, size);
  cur_board->level_color = ccalloc(1, size);
  cur_board->level_param = ccalloc(1, size);
  cur_board->level_under_id = ccalloc(1, size);
  cur_board->level_under_color = ccalloc(1, size);
  cur_board->level_under_param = ccalloc(1, size);

  cur_board->level_id[0] = 127;

  cur_board->num_robots = 0;
  cur_board->num_robots_active = 0;
  cur_board->num_robots_allocated = 0;
  cur_board->robot_list = ccalloc(1, sizeof(struct robot *));
  cur_board->robot_list_name_sorted = ccalloc(1, sizeof(struct robot *));
  cur_board->num_scrolls = 0;
  cur_board->num_scrolls_allocated = 0;
  cur_board->scroll_list = ccalloc(1, sizeof(struct scroll *));
  cur_board->num_sensors = 0;
  cur_board->num_sensors_allocated = 0;
  cur_board->sensor_list = ccalloc(1, sizeof(struct sensor *));
}

#define err_if_missing(expected) if(last_ident < expected) { goto err_free; }

static int load_board_info(struct board *cur_board, struct zip_archive *zp,
 int savegame, int *file_version)
{
  char *buffer;
  unsigned int actual_size;
  struct memfile mf;
  struct memfile prop;
  int last_ident = -1;
  int ident;
  int size;
  int v;

  zip_get_next_uncompressed_size(zp, &actual_size);

  buffer = cmalloc(actual_size);

  zip_read_file(zp, buffer, actual_size, &actual_size);

  mfopen_static(buffer, actual_size, &mf);

  while(next_prop(&prop, &ident, &size, &mf))
  {
    switch(ident)
    {
      case BPROP_EOF:
        mfseek(&mf, 0, SEEK_END);
        break;

      // Essential
      case BPROP_BOARD_NAME:
        size = MIN(size, BOARD_NAME_SIZE);
        mfread(cur_board->board_name, size, 1, &prop);
        cur_board->board_name[BOARD_NAME_SIZE - 1] = 0;
        break;

      case BPROP_BOARD_WIDTH:
        err_if_missing(BPROP_BOARD_NAME);
        cur_board->board_width = load_prop_int(size, &prop);
        if(cur_board->board_width < 1)
          goto err_free;
        break;

      case BPROP_BOARD_HEIGHT:
        err_if_missing(BPROP_BOARD_WIDTH);
        cur_board->board_height = load_prop_int(size, &prop);
        if(cur_board->board_height < 1)
          goto err_free;
        break;

      case BPROP_OVERLAY_MODE:
        err_if_missing(BPROP_BOARD_HEIGHT);
        v = load_prop_int(size, &prop);
        cur_board->overlay_mode = CLAMP(v, 0, 3);
        break;

      case BPROP_NUM_ROBOTS:
        err_if_missing(BPROP_OVERLAY_MODE);
        v = load_prop_int(size, &prop) & 0xFF;
        cur_board->num_robots = v;
        cur_board->num_robots_allocated = v;
        break;

      case BPROP_NUM_SCROLLS:
        err_if_missing(BPROP_NUM_ROBOTS);
        v = load_prop_int(size, &prop) & 0xFF;
        cur_board->num_scrolls = v;
        cur_board->num_scrolls_allocated = v;
        break;

      case BPROP_NUM_SENSORS:
        err_if_missing(BPROP_NUM_SCROLLS);
        v = load_prop_int(size, &prop) & 0xFF;
        cur_board->num_sensors = v;
        cur_board->num_sensors_allocated = v;
        break;

      case BPROP_FILE_VERSION:
        err_if_missing(BPROP_NUM_SENSORS);
        *file_version = load_prop_int(size, &prop);
        break;


      // Non-essential
      case BPROP_MOD_PLAYING:
        size = MIN(size, MAX_PATH-1);
        mfread(cur_board->mod_playing, size, 1, &prop);
        cur_board->mod_playing[size] = 0;
        break;

      case BPROP_VIEWPORT_X:
        v = load_prop_int(size, &prop);
        cur_board->viewport_x = CLAMP(v, 0, 79);
        break;

      case BPROP_VIEWPORT_Y:
        v = load_prop_int(size, &prop);
        cur_board->viewport_y = CLAMP(v, 0, 24);
        break;

      case BPROP_VIEWPORT_WIDTH:
        v = load_prop_int(size, &prop);
        v = CLAMP(v, 1, 80 - cur_board->viewport_x);
        v = CLAMP(v, 1, cur_board->board_width);
        cur_board->viewport_width = v;
        break;

      case BPROP_VIEWPORT_HEIGHT:
        v = load_prop_int(size, &prop);
        v = CLAMP(v, 1, 25 - cur_board->viewport_y);
        v = CLAMP(v, 1, cur_board->board_height);
        cur_board->viewport_height = v;
        break;

      case BPROP_CAN_SHOOT:
        cur_board->can_shoot = load_prop_int(size, &prop);
        break;

      case BPROP_CAN_BOMB:
        cur_board->can_bomb = load_prop_int(size, &prop);
        break;

      case BPROP_FIRE_BURN_BROWN:
        cur_board->fire_burn_brown = load_prop_int(size, &prop);
        break;

      case BPROP_FIRE_BURN_SPACE:
        cur_board->fire_burn_space = load_prop_int(size, &prop);
        break;

      case BPROP_FIRE_BURN_FAKES:
        cur_board->fire_burn_fakes = load_prop_int(size, &prop);
        break;

      case BPROP_FIRE_BURN_TREES:
        cur_board->fire_burn_trees = load_prop_int(size, &prop);
        break;

      case BPROP_EXPLOSIONS_LEAVE:
        cur_board->explosions_leave = load_prop_int(size, &prop);
        break;

      case BPROP_SAVE_MODE:
        cur_board->save_mode = load_prop_int(size, &prop);
        break;

      case BPROP_FOREST_BECOMES:
        cur_board->forest_becomes = load_prop_int(size, &prop);
        break;

      case BPROP_COLLECT_BOMBS:
        cur_board->collect_bombs = load_prop_int(size, &prop);
        break;

      case BPROP_FIRE_BURNS:
        cur_board->fire_burns = load_prop_int(size, &prop);
        break;

      case BPROP_BOARD_N:
        cur_board->board_dir[0] = load_prop_int(size, &prop);
        break;

      case BPROP_BOARD_S:
        cur_board->board_dir[1] = load_prop_int(size, &prop);
        break;

      case BPROP_BOARD_E:
        cur_board->board_dir[2] = load_prop_int(size, &prop);
        break;

      case BPROP_BOARD_W:
        cur_board->board_dir[3] = load_prop_int(size, &prop);
        break;

      case BPROP_RESTART_IF_ZAPPED:
        cur_board->restart_if_zapped = load_prop_int(size, &prop);
        break;

      case BPROP_TIME_LIMIT:
        cur_board->time_limit = load_prop_int(size, &prop);
        break;

      case BPROP_PLAYER_NS_LOCKED:
        cur_board->player_ns_locked = load_prop_int(size, &prop);
        break;

      case BPROP_PLAYER_EW_LOCKED:
        cur_board->player_ew_locked = load_prop_int(size, &prop);
        break;

      case BPROP_PLAYER_ATTACK_LOCKED:
        cur_board->player_attack_locked = load_prop_int(size, &prop);
        break;



      // Savegame only
      case BPROP_SCROLL_X:
        cur_board->scroll_x = (signed short) load_prop_int(size, &prop);
        break;

      case BPROP_SCROLL_Y:
        cur_board->scroll_y = (signed short) load_prop_int(size, &prop);
        break;

      case BPROP_LOCKED_X:
        cur_board->locked_x = (signed short) load_prop_int(size, &prop);
        break;

      case BPROP_LOCKED_Y:
        cur_board->locked_y = (signed short) load_prop_int(size, &prop);
        break;

      case BPROP_PLAYER_LAST_DIR:
        cur_board->player_last_dir = load_prop_int(size, &prop);
        break;

      case BPROP_LAZWALL_START:
        cur_board->lazwall_start = load_prop_int(size, &prop);
        break;

      case BPROP_LAST_KEY:
        cur_board->last_key = load_prop_int(size, &prop);
        break;

      case BPROP_NUM_INPUT:
        cur_board->num_input = load_prop_int(size, &prop);
        break;

      case BPROP_INPUT_SIZE:
        cur_board->input_size = load_prop_int(size, &prop);
        break;

      case BPROP_INPUT_STRING:
        size = MIN(size, ROBOT_MAX_TR-1);
        mfread(cur_board->input_string, size, 1, &prop);
        cur_board->input_string[size] = 0;
        break;

      case BRPOP_BOTTOM_MESG:
        size = MIN(size, ROBOT_MAX_TR-1);
        mfread(cur_board->bottom_mesg, size, 1, &prop);
        cur_board->bottom_mesg[size] = 0;
        break;

      case BPROP_BOTTOM_MESG_TIMER:
        cur_board->b_mesg_timer = load_prop_int(size, &prop);
        break;

      case BPROP_BOTTOM_MESG_ROW:
        cur_board->b_mesg_row = load_prop_int(size, &prop);
        break;

      case BPROP_BOTTOM_MESG_COL:
        cur_board->b_mesg_col = (signed char) load_prop_int(size, &prop);
        break;

      case BPROP_VOLUME:
        cur_board->volume = load_prop_int(size, &prop);
        break;

      case BPROP_VOLUME_INC:
        cur_board->volume_inc = load_prop_int(size, &prop);
        break;

      case BPROP_VOLUME_TARGET:
        cur_board->volume_target = load_prop_int(size, &prop);
        break;

      default:
        break;
    }
    last_ident = ident;
  }

  err_if_missing(BPROP_FILE_VERSION);

  size = cur_board->board_width * cur_board->board_height;

  if(size <= 0 || size > MAX_BOARD_SIZE)
    goto err_free;

  free(buffer);
  return 0;

err_free:
  free(buffer);
  return 1;
}


static int cmp_robots(const void *dest, const void *src)
{
  struct robot *rsrc = *((struct robot **)src);
  struct robot *rdest = *((struct robot **)dest);
  return strcasecmp(rdest->robot_name, rsrc->robot_name);
}

__editor_maybe_static
int load_board_direct(struct world *mzx_world, struct board *cur_board,
 struct zip_archive *zp, int savegame, int file_version, unsigned int board_id)
{
  unsigned int file_id;
  unsigned int board_id_read;
  unsigned int robot_id_read;

  size_t board_size = 0;

  struct robot **robot_list;
  struct robot **robot_list_name_sorted;
  struct scroll **scroll_list;
  struct sensor **sensor_list;

  char found_robots[256] = {0};
  char found_scrolls[256] = {0};
  char found_sensors[256] = {0};

  unsigned int num_robots;
  unsigned int num_scrolls;
  unsigned int num_sensors;
  unsigned int num_robots_active = 0;

  unsigned int i;

  int has_base = 0;
  int has_bid = 0;
  int has_bpr = 0;
  int has_bco = 0;
  int has_uid = 0;
  int has_upr = 0;
  int has_uco = 0;
  int has_och = 0;
  int has_oco = 0;

  cur_board->world_version = mzx_world->version;
  default_board(cur_board);

  while(ZIP_SUCCESS ==
   zip_get_next_prop(zp, &file_id, &board_id_read, &robot_id_read))
  {
    if(board_id_read != board_id)
      break;

    switch(file_id)
    {
      case FPROP_BOARD_INFO:
      {
        if(load_board_info(cur_board, zp, savegame, &file_version))
          goto err_invalid;

        has_base = 1;
        board_size = cur_board->board_width * cur_board->board_height;

        cur_board->level_id = ccalloc(1, board_size);
        cur_board->level_param = ccalloc(1, board_size);
        cur_board->level_color = ccalloc(1, board_size);
        cur_board->level_under_id = ccalloc(1, board_size);
        cur_board->level_under_param = ccalloc(1, board_size);
        cur_board->level_under_color = ccalloc(1, board_size);

        if(cur_board->overlay_mode)
        {
          cur_board->overlay = ccalloc(1, board_size);
          cur_board->overlay_color = ccalloc(1, board_size);
        }

        num_robots = cur_board->num_robots;
        num_scrolls = cur_board->num_scrolls;
        num_sensors = cur_board->num_sensors;

        robot_list = ccalloc(num_robots + 1, sizeof(struct robot *));

        robot_list_name_sorted =
         ccalloc(num_robots, sizeof(struct robot *));

        scroll_list = ccalloc(num_scrolls + 1, sizeof(struct scroll *));

        sensor_list = ccalloc(num_sensors + 1, sizeof(struct sensor *));

        cur_board->robot_list = robot_list;
        cur_board->robot_list_name_sorted = robot_list_name_sorted;
        cur_board->scroll_list = scroll_list;
        cur_board->sensor_list = sensor_list;

        break;
      }

      case FPROP_BOARD_BID:
      {
        if(!has_base)
          goto err_invalid;

        has_bid = 1;
        zip_read_file(zp, cur_board->level_id, board_size, NULL);
        break;
      }

      case FPROP_BOARD_BPR:
      {
        if(!has_base)
          goto err_invalid;

        has_bpr = 1;
        zip_read_file(zp, cur_board->level_param, board_size, NULL);
        break;
      }

      case FPROP_BOARD_BCO:
      {
        if(!has_base)
          goto err_invalid;

        has_bco = 1;
        zip_read_file(zp, cur_board->level_color, board_size, NULL);
        break;
      }

      case FPROP_BOARD_UID:
      {
        if(!has_base)
          goto err_invalid;

        has_uid = 1;
        zip_read_file(zp, cur_board->level_under_id, board_size, NULL);
        break;
      }

      case FPROP_BOARD_UPR:
      {
        if(!has_base)
          goto err_invalid;

        has_upr = 1;
        zip_read_file(zp, cur_board->level_under_param, board_size, NULL);
        break;
      }

      case FPROP_BOARD_UCO:
      {
        if(!has_base)
          goto err_invalid;

        has_uco = 1;
        zip_read_file(zp, cur_board->level_under_color, board_size, NULL);
        break;
      }

      case FPROP_BOARD_OCH:
      {
        if(!has_base)
          goto err_invalid;

        if(cur_board->overlay_mode)
        {
          has_och = 1;
          zip_read_file(zp, cur_board->overlay, board_size, NULL);
        }
        else
        {
          zip_skip_file(zp);
        }

        break;
      }

      case FPROP_BOARD_OCO:
      {
        if(!has_base)
          goto err_invalid;

        if(cur_board->overlay_mode)
        {
          has_oco = 1;
          zip_read_file(zp, cur_board->overlay_color, board_size, NULL);
        }
        else
        {
          zip_skip_file(zp);
        }

        break;
      }

      case FPROP_ROBOT:
      {
        if(!has_base)
          goto err_invalid;

        if(robot_id_read <= num_robots && !robot_list[robot_id_read])
        {
          struct robot *cur_robot;

          cur_robot = load_robot_allocate(mzx_world, zp, savegame, file_version);

          robot_list[robot_id_read] = cur_robot;
          robot_list_name_sorted[num_robots_active] = cur_robot;

          num_robots_active++;
        }
        else
        {
          zip_skip_file(zp);
        }
        break;
      }

      case FPROP_SCROLL:
      {
        if(!has_base)
          goto err_invalid;

        if(robot_id_read <= num_scrolls && !scroll_list[robot_id_read])
        {
          scroll_list[robot_id_read] = load_scroll_allocate(zp);
        }
        else
        {
          zip_skip_file(zp);
        }
        break;
      }

      case FPROP_SENSOR:
      {
        if(!has_base)
          goto err_invalid;

        if(robot_id_read <= num_sensors && !sensor_list[robot_id_read])
        {
          sensor_list[robot_id_read] = load_sensor_allocate(zp);
        }
        else
        {
          zip_skip_file(zp);
        }
        break;
      }

      default:
        break;
    }
  }

  if(!has_base)
    goto err_invalid;

  if(!has_bid)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "bid");

  if(!has_bpr)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "bpr");

  if(!has_bco)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "bco");

  if(!has_uid)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "uid");

  if(!has_upr)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "upr");

  if(!has_uco)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "uco");

  if(!has_och && cur_board->overlay_mode)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "och");

  if(!has_oco && cur_board->overlay_mode)
    error_message(E_ZIP_BOARD_MISSING_DATA, (int)board_id, "oco");

  /* Sort the name sorted list.
   * Do not shorten it; the name sorted list is expected to be num_robots in
   * length at all times, though it's not obvious from the way the legacy loader
   * works. After loading, the legacy loader will perform optimize_null_objects,
   * which removes the unused robots. However, this loader does not, to preserve
   * robot IDs when loading saves. Just leave nulls in the spaces at the end.
   */
  if(num_robots_active > 0)
  {
    qsort(robot_list_name_sorted, num_robots_active,
     sizeof(struct robot *), cmp_robots);
  }

  cur_board->num_robots_active = num_robots_active;

  // Insert the global robot.
  robot_list[0] = &(mzx_world->global_robot);

  // Validate the data on the board.
  if(has_bid)
  {
    char *level_id = cur_board->level_id;
    char *level_param = cur_board->level_param;
    char *level_color = cur_board->level_color;
    char *level_under_id = cur_board->level_under_id;
    unsigned char id;
    unsigned char pr;
    int err;

    // These IDs aren't allowed.
    found_robots[0] = 1;
    found_scrolls[0] = 1;
    found_sensors[0] = 1;

    for(i = 0; i < board_size; i++)
    {
      id = level_id[i];

      switch(id)
      {
        case ROBOT:
        case ROBOT_PUSHABLE:
        {
          pr = level_param[i];

          if(!found_robots[pr] && pr<=num_robots && robot_list[pr])
          {
            found_robots[pr] = 1;
          }
          else
          {
            err = (board_id << 8)|pr;

            if(found_robots[pr])
              error_message(E_ZIP_ROBOT_DUPLICATED, err, NULL);
            else
              error_message(E_ZIP_ROBOT_MISSING_FROM_DATA, err, NULL);

            level_id[i] = CUSTOM_BLOCK;
            level_param[i] = 'R';
            level_color[i] = 0xCF;
          }

          break;
        }

        case SIGN:
        case SCROLL:
        {
          pr = level_param[i];

          if(!found_scrolls[pr] && pr<=num_scrolls && scroll_list[pr])
          {
            found_scrolls[pr] = 1;
          }
          else
          {
            // Silently fix.
            level_id[i] = CUSTOM_BLOCK;
            level_param[i] = 'S';
            level_color[i] = 0xCF;
          }
          break;
        }

        case SENSOR:
        {
          pr = level_param[i];

          if(!found_sensors[pr] && pr<=num_sensors && sensor_list[pr])
          {
            found_sensors[pr] = 1;
          }
          else
          {
            // Silently fix.
            level_id[i] = CUSTOM_FLOOR;
            level_param[i] = 'S';
            level_color[i] = 0xDF;
          }
        }

        default:
        {
          // If for any reason extkinds find their way into a 2.90+ file...
          if(id > 127)
            level_id[i] = CUSTOM_BLOCK;
          break;
        }
      }

      id = level_under_id[i];

      switch(id)
      {
        case SENSOR:
        {
          pr = cur_board->level_under_param[i];

          if(!found_sensors[pr] && pr<=num_sensors && sensor_list[pr])
          {
            found_sensors[pr] = 1;
          }
          else
          {
            // Silently fix.
            level_under_id[i] = CUSTOM_FLOOR;
            cur_board->level_param[i] = 'S';
            cur_board->level_color[i] = 0xDF;
          }
          break;
        }

        default:
        {
          if(level_under_id[i] > 127)
            level_under_id[i] = CUSTOM_FLOOR;
          break;
        }
      }
    }
  }

  // Now, make sure everything loaded was found on the board.
  for(i = 1; i <= num_robots; i++)
  {
    if(robot_list[i] && !found_robots[i])
    {
      // Deleting these is a pain. Just leave them.
      error_message(E_ZIP_ROBOT_MISSING_FROM_BOARD, (board_id << 8)|i, NULL);
    }
  }

  for(i = 1; i <= num_scrolls; i++)
  {
    if(scroll_list[i] && !found_scrolls[i])
    {
      // Silently fix.
      clear_scroll(scroll_list[i]);
      scroll_list[i] = NULL;
    }
  }

  for(i = 1; i <= num_sensors; i++)
  {
    if(sensor_list[i] && !found_sensors[i])
    {
      // Silently fix.
      clear_sensor(sensor_list[i]);
      sensor_list[i] = NULL;
    }
  }

  return VAL_SUCCESS;

err_invalid:
  error_message(E_ZIP_BOARD_CORRUPT, (int)board_id, NULL);
  dummy_board(cur_board);

  return VAL_INVALID;
}

struct board *load_board_allocate(struct world *mzx_world,
 struct zip_archive *zp, int savegame, int file_version, unsigned int board_id)
{
  struct board *cur_board = cmalloc(sizeof(struct board));

  load_board_direct(mzx_world, cur_board, zp, savegame, file_version, board_id);

  return cur_board;
}

void clear_board(struct board *cur_board)
{
  int i;
  int num_robots_active = cur_board->num_robots_active;
  int num_scrolls = cur_board->num_scrolls;
  int num_sensors = cur_board->num_sensors;
  struct robot **robot_list = cur_board->robot_list;
  struct robot **robot_name_list = cur_board->robot_list_name_sorted;
  struct scroll **scroll_list = cur_board->scroll_list;
  struct sensor **sensor_list = cur_board->sensor_list;

  free(cur_board->level_id);
  free(cur_board->level_param);
  free(cur_board->level_color);
  free(cur_board->level_under_id);
  free(cur_board->level_under_param);
  free(cur_board->level_under_color);

  if(cur_board->overlay_mode)
  {
    free(cur_board->overlay);
    free(cur_board->overlay_color);
  }

  for(i = 0; i < num_robots_active; i++)
    if(robot_name_list[i])
      clear_robot(robot_name_list[i]);

  free(robot_name_list);
  free(robot_list);

  for(i = 1; i <= num_scrolls; i++)
    if(scroll_list[i])
      clear_scroll(scroll_list[i]);

  free(scroll_list);

  for(i = 1; i <= num_sensors; i++)
    if(sensor_list[i])
      clear_sensor(sensor_list[i]);

  free(sensor_list);
  free(cur_board);
}

// Just a linear search. Boards aren't addressed by name very often.
int find_board(struct world *mzx_world, char *name)
{
  struct board **board_list = mzx_world->board_list;
  struct board *current_board;
  int num_boards = mzx_world->num_boards;
  int i;
  for(i = 0; i < num_boards; i++)
  {
    current_board = board_list[i];
    if(current_board && !strcasecmp(name, current_board->board_name))
      break;
  }

  if(i != num_boards)
  {
    return i;
  }
  else
  {
    return NO_BOARD;
  }
}
