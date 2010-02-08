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

static int cmp_robots(const void *dest, const void *src)
{
  Robot *rsrc = *((Robot **)src);
  Robot *rdest = *((Robot **)dest);
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

static void load_board_direct(Board *cur_board, FILE *fp, int savegame)
{
  int num_robots, num_scrolls, num_sensors, num_robots_active;
  int overlay_mode, board_mode;
  int size, board_width, board_height;
  int i;
  Robot *cur_robot;
  Scroll *cur_scroll;
  Sensor *cur_sensor;

  char *test_buffer;

  board_mode = fgetc(fp);

  overlay_mode = fgetc(fp);

  if(!overlay_mode)
  {
    int overlay_width;
    int overlay_height;

    overlay_mode = fgetc(fp);
    overlay_width = fgetw(fp);
    overlay_height = fgetw(fp);

    size = overlay_width * overlay_height;

    cur_board->overlay = malloc(size);
    cur_board->overlay_color = malloc(size);

    load_RLE2_plane(cur_board->overlay, fp, size);
    test_buffer = malloc(1024);
    free(test_buffer);

    // Skip sizes
    fseek(fp, 4, SEEK_CUR);
    load_RLE2_plane(cur_board->overlay_color, fp, size);
    test_buffer = malloc(1024);
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

  cur_board->level_id = malloc(size);
  cur_board->level_color = malloc(size);
  cur_board->level_param = malloc(size);
  cur_board->level_under_id = malloc(size);
  cur_board->level_under_color = malloc(size);
  cur_board->level_under_param = malloc(size);

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
  fread(cur_board->mod_playing, FILENAME_SIZE, 1, fp);
  cur_board->mod_playing[FILENAME_SIZE] = 0;
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
  cur_board->last_key = fgetc(fp);
  cur_board->num_input = fgetw(fp);
  cur_board->input_size = fgetc(fp);
  fread(cur_board->input_string, 81, 1, fp);
  cur_board->player_last_dir = fgetc(fp);
  fread(cur_board->bottom_mesg, 81, 1, fp);
  cur_board->b_mesg_timer = fgetc(fp);
  cur_board->lazwall_start = fgetc(fp);
  cur_board->b_mesg_row = fgetc(fp);
  cur_board->b_mesg_col = (signed char)fgetc(fp);
  cur_board->scroll_x = (signed short)fgetw(fp);
  cur_board->scroll_y = (signed short)fgetw(fp);
  cur_board->locked_x = (signed short)fgetw(fp);
  cur_board->locked_y = (signed short)fgetw(fp);
  cur_board->player_ns_locked = fgetc(fp);
  cur_board->player_ew_locked = fgetc(fp);
  cur_board->player_attack_locked = fgetc(fp);
  cur_board->volume = fgetc(fp);
  cur_board->volume_inc = fgetc(fp);
  cur_board->volume_target = fgetc(fp);

  // Load robots
  num_robots = fgetc(fp);
  num_robots_active = 0;

  cur_board->robot_list = calloc(num_robots + 1, sizeof(Robot *));
  // Also allocate for name sorted list
  cur_board->robot_list_name_sorted = calloc(num_robots, sizeof(Robot *));

  // Any null objects being placed will later be optimized out

  if(num_robots)
  {
    for(i = 1; i <= num_robots; i++)
    {
      cur_robot = load_robot_allocate(fp, savegame);
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
       realloc(cur_board->robot_list_name_sorted,
       sizeof(Robot *) * num_robots_active);
    }
    qsort(cur_board->robot_list_name_sorted, num_robots_active,
     sizeof(Robot *), cmp_robots);
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
  cur_board->scroll_list = calloc(num_scrolls + 1, sizeof(Scroll *));

  if(num_scrolls)
  {
    for(i = 1; i <= num_scrolls; i++)
    {
      cur_scroll = load_scroll_allocate(fp, savegame);
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
  cur_board->sensor_list = calloc(num_sensors + 1, sizeof(Sensor *));

  if(num_sensors)
  {
    for(i = 1; i <= num_sensors; i++)
    {
      cur_sensor = load_sensor_allocate(fp, savegame);
      if(cur_sensor->used)
        cur_board->sensor_list[i] = cur_sensor;
      else
        clear_sensor(cur_sensor);
    }
  }

  cur_board->num_sensors = num_sensors;
  cur_board->num_sensors_allocated = num_sensors;
}

#ifdef CONFIG_EDITOR

static Board *load_board_allocate_direct(FILE *fp, int savegame)
{
  Board *cur_board = malloc(sizeof(Board));
  load_board_direct(cur_board, fp, savegame);
  fread(cur_board->board_name, 25, 1, fp);
  return cur_board;
}

#endif // CONFIG_EDITOR

// The file given should point to a name/location combo. This will
// restore the file position to after the name/location.

static void load_board(Board *cur_board, FILE *fp, int savegame)
{
  int board_size = fgetd(fp);

  if(board_size)
  {
    int board_location, last_location;

    board_location = fgetd(fp);
    last_location = ftell(fp);

    fseek(fp, board_location, SEEK_SET);
    load_board_direct(cur_board, fp, savegame);
    fseek(fp, last_location, SEEK_SET);
  }
  else
  {
    cur_board->board_width = 0;
    // And skip board location
    fseek(fp, 4, SEEK_CUR);
  }
}

Board *load_board_allocate(FILE *fp, int savegame)
{
  Board *cur_board = malloc(sizeof(Board));
  load_board(cur_board, fp, savegame);

  if(!cur_board->board_width)
  {
    free(cur_board);
    return NULL;
  }

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

int save_board(Board *cur_board, FILE *fp, int savegame)
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
  fwrite(cur_board->mod_playing, FILENAME_SIZE, 1, fp);
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
  fputc(cur_board->last_key, fp);
  fputw(cur_board->num_input, fp);
  fputc(cur_board->input_size, fp);
  fwrite(cur_board->input_string, 81, 1, fp);
  fputc(cur_board->player_last_dir, fp);
  fwrite(cur_board->bottom_mesg, 81, 1, fp);
  fputc(cur_board->b_mesg_timer, fp);
  fputc(cur_board->lazwall_start, fp);
  fputc(cur_board->b_mesg_row, fp);
  fputc(cur_board->b_mesg_col, fp);
  fputw(cur_board->scroll_x, fp);
  fputw(cur_board->scroll_y, fp);
  fputw(cur_board->locked_x, fp);
  fputw(cur_board->locked_y, fp);
  fputc(cur_board->player_ns_locked, fp);
  fputc(cur_board->player_ew_locked, fp);
  fputc(cur_board->player_attack_locked, fp);
  fputc(cur_board->volume, fp);
  fputc(cur_board->volume_inc, fp);
  fputc(cur_board->volume_target, fp);

  // Save robots
  num_robots = cur_board->num_robots;
  fputc(num_robots, fp);

  if(num_robots)
  {
    Robot *cur_robot;

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
    Scroll *cur_scroll;

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
    Sensor *cur_sensor;

    for(i = 1; i <= num_sensors; i++)
    {
      cur_sensor = cur_board->sensor_list[i];
      save_sensor(cur_sensor, fp, savegame);
    }
  }

  return (ftell(fp) - start_location);
}

void clear_board(Board *cur_board)
{
  int i;
  int num_robots_active = cur_board->num_robots_active;
  int num_scrolls = cur_board->num_scrolls;
  int num_sensors = cur_board->num_sensors;
  Robot **robot_list = cur_board->robot_list;
  Robot **robot_name_list = cur_board->robot_list_name_sorted;
  Robot *cur_robot;
  Scroll **scroll_list = cur_board->scroll_list;
  Sensor **sensor_list = cur_board->sensor_list;

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

  if(robot_name_list)
    free(robot_name_list);

  if(robot_list)
    free(robot_list);

  for(i = 1; i <= num_scrolls; i++)
  {
    if(scroll_list[i])
      clear_scroll(scroll_list[i]);
  }

  if(scroll_list)
    free(scroll_list);

  for(i = 1; i <= num_sensors; i++)
  {
    if(sensor_list[i])
      clear_sensor(sensor_list[i]);
  }

  if(sensor_list)
    free(sensor_list);

  free(cur_board);
}

// Just a linear search. Boards aren't addressed by name very often.
int find_board(World *mzx_world, char *name)
{
  Board **board_list = mzx_world->board_list;
  Board *current_board;
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

#ifdef CONFIG_EDITOR

void replace_current_board(World *mzx_world, char *name)
{
  Board *src_board = mzx_world->current_board;
  FILE *input_mzb = fopen(name, "rb");
  int first_byte = fgetc(input_mzb);
  char version_string[4];
  int current_board_id = mzx_world->current_board_id;

  fread(version_string, 3, 1, input_mzb);
  version_string[3] = 0;

  if((first_byte == 0xFF) &&
   ((strcmp(version_string, world_version_string) <= 0) ||
   !strcmp(version_string, "MB2")))
  {
    clear_board(src_board);
    src_board = load_board_allocate_direct(input_mzb, 0);
    optimize_null_objects(src_board);

    set_update_done_current(mzx_world);

    if(src_board->robot_list)
      src_board->robot_list[0] = mzx_world->global_robot;

    mzx_world->current_board = src_board;
    mzx_world->board_list[current_board_id] = src_board;
  }

  fclose(input_mzb);
}

Board *create_blank_board(void)
{
  Board *cur_board = malloc(sizeof(Board));
  int i;

  cur_board->size = 0;
  cur_board->board_name[0] = 0;
  cur_board->board_width = 100;
  cur_board->board_height = 100;
  cur_board->overlay_mode = 1;
  cur_board->level_id = malloc(100 * 100);
  cur_board->level_param = malloc(100 * 100);
  cur_board->level_color = malloc(100 * 100);
  cur_board->level_under_id = malloc(100 * 100);
  cur_board->level_under_param = malloc(100 * 100);
  cur_board->level_under_color = malloc(100 * 100);
  cur_board->overlay = malloc(100 * 100);
  cur_board->overlay_color = malloc(100 * 100);
  cur_board->mod_playing[0] = 0;
  cur_board->viewport_x = 0;
  cur_board->viewport_y = 0;
  cur_board->viewport_width = 80;
  cur_board->viewport_height = 25;
  cur_board->can_shoot = 1;
  cur_board->can_bomb = 1;
  cur_board->fire_burn_brown = 0;
  cur_board->fire_burn_space = 1;
  cur_board->fire_burn_fakes = 1;
  cur_board->fire_burn_trees = 1;
  cur_board->explosions_leave = EXPL_LEAVE_ASH;
  cur_board->save_mode = CAN_SAVE;
  cur_board->forest_becomes = FOREST_TO_FLOOR;
  cur_board->collect_bombs = 1;
  cur_board->fire_burns = FIRE_BURNS_FOREVER;

  for(i = 0; i < 4; i++)
  {
    cur_board->board_dir[i] = NO_BOARD;
  }

  cur_board->restart_if_zapped = 0;
  cur_board->time_limit = 0;
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
  cur_board->player_ns_locked = 0;
  cur_board->player_ew_locked = 0;
  cur_board->player_attack_locked = 0;
  cur_board->volume = 255;
  cur_board->volume_inc = 0;
  cur_board->volume_target = 255;

  cur_board->num_robots = 0;
  cur_board->num_robots_active = 0;
  cur_board->num_robots_allocated = 0;
  cur_board->robot_list = malloc(sizeof(Robot *));
  cur_board->robot_list_name_sorted = NULL;
  cur_board->num_scrolls = 0;
  cur_board->num_scrolls_allocated = 0;
  cur_board->scroll_list = malloc(sizeof(Scroll *));
  cur_board->num_sensors = 0;
  cur_board->num_sensors_allocated = 0;
  cur_board->sensor_list = malloc(sizeof(Sensor *));

  memset(cur_board->level_id, 0, 100 * 100);
  memset(cur_board->level_color, 7, 100 * 100);
  memset(cur_board->level_param, 0, 100 * 100);
  memset(cur_board->level_under_id, 0, 100 * 100);
  memset(cur_board->level_under_color, 7, 100 * 100);
  memset(cur_board->level_under_param, 0, 100 * 100);
  memset(cur_board->overlay, 32, 100 * 100);
  memset(cur_board->overlay_color, 7, 100 * 100);

  cur_board->level_id[0] = 127;

  return cur_board;
}

void save_board_file(Board *cur_board, char *name)
{
  FILE *board_file = fopen(name, "wb");

  if(board_file)
  {
    fputc(0xFF, board_file);
    fputs(world_version_string, board_file);
    optimize_null_objects(cur_board);
    save_board(cur_board, board_file, 0);
    // Write name
    fwrite(cur_board->board_name, 25, 1, board_file);
    fclose(board_file);
  }
}

void change_board_size(Board *src_board, int new_width, int new_height)
{
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;

  if(new_width == 0)
    new_width = 1;

  if(new_height == 0)
    new_height = 1;

  if((board_width != new_width) || (board_height != new_height))
  {
    char *level_id = src_board->level_id;
    char *level_color = src_board->level_color;
    char *level_param = src_board->level_param;
    char *level_under_id = src_board->level_under_id;
    char *level_under_color = src_board->level_under_color;
    char *level_under_param = src_board->level_under_param;
    char *overlay = src_board->overlay;
    char *overlay_color = src_board->overlay_color;
    int overlay_mode = src_board->overlay_mode;
    int board_size = board_width * board_height;
    int new_size = new_width * new_height;
    int i;

    // Shrinking height? Remove any robots/scrolls/sensors from excess lines
    if(new_height < board_height)
    {
      int offset;
      int check_id;

      for(offset = new_height * board_width; offset < board_size;
       offset++)
      {
        check_id = level_id[offset];

        if(check_id == 122)
          clear_sensor_id(src_board, level_param[offset]);
        else

        if((check_id == 126) || (check_id == 125))
          clear_scroll_id(src_board, level_param[offset]);
        else

        if((check_id == 123) || (check_id == 124))
          clear_robot_id(src_board, level_param[offset]);
      }
    }

    if(new_width < board_width)
    {
      // Take out pieces from each width, then reallocate.
      // Only do this for the current height or the new height, whichever is
      // less.

      int src_offset, dest_offset;
      int max_height = new_height;
      int check_id, offset;

      if(new_height > board_height)
        max_height = board_height;

      for(i = 0, src_offset = 0, dest_offset = 0; i < max_height;
       i++, src_offset += board_width, dest_offset += new_width)
      {
        // First, remove any robots/scrolls/sensors from excess chunks
        for(offset = src_offset + new_width; offset < src_offset + board_width;
          offset++)
        {
          check_id = level_id[offset];

          if(check_id == 122)
            clear_sensor_id(src_board, level_param[offset]);
          else

          if((check_id == 126) || (check_id == 125))
            clear_scroll_id(src_board, level_param[offset]);
          else

          if((check_id == 123) || (check_id == 124))
            clear_robot_id(src_board, level_param[offset]);
        }

        memmove(level_id + dest_offset, level_id + src_offset, new_width);
        memmove(level_param + dest_offset, level_param + src_offset, new_width);
        memmove(level_color + dest_offset, level_color + src_offset, new_width);
        memmove(level_under_id + dest_offset,
         level_under_id + src_offset, new_width);
        memmove(level_under_param + dest_offset,
         level_under_param + src_offset, new_width);
        memmove(level_under_color + dest_offset,
         level_under_color + src_offset, new_width);
      }

      // Resize layers
      src_board->level_id = realloc(level_id, new_size);
      src_board->level_color = realloc(level_color, new_size);
      src_board->level_param = realloc(level_param, new_size);
      src_board->level_under_id = realloc(level_under_id, new_size);
      src_board->level_under_color = realloc(level_under_color, new_size);
      src_board->level_under_param = realloc(level_under_param, new_size);

      level_id = src_board->level_id;
      level_color = src_board->level_color;
      level_param = src_board->level_param;
      level_under_id = src_board->level_under_id;
      level_under_color = src_board->level_under_color;
      level_under_param = src_board->level_under_param;

      // Do the overlay too, if it exists
      if(overlay_mode)
      {
        for(i = 0, src_offset = 0, dest_offset = 0; i < max_height;
         i++, src_offset += board_width, dest_offset += new_width)
        {
          memmove(overlay + dest_offset, overlay + src_offset, new_width);
          memmove(overlay_color + dest_offset, overlay_color + src_offset, new_width);
        }

        src_board->overlay = realloc(overlay, new_size);
        src_board->overlay_color = realloc(overlay_color, new_size);

        overlay = src_board->overlay;
        overlay_color = src_board->overlay_color;
      }
    }
    else

    if(new_width > board_width)
    {
      // Reallocate first, then copy over pieces, padding in widths
      // Only do this for the current height or the new height, whichever is
      // less.
      int src_offset, dest_offset;
      int max_height = new_height;
      int width_difference = new_width - board_width;

      if(new_height > board_height)
        max_height = board_height;

      // Resize first this time.
      src_board->level_id = realloc(level_id, new_size);
      src_board->level_color = realloc(level_color, new_size);
      src_board->level_param = realloc(level_param, new_size);
      src_board->level_under_id = realloc(level_under_id, new_size);
      src_board->level_under_color = realloc(level_under_color, new_size);
      src_board->level_under_param = realloc(level_under_param, new_size);

      // Resize the overlay too, if it exists
      if(overlay_mode)
      {
        src_board->overlay = realloc(overlay, new_size);
        src_board->overlay_color = realloc(overlay_color, new_size);
        overlay = src_board->overlay;
        overlay_color = src_board->overlay_color;
      }

      level_id = src_board->level_id;
      level_color = src_board->level_color;
      level_param = src_board->level_param;
      level_under_id = src_board->level_under_id;
      level_under_color = src_board->level_under_color;
      level_under_param = src_board->level_under_param;

      // And start at the end instead of the beginning
      src_offset = board_width * (max_height - 1);
      dest_offset = new_width * (max_height - 1);

      for(i = 0; i < max_height; i++, src_offset -= board_width, dest_offset -= new_width)
      {
        memmove(level_id + dest_offset, level_id + src_offset, board_width);
        memmove(level_param + dest_offset, level_param + src_offset, board_width);
        memmove(level_color + dest_offset, level_color + src_offset, board_width);
        memmove(level_under_id + dest_offset,
         level_under_id + src_offset, board_width);
        memmove(level_under_param + dest_offset,
         level_under_param + src_offset, board_width);
        memmove(level_under_color + dest_offset,
         level_under_color + src_offset, board_width);
        // And fill in the remainder with blanks
        memset(level_id + dest_offset + board_width, 0, width_difference);
        memset(level_param + dest_offset + board_width, 0, width_difference);
        memset(level_color + dest_offset + board_width, 7, width_difference);
        memset(level_under_id + dest_offset + board_width, 0, width_difference);
        memset(level_under_param + dest_offset + board_width, 0, width_difference);
        memset(level_under_color + dest_offset + board_width, 7, width_difference);
      }

      src_offset = board_width * (max_height - 1);
      dest_offset = new_width * (max_height - 1);

      // Do the overlay too, if it exists
      if(overlay_mode)
      {
        for(i = 0; i < max_height; i++, src_offset -= board_width, dest_offset -= new_width)
        {
          memmove(overlay + dest_offset, overlay + src_offset, board_width);
          memmove(overlay_color + dest_offset, overlay_color + src_offset, board_width);
          // And fill in the remainder with blanks
          memset(overlay + dest_offset + board_width, 32, width_difference);
          memset(overlay_color + dest_offset + board_width, 7, width_difference);
        }
      }
    }
    else
    {
      // Width remains the same, just a straight resize

      src_board->level_id = realloc(level_id, new_size);
      src_board->level_color = realloc(level_color, new_size);
      src_board->level_param = realloc(level_param, new_size);
      src_board->level_under_id = realloc(level_under_id, new_size);
      src_board->level_under_color = realloc(level_under_color, new_size);
      src_board->level_under_param = realloc(level_under_param, new_size);

      // Resize the overlay too, if it exists
      if(overlay_mode)
      {
        src_board->overlay = realloc(overlay, new_size);
        src_board->overlay_color = realloc(overlay_color, new_size);
      }

      level_id = src_board->level_id;
      level_color = src_board->level_color;
      level_param = src_board->level_param;
      level_under_id = src_board->level_under_id;
      level_under_color = src_board->level_under_color;
      level_under_param = src_board->level_under_param;
      overlay = src_board->overlay;
      overlay_color = src_board->overlay_color;
    }

    // Fill in any blanks
    if(new_height > board_height)
    {
      int offset = new_width * board_height;
      int size_difference = new_size - offset;

      memset(level_id + offset, 0, size_difference);
      memset(level_param + offset, 0, size_difference);
      memset(level_color + offset, 7, size_difference);
      memset(level_under_id + offset, 0, size_difference);
      memset(level_under_param + offset, 0, size_difference);
      memset(level_under_color + offset, 7, size_difference);

      if(overlay_mode)
      {
        memset(overlay + offset, 32, size_difference);
        memset(overlay_color + offset, 7, size_difference);
      }
    }

    // Set the new sizes
    src_board->board_width = new_width;
    src_board->board_height = new_height;
  }
}

#endif // CONFIG_EDITOR
