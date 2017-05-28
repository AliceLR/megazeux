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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mzm.h"
#include "data.h"
#include "idput.h"
#include "world.h"
#include "validation.h"

// This is assumed to not go over the edges.

static void save_mzm_common(struct world *mzx_world, int start_x, int start_y, int width,
 int height, int mode, int savegame, void *(*alloc)(size_t, void **), void **storage)
{
    int storage_mode = 0;
    void *buffer;
    unsigned char *bufferPtr;
    int robot_sizes[256];

    size_t mzm_size;

    if(mode)
      storage_mode = 1;

    if(savegame)
      savegame = 1;

    // Before saving the MZM, we first calculate its file size
    // Then we allocate the memory needed to store the MZM
    mzm_size = 20;
    
    // Add on the storage space required to store all tiles
    if (storage_mode == 0) {
      mzm_size += width * height * 6;
    } else {
      mzm_size += width * height * 2;
    }

    if (mode == 0) {
      // Allocate memory for robots
      {
        struct board *src_board = mzx_world->current_board;
        struct robot **robot_list = src_board->robot_list_name_sorted;
        int num_robots_active = src_board->num_robots_active;
        int rid;
        int offset;
        size_t robot_size;

        for (int i = 0; i < num_robots_active; i++) {
          struct robot *cur_robot = robot_list[i];
          if (cur_robot)
          {
            if (cur_robot->xpos >= start_x && cur_robot->ypos >= start_y &&
                cur_robot->xpos < start_x + width && cur_robot->ypos < start_y + height)
            {
              offset = cur_robot->xpos + (cur_robot->ypos * src_board->board_width);
              assert(is_robot((enum thing)src_board->level_id[offset]));
              rid = src_board->level_param[offset];
              robot_size = save_robot_calculate_size(mzx_world, cur_robot,
               savegame, WORLD_VERSION);
              robot_sizes[rid] = (int)robot_size;
              mzm_size += robot_size;
            }
          }
        }
      }
    }
    buffer = alloc(mzm_size, storage);
    bufferPtr = buffer;
    memcpy(bufferPtr, "MZM3", 4);
    bufferPtr += 4;

    mem_putw(width, &bufferPtr);
    mem_putw(height, &bufferPtr);
    // Come back here later if there's robot information
    mem_putd(0, &bufferPtr);
    mem_putc(0, &bufferPtr);
    mem_putc(storage_mode, &bufferPtr);
    mem_putc(0, &bufferPtr);

    mem_putw(WORLD_VERSION, &bufferPtr);
    bufferPtr += 3;

    switch(mode)
    {
      // Board, raw
      case 0:
      {
        struct board *src_board = mzx_world->current_board;
        int board_width = src_board->board_width;
        char *level_id = src_board->level_id;
        char *level_param = src_board->level_param;
        char *level_color = src_board->level_color;
        char *level_under_id = src_board->level_under_id;
        char *level_under_param = src_board->level_under_param;
        char *level_under_color = src_board->level_under_color;
        int x, y;
        int offset = start_x + (start_y * board_width);
        int line_skip = board_width - width;
        int num_robots = 0;
        int robot_numbers[256];
        int robot_table_position;
        enum thing current_id;
        int i;

        for(y = 0; y < height; y++)
        {
          for(x = 0; x < width; x++, offset++)
          {
            current_id = (enum thing)level_id[offset];

            if(is_robot(current_id))
            {
              // Robot
              robot_numbers[num_robots] = level_param[offset];
              num_robots++;

              mem_putc(current_id, &bufferPtr);
              mem_putc(0, &bufferPtr);
              mem_putc(level_color[offset], &bufferPtr);
              mem_putc(level_under_id[offset], &bufferPtr);
              mem_putc(level_under_param[offset], &bufferPtr);
              mem_putc(level_under_color[offset], &bufferPtr);
            }
            else

            if((current_id == SENSOR) || is_signscroll(current_id) ||
             (current_id == PLAYER))
            {
              // Sensor, scroll, sign, or player
              // Put customblock fake
              mem_putc((int)CUSTOM_BLOCK, &bufferPtr);
              mem_putc(get_id_char(src_board, offset), &bufferPtr);
              mem_putc(get_id_color(src_board, offset), &bufferPtr);
              mem_putc(level_under_id[offset], &bufferPtr);
              mem_putc(level_under_param[offset], &bufferPtr);
              mem_putc(level_under_color[offset], &bufferPtr);
            }
            else
            {
              mem_putc(current_id, &bufferPtr);
              mem_putc(level_param[offset], &bufferPtr);
              mem_putc(level_color[offset], &bufferPtr);
              mem_putc(level_under_id[offset], &bufferPtr);
              mem_putc(level_under_param[offset], &bufferPtr);
              mem_putc(level_under_color[offset], &bufferPtr);
            }
          }
          offset += line_skip;
        }

        // Go back to header to put robot table information
        if(num_robots)
        {
          struct robot **robot_list = src_board->robot_list;

          robot_table_position = bufferPtr - (unsigned char *)buffer;
          bufferPtr = buffer;
          bufferPtr += 8;
          // Where the robots will be stored
          mem_putd(robot_table_position, &bufferPtr);
          // Number of robots
          mem_putc(num_robots, &bufferPtr);
          // Skip board storage mode
          bufferPtr += 1;
          // Savegame mode for robots
          mem_putc(savegame, &bufferPtr);

          // Back to robot table position
          bufferPtr = buffer;
          bufferPtr += robot_table_position;

          for(i = 0; i < num_robots; i++)
          {
            // Save each robot
            save_robot_to_memory(robot_list[robot_numbers[i]], bufferPtr, savegame, WORLD_VERSION);
            bufferPtr += robot_sizes[robot_numbers[i]];
          }
        }

        break;
      }

      // Overlay/Vlayer
      case 1:
      case 2:
      {
        struct board *src_board = mzx_world->current_board;
        int board_width;
        int x, y;
        int offset;
        int line_skip;
        char *chars;
        char *colors;

        if(mode == 1)
        {
          // Overlay
          if(!src_board->overlay_mode)
            setup_overlay(src_board, 3);

          chars = src_board->overlay;
          colors = src_board->overlay_color;
          board_width = src_board->board_width;
        }
        else
        {
          // Vlayer
          chars = mzx_world->vlayer_chars;
          colors = mzx_world->vlayer_colors;
          board_width = mzx_world->vlayer_width;
        }

        offset = start_x + (start_y * board_width);
        line_skip = board_width - width;

        for(y = 0; y < height; y++)
        {
          for(x = 0; x < width; x++, offset++)
          {
            mem_putc(chars[offset], &bufferPtr);
            mem_putc(colors[offset], &bufferPtr);
          }
          offset += line_skip;
        }

        break;
      }

      // Board, char based
      case 3:
      {
        struct board *src_board = mzx_world->current_board;
        int board_width = src_board->board_width;
        int x, y;
        int offset = start_x + (start_y * board_width);
        int line_skip = board_width - width;

        for(y = 0; y < height; y++)
        {
          for(x = 0; x < width; x++, offset++)
          {
            mem_putc(get_id_char(src_board, offset), &bufferPtr);
            mem_putc(get_id_color(src_board, offset), &bufferPtr);
          }
          offset += line_skip;
        }


        break;
      }
    }

}

static void *mem_allocate(size_t length, void **dest) {
  size_t *dataPtr;
  *dest = malloc(length + sizeof(size_t));
  dataPtr = *dest;
  dataPtr[0] = length;
  return &dataPtr[1];
}

void save_mzm(struct world *mzx_world, char *name, int start_x, int start_y,
 int width, int height, int mode, int savegame)
{
  FILE *output_file = fopen_unsafe(name, "wb");
  size_t *buffer;
  size_t length;
  if(output_file)
  {
    buffer = NULL;
    save_mzm_common(mzx_world, start_x, start_y, width, height, mode, savegame, mem_allocate, (void **)(&buffer));
    length = buffer[0];
    fwrite(&buffer[1], length, 1, output_file);
    free(buffer);
    fclose(output_file);
  }
}

struct str_allocate_data {
  struct string *str;
  const char *name;
  struct world *mzx_world;
  int id;
};

static void *str_allocate(size_t length, void **dest) {
  struct str_allocate_data **dataPtr = (struct str_allocate_data **)dest;
  struct str_allocate_data *data = *dataPtr;

  data->str = new_string(data->mzx_world, data->name, length, data->id);
  
  return data->str->value;
}

void save_mzm_string(struct world *mzx_world, const char *name, int start_x, int start_y,
 int width, int height, int mode, int savegame, int id)
{
  struct str_allocate_data *data = malloc(sizeof(struct str_allocate_data));
  
  data->str = NULL;
  data->name = name;
  data->mzx_world = mzx_world;
  data->id = id;

  save_mzm_common(mzx_world, start_x, start_y, width, height, mode, savegame, str_allocate, (void **)(&data));
  free(data);
}

// This will clip.

static int load_mzm_common(struct world *mzx_world, const void *buffer, int file_length, int start_x,
 int start_y, int mode, int savegame, char *name)
{
  const unsigned char *bufferPtr = buffer;
  char magic_string[5];
  int storage_mode;
  int width, height;
  int savegame_mode;
  int num_robots;
  int robots_location;

  int data_start;
  int expected_data_size;

  // MegaZeux 2.83 is the last version that won't save the ver,
  // so if we don't have a ver, just act like it's 2.83
  int mzm_world_version = 0x0253;

  if (file_length < 4)
    goto err_invalid;

  memcpy(magic_string, bufferPtr, 4);
  bufferPtr += 4;

  magic_string[4] = 0;

  if(!strncmp(magic_string, "MZMX", 4))
  {
    // An MZM1 file is always storage mode 0
    storage_mode = 0;
    savegame_mode = 0;
    num_robots = 0;
    robots_location = 0;
    if (file_length < 16) goto err_invalid;
    width = mem_getc(&bufferPtr);
    height = mem_getc(&bufferPtr);
    bufferPtr += 10;
  }
  else

  if(!strncmp(magic_string, "MZM2", 4))
  {
    if (file_length < 16) goto err_invalid;
    width = mem_getw(&bufferPtr);
    height = mem_getw(&bufferPtr);
    robots_location = mem_getd(&bufferPtr);
    num_robots = mem_getc(&bufferPtr);
    storage_mode = mem_getc(&bufferPtr);
    savegame_mode = mem_getc(&bufferPtr);
    bufferPtr += 1;
  }
  else

  if(!strncmp(magic_string, "MZM3", 4))
  {
    if (file_length < 20) goto err_invalid;
    // MZM3 is like MZM2, except the robots are stored as source code if
    // savegame_mode is 0 and version >= VERSION_PROGRAM_SOURCE.
    width = mem_getw(&bufferPtr);
    height = mem_getw(&bufferPtr);
    robots_location = mem_getd(&bufferPtr);
    num_robots = mem_getc(&bufferPtr);
    storage_mode = mem_getc(&bufferPtr);
    savegame_mode = mem_getc(&bufferPtr);
    mzm_world_version = mem_getw(&bufferPtr);
    bufferPtr += 3;
  }

  else
    goto err_invalid;

  data_start = bufferPtr - (const unsigned char *)buffer;
  expected_data_size = (width * height) * (storage_mode ? 2 : 6);

  // Validate
  if(
    (savegame_mode > 1) || (savegame_mode < 0) // Invalid save mode
    || (storage_mode > 1) || (storage_mode < 0) // Invalid storage mode
    || (file_length - data_start < expected_data_size) // not enough space to store data
    || (file_length < robots_location) // The end of file is before the robot offset
    || (robots_location && (expected_data_size + data_start > robots_location)) // robots offset before data end
    )
    goto err_invalid;

  // If the mzm version is newer than the MZX version, show a message and continue.
  if(mzm_world_version > WORLD_VERSION)
  {
    val_error_str(MZM_FILE_VERSION_TOO_RECENT, mzm_world_version, name);
    set_validation_suppression(MZM_FILE_VERSION_TOO_RECENT, 1);
  }

  // If the MZM is a save MZM but we're not loading at runtime, show a message and continue.
  if(savegame_mode > savegame)
  {
    val_error_str(MZM_FILE_FROM_SAVEGAME, 0, name);
  }

  switch(mode)
  {
    // Write to board
    case 0:
    {
      struct board *src_board = mzx_world->current_board;
      int board_width = src_board->board_width;
      int board_height = src_board->board_height;
      int effective_width = width;
      int effective_height = height;
      int file_line_skip;
      int line_skip;
      int x, y;
      int offset = start_x + (start_y * board_width);
      char *level_id = src_board->level_id;
      char *level_param = src_board->level_param;
      char *level_color = src_board->level_color;
      char *level_under_id = src_board->level_under_id;
      char *level_under_param = src_board->level_under_param;
      char *level_under_color = src_board->level_under_color;
      enum thing src_id;

      // Clip

      if((effective_width + start_x) >= board_width)
        effective_width = board_width - start_x;

      if((effective_height + start_y) >= board_height)
        effective_height = board_height - start_y;

      line_skip = board_width - effective_width;

      switch(storage_mode)
      {
        case 0:
        {
          // Board style, write as is
          enum thing current_id;
          int current_robot_loaded = 0;
          int robot_x_locations[256];
          int robot_y_locations[256];
          int width_difference;
          int i;

          width_difference = width - effective_width;

          for(y = 0; y < effective_height; y++)
          {
            for(x = 0; x < effective_width; x++, offset++)
            {
              current_id = (enum thing)mem_getc(&bufferPtr);

              if(current_id >= SENSOR)
              {
                if(is_robot(current_id))
                {
                  robot_x_locations[current_robot_loaded] = x + start_x;
                  robot_y_locations[current_robot_loaded] = y + start_y;
                  current_robot_loaded++;
                }
                // Wipe a bunch of crap we don't want in MZMs with spaces
                else
                  current_id = 0;
              }

              src_id = (enum thing)level_id[offset];

              if(src_id >= SENSOR)
              {
                if(src_id == SENSOR)
                  clear_sensor_id(src_board, level_param[offset]);
                else

                if(is_signscroll(src_id))
                  clear_scroll_id(src_board, level_param[offset]);
                else

                if(is_robot(src_id))
                  clear_robot_id(src_board, level_param[offset]);
              }

              // Don't allow the player to be overwritten
              if(src_id != PLAYER)
              {
                level_id[offset] = current_id;
                level_param[offset] = mem_getc(&bufferPtr);
                level_color[offset] = mem_getc(&bufferPtr);
                level_under_id[offset] = mem_getc(&bufferPtr);
                level_under_param[offset] = mem_getc(&bufferPtr);
                level_under_color[offset] = mem_getc(&bufferPtr);

                // We don't want this on the under layer, thanks
                if(level_under_id[offset] >= SENSOR)
                {
                  level_under_id[offset] = 0;
                  level_under_param[offset] = 0;
                  level_under_color[offset] = 7;
                }
              }
              else
              {
                bufferPtr += 5;
              }
            }

            offset += line_skip;

            // Gotta run through and mark the next robots to be skipped
            for(i = 0; i < width_difference; i++)
            {
              current_id = (enum thing)mem_getc(&bufferPtr);
              bufferPtr += 5;

              if(is_robot(current_id))
              {
                robot_x_locations[current_robot_loaded] = -1;
                current_robot_loaded++;
              }
            }
          }

          for(i = current_robot_loaded; i < num_robots; i++)
          {
            robot_x_locations[i] = -1;
          }

          if(num_robots)
          {
            struct robot *cur_robot;
            int current_x, current_y;
            int offset;
            int new_param;
            int robot_calculated_size;
            int robot_partial_size;
            int current_position;

            robot_partial_size = calculate_partial_robot_size(savegame_mode, mzm_world_version);

            // If we're loading a "runtime MZM" then it means that we're loading
            // bytecode. And to do this we must both be in-game and must be
            // running the same version this was made in. But since loading
            // dynamically created MZMs in the editor is still useful, we'll just
            // dummy out the robots.

            // We don't want to see these in any case.
            set_validation_suppression(WORLD_ROBOT_MISSING, 1);
            set_validation_suppression(BOARD_ROBOT_CORRUPT, 1);

            if((savegame_mode > savegame) ||
              (WORLD_VERSION < mzm_world_version))
            {
              bufferPtr = buffer;
              bufferPtr += robots_location;

              for(i = 0; i < num_robots; i++)
              {
                current_x = robot_x_locations[i];
                current_y = robot_y_locations[i];

                // Unfortunately, getting the actual character for the robot is
                // kind of a lot of work right now. We have to load it then
                // throw it away.

                // If this is from a futer version and the format changed, we'll
                // just end up with an 'R' char, but we don't plan on changing
                // the robot format until the zip overhaul (which will use
                // different code).
                if(current_x != -1)
                {
                  current_position = bufferPtr - (const unsigned char *)buffer;

                  if (current_position + robot_partial_size > file_length)
                    goto err_invalid;
                  robot_calculated_size = load_robot_calculate_size(bufferPtr, savegame_mode, mzm_world_version);
                  if (current_position + robot_calculated_size > file_length)
                    goto err_invalid;

                  cur_robot = cmalloc(sizeof(struct robot));

                  cur_robot->world_version = mzx_world->version;
                  load_robot_from_memory(mzx_world, cur_robot, bufferPtr,
                   savegame_mode, mzm_world_version, (int)current_position);
                  bufferPtr += robot_calculated_size;
                  offset = current_x + (current_y * board_width);
                  level_id[offset] = CUSTOM_BLOCK;
                  level_param[offset] = cur_robot->robot_char;
                  clear_robot(cur_robot);
                }
              }
            }
            else
            {
              int missing_count = get_error_count(WORLD_ROBOT_MISSING);
              int corrupt_count = get_error_count(BOARD_ROBOT_CORRUPT);

              bufferPtr = buffer;
              bufferPtr += robots_location;

              for(i = 0; i < num_robots; i++)
              {
                current_position = bufferPtr - (const unsigned char *)buffer;

                if (current_position + robot_partial_size > file_length)
                  goto err_invalid;
                robot_calculated_size = load_robot_calculate_size(bufferPtr, savegame_mode, mzm_world_version);
                if (current_position + robot_calculated_size > file_length)
                  goto err_invalid;

                cur_robot = cmalloc(sizeof(struct robot));
                cur_robot->world_version = mzx_world->version;
                load_robot_from_memory(mzx_world, cur_robot, bufferPtr, savegame_mode,
                  mzm_world_version, bufferPtr - (const unsigned char *)buffer);
                bufferPtr += robot_calculated_size;
                current_x = robot_x_locations[i];
                current_y = robot_y_locations[i];
                cur_robot->xpos = current_x;
                cur_robot->ypos = current_y;

#ifdef CONFIG_DEBYTECODE
                // If we're loading source code at runtime, we need to compile it
                if(savegame_mode < savegame)
                  prepare_robot_bytecode(mzx_world, cur_robot);
#endif

                if(current_x != -1)
                {
                  new_param = find_free_robot(src_board);
                  offset = current_x + (current_y * board_width);

                  if(new_param != -1)
                  {
                    if((enum thing)level_id[offset] != PLAYER)
                    {
                      add_robot_name_entry(src_board, cur_robot,
                        cur_robot->robot_name);
                      src_board->robot_list[new_param] = cur_robot;
                      cur_robot->xpos = current_x;
                      cur_robot->ypos = current_y;
                      level_param[offset] = new_param;
                    }
                    else
                    {
                      clear_robot(cur_robot);
                    }
                  }
                  else
                  {
                    clear_robot(cur_robot);
                    level_id[offset] = 0;
                    level_param[offset] = 0;
                    level_color[offset] = 7;
                  }
                }
                else
                {
                  clear_robot(cur_robot);
                }
              }

              // If any of these errors were encountered, report once
              if((missing_count - get_error_count(WORLD_ROBOT_MISSING)) |
                (corrupt_count - get_error_count(BOARD_ROBOT_CORRUPT)))
              {
                val_error_str(MZM_ROBOT_CORRUPT, 0, name);
                set_validation_suppression(MZM_ROBOT_CORRUPT, 1);
              }
            }
          }
          break;
        }

        case 1:
        {
          // Compact style; expand to customblocks
          // Board style, write as is

          file_line_skip = (width - effective_width) * 2;

          for(y = 0; y < effective_height; y++)
          {
            for(x = 0; x < effective_width; x++, offset++)
            {
              src_id = (enum thing)level_id[offset];

              if(src_id == SENSOR)
                clear_sensor_id(src_board, level_param[offset]);
              else

              if(is_signscroll(src_id))
                clear_scroll_id(src_board, level_param[offset]);
              else

              if(is_robot(src_id))
                clear_robot_id(src_board, level_param[offset]);

              // Don't allow the player to be overwritten
              if(src_id != PLAYER)
              {
                level_id[offset] = CUSTOM_BLOCK;
                level_param[offset] = mem_getc(&bufferPtr);
                level_color[offset] = mem_getc(&bufferPtr);
                level_under_id[offset] = 0;
                level_under_param[offset] = 0;
                level_under_color[offset] = 0;
              }
              else
              {
                bufferPtr += 2;
              }
            }

            offset += line_skip;
            bufferPtr += file_line_skip;
          }
          break;
        }
      }
      break;
    }

    // Overlay/vlayer
    case 1:
    case 2:
    {
      struct board *src_board = mzx_world->current_board;
      int dest_width = src_board->board_width;
      int dest_height = src_board->board_height;
      char *dest_chars;
      char *dest_colors;
      int effective_width = width;
      int effective_height = height;
      int file_line_skip;
      int line_skip;
      int x, y;
      int offset;

      if(mode == 1)
      {
        // Overlay
        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        dest_chars = src_board->overlay;
        dest_colors = src_board->overlay_color;
        dest_width = src_board->board_width;
        dest_height = src_board->board_height;
      }
      else
      {
        // Vlayer
        dest_chars = mzx_world->vlayer_chars;
        dest_colors = mzx_world->vlayer_colors;
        dest_width = mzx_world->vlayer_width;
        dest_height = mzx_world->vlayer_height;
      }

      offset = start_x + (start_y * dest_width);

      // Clip

      if((effective_width + start_x) >= dest_width)
        effective_width = dest_width - start_x;

      if((effective_height + start_y) >= dest_height)
        effective_height = dest_height - start_y;

      line_skip = dest_width - effective_width;

      switch(storage_mode)
      {
        case 0:
        {
          // Coming from board storage; for now use param as char

          file_line_skip = (width - effective_width) * 6;

          for(y = 0; y < effective_height; y++)
          {
            for(x = 0; x < effective_width; x++, offset++)
            {
              // Skip ID
              bufferPtr += 1;
              dest_chars[offset] = mem_getc(&bufferPtr);
              dest_colors[offset] = mem_getc(&bufferPtr);
              // Skip under parts
              bufferPtr += 3;
            }

            offset += line_skip;
            bufferPtr += file_line_skip;
          }
          break;
        }

        case 1:
        {
          // Coming from layer storage; transfer directly

          file_line_skip = (width - effective_width) * 2;

          for(y = 0; y < effective_height; y++)
          {
            for(x = 0; x < effective_width; x++, offset++)
            {
              dest_chars[offset] = mem_getc(&bufferPtr);
              dest_colors[offset] = mem_getc(&bufferPtr);
            }

            offset += line_skip;
            bufferPtr += file_line_skip;
          }
          break;
        }

      }
      break;
    }
  }

  // Clear none of the validation error suppressions:
  // 1) in combination with poor practice, they could lock MZX,
  // 2) they'll get reset after reloading the world.
  return 0;

err_invalid:
  val_error_str(MZM_FILE_INVALID, 0, name);
  set_validation_suppression(MZM_FILE_INVALID, 1);
  return -1;
}

int load_mzm(struct world *mzx_world, char *name, int start_x, int start_y,
 int mode, int savegame)
{
  FILE *input_file;
  size_t file_size;
  void *buffer;
  int success;
  input_file = fopen_unsafe(name, "rb");
  if(input_file)
  {
    fseek(input_file, 0, SEEK_END);
    file_size = ftell(input_file);
    buffer = cmalloc(file_size);
    fseek(input_file, 0, SEEK_SET);
    fread(buffer, file_size, 1, input_file);
    fclose(input_file);

    success = load_mzm_common(mzx_world, buffer, (int)file_size, start_x, start_y, mode, savegame, name);
    free(buffer);
    return success;
  } else {
    val_error_str(MZM_DOES_NOT_EXIST, 0, name);
    set_validation_suppression(MZM_DOES_NOT_EXIST, 1);
    return -1;
  }
}

int load_mzm_memory(struct world *mzx_world, char *name, int start_x, int start_y,
 int mode, int savegame, const void *buffer, size_t length)
{
  return load_mzm_common(mzx_world, buffer, (int)length, start_x, start_y, mode, savegame, name);
}
