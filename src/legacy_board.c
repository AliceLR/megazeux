/* MegaZeux
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "legacy_board.h"
#include "legacy_robot.h"

#include "board.h"
#include "const.h"
#include "error.h"
#include "robot.h"
#include "util.h"
#include "world.h"
#include "world_struct.h"

/* 13 (not NULL terminated in format) */
#define LEGACY_MOD_FILENAME_MAX 13

/* 80 (w/o saved NULL terminator) */
#define LEGACY_BOTTOM_MESG_MAX  80

/* 80 (w/o saved NULL terminator) */
#define LEGACY_INPUT_STRING_MAX 80


static int cmp_robots(const void *dest, const void *src)
{
  struct robot *rsrc = *((struct robot **)src);
  struct robot *rdest = *((struct robot **)dest);
  return strcasecmp(rdest->robot_name, rsrc->robot_name);
}

static int load_RLE2_plane(char *plane, FILE *fp, int size)
{
  int i, runsize;
  int current_char;

  if(size < 0)
    return -3;

  for(i = 0; i < size; i++)
  {
    current_char = fgetc(fp);
    if(current_char == EOF)
    {
      return -1;
    }
    else if(!(current_char & 0x80))
    {
      // Regular character
      plane[i] = current_char;
    }
    else
    {
      // A run
      runsize = current_char & 0x7F;
      if((i + runsize) > size)
        return -2;

      current_char = fgetc(fp);
      if(current_char == EOF)
        return -1;

      memset(plane + i, current_char, runsize);
      i += (runsize - 1);
    }
  }

  return 0;
}

int legacy_load_board_direct(struct world *mzx_world, struct board *cur_board,
 FILE *fp, int data_size, int savegame, int file_version)
{
  int num_robots, num_scrolls, num_sensors, num_robots_active;
  int overlay_mode, size, board_width, board_height, i;
  int viewport_x, viewport_y, viewport_width, viewport_height;
  boolean truncated = false;

  struct robot *cur_robot;
  struct scroll *cur_scroll;
  struct sensor *cur_sensor;

  int board_location = ftell(fp);

  cur_board->num_robots = 0;
  cur_board->num_robots_allocated = 0;
  cur_board->num_robots_active = 0;
  cur_board->num_scrolls = 0;
  cur_board->num_scrolls_allocated = 0;
  cur_board->num_sensors = 0;
  cur_board->num_sensors_allocated = 0;
  cur_board->robot_list = NULL;
  cur_board->robot_list_name_sorted = NULL;
  cur_board->sensor_list = NULL;
  cur_board->scroll_list = NULL;

  // Initialize some fields that may no longer be loaded
  // from the board file itself..

  cur_board->last_key = '?';
  cur_board->num_input = 0;
  cur_board->input_size = 0;
  cur_board->input_string[0] = 0;
  cur_board->player_last_dir = 0x10;
  cur_board->bottom_mesg[0] = 0;
  cur_board->b_mesg_timer = 0;
  cur_board->lazwall_start = 7;
  cur_board->b_mesg_row = 24;
  cur_board->b_mesg_col = -1;
  cur_board->scroll_x = 0;
  cur_board->scroll_y = 0;
  cur_board->locked_x = -1;
  cur_board->locked_y = -1;
  cur_board->volume = 255;
  cur_board->volume_inc = 0;
  cur_board->volume_target = 255;
  cur_board->reset_on_entry = 0;
  cur_board->charset_path[0] = 0;
  cur_board->palette_path[0] = 0;

#ifdef DEBUG
  cur_board->is_extram = false;
#endif

  // board_mode, unused
  if(fgetc(fp) == EOF)
  {
    error_message(E_WORLD_BOARD_MISSING, board_location, NULL);
    return VAL_MISSING;
  }

  overlay_mode = fgetc(fp);

  if(!overlay_mode)
  {
    int overlay_width;
    int overlay_height;

    overlay_mode = fgetc(fp);
    overlay_width = fgetw(fp);
    overlay_height = fgetw(fp);

    size = overlay_width * overlay_height;

    if((size < 1) || (size > MAX_BOARD_SIZE))
      goto err_invalid;

    cur_board->overlay = cmalloc(size);
    cur_board->overlay_color = cmalloc(size);

    if(load_RLE2_plane(cur_board->overlay, fp, size))
      goto err_freeoverlay;

    // Skip sizes
    if(fgetd(fp) == EOF ||
     load_RLE2_plane(cur_board->overlay_color, fp, size))
      goto err_freeoverlay;
  }
  else
  {
    overlay_mode = 0;
    // Undo that last get
    fseek(fp, -1, SEEK_CUR);
  }

  cur_board->overlay_mode = overlay_mode;

  board_width = fgetw(fp);
  board_height = fgetw(fp);
  cur_board->board_width = board_width;
  cur_board->board_height = board_height;

  size = board_width * board_height;

  if((size < 1) || (size > MAX_BOARD_SIZE))
    goto err_freeoverlay;

  cur_board->level_id = cmalloc(size);
  cur_board->level_color = cmalloc(size);
  cur_board->level_param = cmalloc(size);
  cur_board->level_under_id = cmalloc(size);
  cur_board->level_under_color = cmalloc(size);
  cur_board->level_under_param = cmalloc(size);

  if(load_RLE2_plane(cur_board->level_id, fp, size))
    goto err_freeboard;

  if(fgetd(fp) == EOF ||
   load_RLE2_plane(cur_board->level_color, fp, size))
    goto err_freeboard;

  if(fgetd(fp) == EOF ||
   load_RLE2_plane(cur_board->level_param, fp, size))
    goto err_freeboard;

  if(fgetd(fp) == EOF ||
   load_RLE2_plane(cur_board->level_under_id, fp, size))
    goto err_freeboard;

  if(fgetd(fp) == EOF ||
   load_RLE2_plane(cur_board->level_under_color, fp, size))
    goto err_freeboard;

  if(fgetd(fp) == EOF ||
   load_RLE2_plane(cur_board->level_under_param, fp, size))
    goto err_freeboard;

  // Load board parameters

  if(file_version < V283)
  {
    if(!fread(cur_board->mod_playing, LEGACY_MOD_FILENAME_MAX, 1, fp))
      cur_board->mod_playing[0] = 0;

    cur_board->mod_playing[LEGACY_MOD_FILENAME_MAX] = 0;
  }
  else
  {
    size_t len = fgetw(fp);
    if(len >= MAX_PATH)
      len = MAX_PATH - 1;

    if(!fread(cur_board->mod_playing, len, 1, fp))
      len = 0;

    cur_board->mod_playing[len] = 0;
  }

  viewport_x = fgetc(fp);
  viewport_y = fgetc(fp);
  viewport_width = fgetc(fp);
  viewport_height = fgetc(fp);

  if(
   (viewport_x < 0) || (viewport_x > 79) ||
   (viewport_y < 0) || (viewport_y > 24) ||
   (viewport_width < 1) || (viewport_width > 80) ||
   (viewport_height < 1) || (viewport_height > 25))
    goto err_freeboard;

  cur_board->viewport_x = viewport_x;
  cur_board->viewport_y = viewport_y;
  cur_board->viewport_width = viewport_width;
  cur_board->viewport_height = viewport_height;
  cur_board->can_shoot = fgetc(fp);
  cur_board->can_bomb = fgetc(fp);
  cur_board->fire_burn_brown = fgetc(fp);
  cur_board->fire_burn_space = fgetc(fp);
  cur_board->fire_burn_fakes = fgetc(fp);
  cur_board->fire_burn_trees = fgetc(fp);
  cur_board->explosions_leave = fgetc(fp);
  cur_board->save_mode = fgetc(fp);
  cur_board->forest_becomes = fgetc(fp);
  cur_board->collect_bombs = fgetc(fp);
  cur_board->fire_burns = fgetc(fp);

  for(i = 0; i < 4; i++)
  {
    cur_board->board_dir[i] = fgetc(fp);
  }

  cur_board->restart_if_zapped = fgetc(fp);
  cur_board->time_limit = fgetw(fp);

  if(file_version < V283)
  {
    cur_board->last_key = fgetc(fp);
    cur_board->num_input = fgetw(fp);
    cur_board->input_size = fgetc(fp);

    if(!fread(cur_board->input_string, LEGACY_INPUT_STRING_MAX + 1, 1, fp))
      cur_board->input_string[0] = 0;
    cur_board->input_string[LEGACY_INPUT_STRING_MAX] = 0;

    cur_board->player_last_dir = fgetc(fp);

    if(!fread(cur_board->bottom_mesg, LEGACY_BOTTOM_MESG_MAX + 1, 1, fp))
      cur_board->bottom_mesg[0] = 0;
    cur_board->bottom_mesg[LEGACY_BOTTOM_MESG_MAX] = 0;

    cur_board->b_mesg_timer = fgetc(fp);
    cur_board->lazwall_start = fgetc(fp);
    cur_board->b_mesg_row = fgetc(fp);
    cur_board->b_mesg_col = (signed char)fgetc(fp);
    cur_board->scroll_x = (signed short)fgetw(fp);
    cur_board->scroll_y = (signed short)fgetw(fp);
    cur_board->locked_x = (signed short)fgetw(fp);
    cur_board->locked_y = (signed short)fgetw(fp);
  }
  else if(savegame)
  {
    size_t len;

    cur_board->last_key = fgetc(fp);
    cur_board->num_input = fgetw(fp);
    cur_board->input_size = fgetw(fp);

    len = fgetw(fp);
    if(len >= ROBOT_MAX_TR)
      len = ROBOT_MAX_TR - 1;

    if(!fread(cur_board->input_string, len, 1, fp))
      len = 0;
    cur_board->input_string[len] = 0;

    cur_board->player_last_dir = fgetc(fp);

    len = fgetw(fp);
    if(len >= ROBOT_MAX_TR)
      len = ROBOT_MAX_TR - 1;

    if(!fread(cur_board->bottom_mesg, len, 1, fp))
      len = 0;
    cur_board->bottom_mesg[len] = 0;

    cur_board->b_mesg_timer = fgetc(fp);
    cur_board->lazwall_start = fgetc(fp);
    cur_board->b_mesg_row = fgetc(fp);
    cur_board->b_mesg_col = (signed char)fgetc(fp);
    cur_board->scroll_x = (signed short)fgetw(fp);
    cur_board->scroll_y = (signed short)fgetw(fp);
    cur_board->locked_x = (signed short)fgetw(fp);
    cur_board->locked_y = (signed short)fgetw(fp);
  }

  cur_board->player_ns_locked = fgetc(fp);
  cur_board->player_ew_locked = fgetc(fp);
  cur_board->player_attack_locked = fgetc(fp);

  if(file_version < V283 || savegame)
  {
    cur_board->volume = fgetc(fp);
    cur_board->volume_inc = fgetc(fp);
    cur_board->volume_target = fgetc(fp);
  }


  /***************/
  /* Load robots */
  /***************/
  num_robots = fgetc(fp);
  num_robots_active = 0;

  if(num_robots == EOF)
    truncated = true;

  // EOF/crazy value check
  if((num_robots < 0) || (num_robots > 255) || (num_robots > size))
    goto board_scan;

  cur_board->robot_list = ccalloc(num_robots + 1, sizeof(struct robot *));
  // Also allocate for name sorted list
  cur_board->robot_list_name_sorted =
   ccalloc(num_robots, sizeof(struct robot *));

  // Any null objects being placed will later be optimized out
  set_error_suppression(E_WORLD_ROBOT_MISSING, 0);

  if(num_robots)
  {
    for(i = 1; i <= num_robots; i++)
    {
      // Make sure there's robots to load here
      if(truncated)
        break;

      cur_robot = legacy_load_robot_allocate(mzx_world, fp, savegame,
       file_version, &truncated);

      if(cur_robot->used)
      {
        cur_board->robot_list[i] = cur_robot;
        cur_board->robot_list_name_sorted[num_robots_active] = cur_robot;
        num_robots_active++;
      }
      else
      {
        // We don't need no null robot
        clear_robot(cur_robot);
        cur_board->robot_list[i] = NULL;
      }
    }
  }

  if(num_robots_active > 0)
  {
    if(num_robots_active != num_robots)
    {
      cur_board->robot_list_name_sorted =
       crealloc(cur_board->robot_list_name_sorted,
       sizeof(struct robot *) * num_robots_active);
    }
    qsort(cur_board->robot_list_name_sorted, num_robots_active,
     sizeof(struct robot *), cmp_robots);
  }
  else
  {
    free(cur_board->robot_list_name_sorted);
    cur_board->robot_list_name_sorted = NULL;
  }

  cur_board->num_robots = num_robots;
  cur_board->num_robots_allocated = num_robots;
  cur_board->num_robots_active = num_robots_active;


  /****************/
  /* Load scrolls */
  /****************/
  num_scrolls = fgetc(fp);

  if(num_scrolls == EOF)
    truncated = 1;

  if((num_scrolls < 0) || (num_scrolls > 255) || (num_robots + num_scrolls > size))
    goto board_scan;

  cur_board->scroll_list = ccalloc(num_scrolls + 1, sizeof(struct scroll *));

  if(num_scrolls)
  {
    for(i = 1; i <= num_scrolls; i++)
    {
      cur_scroll = legacy_load_scroll_allocate(fp);
      if(cur_scroll->used)
        cur_board->scroll_list[i] = cur_scroll;
      else
        clear_scroll(cur_scroll);
    }
  }

  cur_board->num_scrolls = num_scrolls;
  cur_board->num_scrolls_allocated = num_scrolls;


  /****************/
  /* Load sensors */
  /****************/
  num_sensors = fgetc(fp);

  if(num_sensors == EOF)
    truncated = 1;

  if((num_sensors < 0) || (num_sensors > 255) ||
   (num_scrolls + num_sensors + num_robots > size))
    goto board_scan;

  cur_board->sensor_list = ccalloc(num_sensors + 1, sizeof(struct sensor *));

  if(num_sensors)
  {
    for(i = 1; i <= num_sensors; i++)
    {
      cur_sensor = legacy_load_sensor_allocate(fp);
      if(cur_sensor->used)
        cur_board->sensor_list[i] = cur_sensor;
      else
        clear_sensor(cur_sensor);
    }
  }

  cur_board->num_sensors = num_sensors;
  cur_board->num_sensors_allocated = num_sensors;


board_scan:
  // Now do a board scan to make sure there aren't more than the data told us.
  {
    char *level_id = cur_board->level_id;
    char *level_param = cur_board->level_param;
    char *level_under_id = cur_board->level_under_id;
    struct robot **robot_list = cur_board->robot_list;

    int robot_count = 0, scroll_count = 0, sensor_count = 0;
    char err_mesg[80] = { 0 };
    unsigned char id;
    unsigned char pr;

    char found_robots[256] = {0};

    for(i = 0; i < (board_width * board_height); i++)
    {
      id = level_id[i];
      pr = level_param[i];

      if(level_under_id[i] > 127)
        level_under_id[i] = CUSTOM_FLOOR;

      switch(id)
      {
        case ROBOT:
        case ROBOT_PUSHABLE:
        {
          robot_count++;
          if(!found_robots[pr] && pr <= num_robots && robot_list[pr])
          {
            found_robots[pr] = 1;
            // Also fix the xpos/ypos values, which may have been saved
            // incorrectly in ver1to2 worlds (see: Caverns of Zeux).
            cur_robot = robot_list[pr];
            cur_robot->xpos = i % board_width;
            cur_robot->ypos = i / board_width;
            cur_robot->compat_xpos = cur_robot->xpos;
            cur_robot->compat_ypos = cur_robot->ypos;
          }

          else
          {
            cur_board->level_id[i] = CUSTOM_BLOCK;
            cur_board->level_param[i] = 'R';
            cur_board->level_color[i] = 0xCF;
          }
          break;
        }
        case SIGN:
        case SCROLL:
        {
          scroll_count++;
          if(scroll_count > cur_board->num_scrolls)
          {
            cur_board->level_id[i] = CUSTOM_BLOCK;
            cur_board->level_param[i] = 'S';
            cur_board->level_color[i] = 0xCF;
          }
          break;
        }
        case SENSOR:
        {
          // Wait, I forgot.  Nobody cares about sensors.
          //sensor_count++;
          if(sensor_count > cur_board->num_sensors)
          {
            cur_board->level_id[i] = CUSTOM_FLOOR;
            cur_board->level_param[i] = 'S';
            cur_board->level_color[i] = 0xDF;
          }
          break;
        }
        default:
        {
          if(id > 127)
            level_id[i] = CUSTOM_BLOCK;
          break;
        }
      }
    }

    // Perform a robot scan to make sure every robot is actually on the board.
    // Some old worlds (e.g. Catacombs of Zeux, Demon Earth) contain useless
    // robots that aren't. Silently mark the missing ones to not be saved.
    for(i = 1; i <= num_robots; i++)
    {
      cur_robot = robot_list[i];

      if(cur_robot && cur_robot->used)
      {
        if(!found_robots[i])
        {
          debug("Robot # %d was loaded but is missing on board @ %d; marked unused\n",
           i, board_location);

          cur_robot->used = 0;
        }
      }
    }

    if(robot_count > cur_board->num_robots)
    {
      snprintf(err_mesg, 80, "found %i robots; expected %i",
       robot_count, cur_board->num_robots);

      error_message(E_BOARD_SUMMARY, board_location, err_mesg);
    }
    if(scroll_count > cur_board->num_scrolls)
    {
      snprintf(err_mesg, 80, "found %i scrolls/signs; expected %i",
       scroll_count, cur_board->num_scrolls);

      error_message(E_BOARD_SUMMARY, board_location, err_mesg);
    }
    // This won't be reached but I'll leave it anyway.
    if(sensor_count > cur_board->num_sensors)
    {
      snprintf(err_mesg, 80, "found %i sensors; expected %i",
       sensor_count, cur_board->num_sensors);

      error_message(E_BOARD_SUMMARY, board_location, err_mesg);
    }
    if(err_mesg[0])
    {
      error_message(E_BOARD_SUMMARY, board_location,
       "Any extra robots/scrolls/signs were replaced");
    }

  }

  if(truncated == 1)
    error_message(E_WORLD_BOARD_TRUNCATED_SAFE, board_location, NULL);

  return VAL_SUCCESS;

err_freeboard:
  free(cur_board->level_id);
  free(cur_board->level_color);
  free(cur_board->level_param);
  free(cur_board->level_under_id);
  free(cur_board->level_under_color);
  free(cur_board->level_under_param);

err_freeoverlay:
  if(overlay_mode)
  {
    free(cur_board->overlay);
    free(cur_board->overlay_color);
  }

err_invalid:
  error_message(E_WORLD_BOARD_CORRUPT, board_location, NULL);
  return VAL_INVALID;
}


struct board *legacy_load_board_allocate(struct world *mzx_world, FILE *fp,
 int data_offset, int data_size, int savegame, int file_version)
{
  struct board *cur_board;
  enum val_result result;

  // Skip deleted boards
  if(!data_size)
    return NULL;

  // Should generally be at this position after reading the previous
  // board, but don't count on that being true...
  if(ftell(fp) != data_offset)
  {
    if(fseek(fp, data_offset, SEEK_SET))
    {
      error_message(E_WORLD_BOARD_MISSING, data_offset, NULL);
      return NULL;
    }
  }

  cur_board = cmalloc(sizeof(struct board));
  cur_board->world_version = mzx_world->version;
  result = legacy_load_board_direct(mzx_world, cur_board, fp, data_size,
   savegame, file_version);

  if(result != VAL_SUCCESS)
    dummy_board(cur_board);

  return cur_board;
}
