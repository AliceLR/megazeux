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
#include "world.h"
#include "const.h"
#include "extmem.h"
#include "util.h"

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

static void load_RLE2_plane(char *plane, FILE *fp, int size)
{
  int i, runsize;
  char current_char;

  for(i = 0; i < size; i++)
  {
    current_char = fgetc(fp);
    if(!(current_char & 0x80))
    {
      // Regular character
      plane[i] = current_char;
    }
    else
    {
      // A run
      runsize = current_char & 0x7F;
      current_char = fgetc(fp);
      memset(plane + i, current_char, runsize);
      i += (runsize - 1);
    }
  }
}

__editor_maybe_static void load_board_direct(struct board *cur_board,
 FILE *fp, int savegame, int version)
{
  int num_robots, num_scrolls, num_sensors, num_robots_active;
  int overlay_mode, size, board_width, board_height, i;

  struct robot *cur_robot;
  struct scroll *cur_scroll;
  struct sensor *cur_sensor;

  char *test_buffer;

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

  // board_mode, unused
  fgetc(fp);

  overlay_mode = fgetc(fp);

  if(!overlay_mode)
  {
    int overlay_width;
    int overlay_height;

    overlay_mode = fgetc(fp);
    overlay_width = fgetw(fp);
    overlay_height = fgetw(fp);

    size = overlay_width * overlay_height;

    cur_board->overlay = cmalloc(size);
    cur_board->overlay_color = cmalloc(size);

    load_RLE2_plane(cur_board->overlay, fp, size);
    test_buffer = cmalloc(1024);
    free(test_buffer);

    // Skip sizes
    fseek(fp, 4, SEEK_CUR);
    load_RLE2_plane(cur_board->overlay_color, fp, size);
    test_buffer = cmalloc(1024);
    free(test_buffer);
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

  cur_board->level_id = cmalloc(size);
  cur_board->level_color = cmalloc(size);
  cur_board->level_param = cmalloc(size);
  cur_board->level_under_id = cmalloc(size);
  cur_board->level_under_color = cmalloc(size);
  cur_board->level_under_param = cmalloc(size);

  load_RLE2_plane(cur_board->level_id, fp, size);
  fseek(fp, 4, SEEK_CUR);
  load_RLE2_plane(cur_board->level_color, fp, size);
  fseek(fp, 4, SEEK_CUR);
  load_RLE2_plane(cur_board->level_param, fp, size);
  fseek(fp, 4, SEEK_CUR);
  load_RLE2_plane(cur_board->level_under_id, fp, size);
  fseek(fp, 4, SEEK_CUR);
  load_RLE2_plane(cur_board->level_under_color, fp, size);
  fseek(fp, 4, SEEK_CUR);
  load_RLE2_plane(cur_board->level_under_param, fp, size);

  // Load board parameters

  if(version < 0x0253)
  {
    fread(cur_board->mod_playing, LEGACY_MOD_FILENAME_MAX, 1, fp);
    cur_board->mod_playing[LEGACY_MOD_FILENAME_MAX] = 0;
  }
  else
  {
    size_t len = fgetw(fp);
    if(len >= MAX_PATH)
      len = MAX_PATH - 1;

    fread(cur_board->mod_playing, len, 1, fp);
    cur_board->mod_playing[len] = 0;
  }

  cur_board->viewport_x = fgetc(fp);
  cur_board->viewport_y = fgetc(fp);
  cur_board->viewport_width = fgetc(fp);
  cur_board->viewport_height = fgetc(fp);
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

  if(version < 0x0253)
  {
    cur_board->last_key = fgetc(fp);
    cur_board->num_input = fgetw(fp);
    cur_board->input_size = fgetc(fp);

    fread(cur_board->input_string, LEGACY_INPUT_STRING_MAX + 1, 1, fp);
    cur_board->input_string[LEGACY_INPUT_STRING_MAX] = 0;

    cur_board->player_last_dir = fgetc(fp);

    fread(cur_board->bottom_mesg, LEGACY_BOTTOM_MESG_MAX + 1, 1, fp);
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

    fread(cur_board->input_string, len, 1, fp);
    cur_board->input_string[len] = 0;

    cur_board->player_last_dir = fgetc(fp);

    len = fgetw(fp);
    if(len >= ROBOT_MAX_TR)
      len = ROBOT_MAX_TR - 1;

    fread(cur_board->bottom_mesg, len, 1, fp);
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

  if(version < 0x0253 || savegame)
  {
    cur_board->volume = fgetc(fp);
    cur_board->volume_inc = fgetc(fp);
    cur_board->volume_target = fgetc(fp);
  }

  // Load robots
  num_robots = fgetc(fp);
  num_robots_active = 0;

  cur_board->robot_list = ccalloc(num_robots + 1, sizeof(struct robot *));
  // Also allocate for name sorted list
  cur_board->robot_list_name_sorted =
   ccalloc(num_robots, sizeof(struct robot *));

  // Any null objects being placed will later be optimized out

  if(num_robots)
  {
    for(i = 1; i <= num_robots; i++)
    {
      cur_robot = load_robot_allocate(fp, savegame, version);
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

  if(num_robots_active)
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

  // Load scrolls
  num_scrolls = fgetc(fp);
  cur_board->scroll_list = ccalloc(num_scrolls + 1, sizeof(struct scroll *));

  if(num_scrolls)
  {
    for(i = 1; i <= num_scrolls; i++)
    {
      cur_scroll = load_scroll_allocate(fp);
      if(cur_scroll->used)
        cur_board->scroll_list[i] = cur_scroll;
      else
        clear_scroll(cur_scroll);
    }
  }

  cur_board->num_scrolls = num_scrolls;
  cur_board->num_scrolls_allocated = num_scrolls;

  // Load sensors
  num_sensors = fgetc(fp);
  cur_board->sensor_list = ccalloc(num_sensors + 1, sizeof(struct sensor *));

  if(num_sensors)
  {
    for(i = 1; i <= num_sensors; i++)
    {
      cur_sensor = load_sensor_allocate(fp);
      if(cur_sensor->used)
        cur_board->sensor_list[i] = cur_sensor;
      else
        clear_sensor(cur_sensor);
    }
  }

  cur_board->num_sensors = num_sensors;
  cur_board->num_sensors_allocated = num_sensors;
}

struct board *load_board_allocate(FILE *fp, int savegame, int version)
{
  struct board *cur_board = cmalloc(sizeof(struct board));
  int board_size, board_location, last_location;

  board_size = fgetd(fp);

  // Skip deleted boards
  if(!board_size)
  {
    fseek(fp, 4, SEEK_CUR);
    free(cur_board);
    return NULL;
  }

  board_location = fgetd(fp);
  last_location = ftell(fp);

  fseek(fp, board_location, SEEK_SET);
  load_board_direct(cur_board, fp, savegame, version);
  fseek(fp, last_location, SEEK_SET);

  return cur_board;
}

static void save_RLE2_plane(char *plane, FILE *fp, int size)
{
  int i, runsize;
  char current_char;

  for(i = 0; i < size; i++)
  {
    current_char = plane[i];
    runsize = 1;

    while((i < (size - 1)) && (plane[i + 1] == current_char) &&
     (runsize < 127))
    {
      i++;
      runsize++;
    }

    // Put the runsize if necessary
    if((runsize > 1) || current_char & 0x80)
    {
      fputc(runsize | 0x80, fp);
      // Put the run character
      fputc(current_char, fp);
    }
    else
    {
      fputc(current_char, fp);
    }
  }
}

int save_board(struct board *cur_board, FILE *fp, int savegame)
{
  int num_robots, num_scrolls, num_sensors;
  int start_location = ftell(fp);
  int board_width = cur_board->board_width;
  int board_height = cur_board->board_height;
  int board_size = board_width * board_height;
  int i;

  // Board mode is now ignored, put 0
  fputc(0, fp);
  // Put overlay mode

  if(cur_board->overlay_mode)
  {
    fputc(0, fp);
    fputc(cur_board->overlay_mode, fp);
    fputw(cur_board->board_width, fp);
    fputw(cur_board->board_height, fp);
    save_RLE2_plane(cur_board->overlay, fp, board_size);
    fputw(cur_board->board_width, fp);
    fputw(cur_board->board_height, fp);
    save_RLE2_plane(cur_board->overlay_color, fp, board_size);
  }

  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_id, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_color, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_param, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_under_id, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_under_color, fp, board_size);
  fputw(board_width, fp);
  fputw(board_height, fp);
  save_RLE2_plane(cur_board->level_under_param, fp, board_size);

  // Save board parameters

  {
    size_t len = strlen(cur_board->mod_playing);
    fputw((int)len, fp);
    if(len)
      fwrite(cur_board->mod_playing, len, 1, fp);
  }

  fputc(cur_board->viewport_x, fp);
  fputc(cur_board->viewport_y, fp);
  fputc(cur_board->viewport_width, fp);
  fputc(cur_board->viewport_height, fp);
  fputc(cur_board->can_shoot, fp);
  fputc(cur_board->can_bomb, fp);
  fputc(cur_board->fire_burn_brown, fp);
  fputc(cur_board->fire_burn_space, fp);
  fputc(cur_board->fire_burn_fakes, fp);
  fputc(cur_board->fire_burn_trees, fp);
  fputc(cur_board->explosions_leave, fp);
  fputc(cur_board->save_mode, fp);
  fputc(cur_board->forest_becomes, fp);
  fputc(cur_board->collect_bombs, fp);
  fputc(cur_board->fire_burns, fp);

  for(i = 0; i < 4; i++)
  {
    fputc(cur_board->board_dir[i], fp);
  }

  fputc(cur_board->restart_if_zapped, fp);
  fputw(cur_board->time_limit, fp);

  if(savegame)
  {
    size_t len;

    fputc(cur_board->last_key, fp);
    fputw(cur_board->num_input, fp);
    fputw((int)cur_board->input_size, fp);

    len = strlen(cur_board->input_string);
    fputw((int)len, fp);
    if(len)
      fwrite(cur_board->input_string, len, 1, fp);

    fputc(cur_board->player_last_dir, fp);

    len = strlen(cur_board->bottom_mesg);
    fputw((int)len, fp);
    if(len)
      fwrite(cur_board->bottom_mesg, len, 1, fp);

    fputc(cur_board->b_mesg_timer, fp);
    fputc(cur_board->lazwall_start, fp);
    fputc(cur_board->b_mesg_row, fp);
    fputc(cur_board->b_mesg_col, fp);
    fputw(cur_board->scroll_x, fp);
    fputw(cur_board->scroll_y, fp);
    fputw(cur_board->locked_x, fp);
    fputw(cur_board->locked_y, fp);
  }

  fputc(cur_board->player_ns_locked, fp);
  fputc(cur_board->player_ew_locked, fp);
  fputc(cur_board->player_attack_locked, fp);

  if(savegame)
  {
    fputc(cur_board->volume, fp);
    fputc(cur_board->volume_inc, fp);
    fputc(cur_board->volume_target, fp);
  }

  // Save robots
  num_robots = cur_board->num_robots;
  fputc(num_robots, fp);

  if(num_robots)
  {
    struct robot *cur_robot;

    for(i = 1; i <= num_robots; i++)
    {
      cur_robot = cur_board->robot_list[i];
      save_robot(cur_robot, fp, savegame);
    }
  }

  // Save scrolls
  num_scrolls = cur_board->num_scrolls;
  putc(num_scrolls, fp);

  if(num_scrolls)
  {
    struct scroll *cur_scroll;

    for(i = 1; i <= num_scrolls; i++)
    {
      cur_scroll = cur_board->scroll_list[i];
      save_scroll(cur_scroll, fp, savegame);
    }
  }

  // Save sensors
  num_sensors = cur_board->num_sensors;
  fputc(num_sensors, fp);

  if(num_sensors)
  {
    struct sensor *cur_sensor;

    for(i = 1; i <= num_sensors; i++)
    {
      cur_sensor = cur_board->sensor_list[i];
      save_sensor(cur_sensor, fp, savegame);
    }
  }

  return (ftell(fp) - start_location);
}

void clear_board(struct board *cur_board)
{
  int i;
  int num_robots_active = cur_board->num_robots_active;
  int num_scrolls = cur_board->num_scrolls;
  int num_sensors = cur_board->num_sensors;
  struct robot **robot_list = cur_board->robot_list;
  struct robot **robot_name_list = cur_board->robot_list_name_sorted;
  struct robot *cur_robot;
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
  {
    cur_robot = robot_name_list[i];
    clear_robot(cur_robot);
  }

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
