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

#include "board.h"

#include "../board.h"
#include "../extmem.h"
#include "../world.h"

#include "world.h"
#include "configure.h"

#include <string.h>

static int board_magic(const char magic_string[4])
{
  if(magic_string[0] == 0xFF)
  {
    if(magic_string[1] == 'M')
    {
      // MZX versions >= 2.00 && <= 2.51S1
      if(magic_string[2] == 'B' && magic_string[3] == '2')
        return 0x0200;

      // MZX versions >= 2.51S2 && <= 9.xx
      if((magic_string[2] > 1) && (magic_string[2] < 10))
        return ((int)magic_string[2] << 8) + (int)magic_string[3];
    }
  }

  // Not a recognized board magic
  return 0;
}

static struct board *load_board_allocate_direct(FILE *fp, int version)
{
  struct board *cur_board = cmalloc(sizeof(struct board));
  int board_start, board_end;

  board_start = ftell(fp);
  fseek(fp, 0, SEEK_END);
  board_end = ftell(fp);
  fseek(fp, board_start, SEEK_SET);

  cur_board->world_version = version;
  load_board_direct(cur_board, fp, (board_end - board_start), 0, version);
  fread(cur_board->board_name, 25, 1, fp);
  return cur_board;
}

void replace_current_board(struct world *mzx_world, char *name)
{
  int version, current_board_id = mzx_world->current_board_id;
  struct board *src_board = mzx_world->current_board;
  FILE *input_mzb = fopen_unsafe(name, "rb");
  char version_string[4];

  fread(version_string, 4, 1, input_mzb);
  version = board_magic(version_string);

  if(version > 0 && version <= WORLD_VERSION)
  {
    clear_board(src_board);
    src_board = load_board_allocate_direct(input_mzb, version);
    optimize_null_objects(src_board);

    set_update_done_current(mzx_world);

    if(src_board->robot_list)
      src_board->robot_list[0] = &mzx_world->global_robot;

    set_current_board(mzx_world, src_board);
    mzx_world->board_list[current_board_id] = src_board;
  }

  fclose(input_mzb);
}

struct board *create_blank_board(struct editor_config_info *conf)
{
  struct board *cur_board = cmalloc(sizeof(struct board));
  int layer_size = conf->board_width * conf->board_height;
  int i;

  cur_board->size = 0;
  cur_board->board_name[0] = 0;
  cur_board->board_width =       conf->board_width;
  cur_board->board_height =      conf->board_height;
  cur_board->overlay_mode =      conf->overlay_enabled;
  cur_board->level_id =          cmalloc(layer_size);
  cur_board->level_param =       cmalloc(layer_size);
  cur_board->level_color =       cmalloc(layer_size);
  cur_board->level_under_id =    cmalloc(layer_size);
  cur_board->level_under_param = cmalloc(layer_size);
  cur_board->level_under_color = cmalloc(layer_size);
  if(cur_board->overlay_mode)
  {
    cur_board->overlay =         cmalloc(layer_size);
    cur_board->overlay_color =   cmalloc(layer_size);
  }
  cur_board->mod_playing[0] = 0;

  cur_board->viewport_x =        conf->viewport_x;
  cur_board->viewport_y =        conf->viewport_y;
  cur_board->viewport_width =    conf->viewport_w;
  cur_board->viewport_height =   conf->viewport_h;
  cur_board->can_shoot =         conf->can_shoot;
  cur_board->can_bomb =          conf->can_bomb;
  cur_board->fire_burn_brown =   conf->fire_burns_brown;
  cur_board->fire_burn_space =   conf->fire_burns_spaces;
  cur_board->fire_burn_fakes =   conf->fire_burns_fakes;
  cur_board->fire_burn_trees =   conf->fire_burns_trees;
  cur_board->explosions_leave =  conf->explosions_leave;
  cur_board->save_mode =         conf->saving_enabled;
  cur_board->forest_becomes =    conf->forest_to_floor;
  cur_board->collect_bombs =     conf->collect_bombs;
  cur_board->fire_burns =        conf->fire_burns_forever;

  for(i = 0; i < 4; i++)
  {
    cur_board->board_dir[i] = NO_BOARD;
  }

  cur_board->restart_if_zapped = conf->restart_if_hurt;
  cur_board->time_limit =        conf->time_limit;
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
  cur_board->player_ns_locked =     conf->player_locked_ns;
  cur_board->player_ew_locked =     conf->player_locked_ew;
  cur_board->player_attack_locked = conf->player_locked_att;
  cur_board->volume = 255;
  cur_board->volume_inc = 0;
  cur_board->volume_target = 255;

  cur_board->num_robots = 0;
  cur_board->num_robots_active = 0;
  cur_board->num_robots_allocated = 0;
  cur_board->robot_list = cmalloc(sizeof(struct robot *));
  cur_board->robot_list_name_sorted = NULL;
  cur_board->num_scrolls = 0;
  cur_board->num_scrolls_allocated = 0;
  cur_board->scroll_list = cmalloc(sizeof(struct scroll *));
  cur_board->num_sensors = 0;
  cur_board->num_sensors_allocated = 0;
  cur_board->sensor_list = cmalloc(sizeof(struct sensor *));

  memset(cur_board->level_id, 0, layer_size);
  memset(cur_board->level_color, 7, layer_size);
  memset(cur_board->level_param, 0, layer_size);
  memset(cur_board->level_under_id, 0, layer_size);
  memset(cur_board->level_under_color, 7, layer_size);
  memset(cur_board->level_under_param, 0, layer_size);
  if(cur_board->overlay_mode)
  {
    memset(cur_board->overlay, 32, layer_size);
    memset(cur_board->overlay_color, 7, layer_size);
  }

  cur_board->level_id[0] = 127;

  return cur_board;
}

void save_board_file(struct board *cur_board, char *name)
{
  FILE *board_file = fopen_unsafe(name, "wb");

  if(board_file)
  {
    fputc(0xff, board_file);

    fputc('M', board_file);
    fputc((WORLD_VERSION >> 8) & 0xff, board_file);
    fputc(WORLD_VERSION & 0xff, board_file);

    optimize_null_objects(cur_board);
    save_board(cur_board, board_file, 0, WORLD_VERSION);
    // Write name
    fwrite(cur_board->board_name, 25, 1, board_file);
    fclose(board_file);
  }
}

void change_board_size(struct board *src_board, int new_width, int new_height)
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
      src_board->level_id = crealloc(level_id, new_size);
      src_board->level_color = crealloc(level_color, new_size);
      src_board->level_param = crealloc(level_param, new_size);
      src_board->level_under_id = crealloc(level_under_id, new_size);
      src_board->level_under_color = crealloc(level_under_color, new_size);
      src_board->level_under_param = crealloc(level_under_param, new_size);

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

        src_board->overlay = crealloc(overlay, new_size);
        src_board->overlay_color = crealloc(overlay_color, new_size);

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
      src_board->level_id = crealloc(level_id, new_size);
      src_board->level_color = crealloc(level_color, new_size);
      src_board->level_param = crealloc(level_param, new_size);
      src_board->level_under_id = crealloc(level_under_id, new_size);
      src_board->level_under_color = crealloc(level_under_color, new_size);
      src_board->level_under_param = crealloc(level_under_param, new_size);

      // Resize the overlay too, if it exists
      if(overlay_mode)
      {
        src_board->overlay = crealloc(overlay, new_size);
        src_board->overlay_color = crealloc(overlay_color, new_size);
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

      src_board->level_id = crealloc(level_id, new_size);
      src_board->level_color = crealloc(level_color, new_size);
      src_board->level_param = crealloc(level_param, new_size);
      src_board->level_under_id = crealloc(level_under_id, new_size);
      src_board->level_under_color = crealloc(level_under_color, new_size);
      src_board->level_under_param = crealloc(level_under_param, new_size);

      // Resize the overlay too, if it exists
      if(overlay_mode)
      {
        src_board->overlay = crealloc(overlay, new_size);
        src_board->overlay_color = crealloc(overlay_color, new_size);
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
