/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mzm.h"

#include "data.h"
#include "error.h"
#include "idput.h"
#include "legacy_robot.h"
#include "legacy_world.h"
#include "robot.h"
#include "str.h"
#include "world.h"
#include "world_prop.h"
#include "zip.h"


/* The MZM format hasn't changed a whole lot with zips; only the robots
 * section is really different. This is because MZMs are in general meant to
 * be a raw data format, and furthermore accessing the raw data might be useful.
 */


// This is assumed to not go over the edges.

static void save_mzm_common(struct world *mzx_world, int start_x, int start_y,
 int width, int height, int mode, int savegame, void *(*alloc)(size_t, void **),
 void **storage)
{
    int storage_mode = 0;
    void *buffer;
    unsigned char *bufferPtr;

    size_t mzm_size;
    int num_robots_alloc = 0;

    if(mode)
      storage_mode = 1;

    if(savegame)
      savegame = 1;

    // Before saving the MZM, we first calculate its file size
    // Then we allocate the memory needed to store the MZM
    mzm_size = 20;
    
    // Add on the storage space required to store all tiles
    mzm_size += (width * height) * (storage_mode ? 2 : 6);

    if(mode == 0)
    {
      // Allocate memory for robots
      struct board *src_board = mzx_world->current_board;
      struct robot **robot_list = src_board->robot_list_name_sorted;
      int num_robots_active = src_board->num_robots_active;
      int i;

      for(i = 0; i < num_robots_active; i++)
      {
        struct robot *cur_robot = robot_list[i];
        if (cur_robot)
        {
          if(cur_robot->xpos >= start_x &&
           cur_robot->ypos >= start_y &&
           cur_robot->xpos < start_x + width &&
           cur_robot->ypos < start_y + height)
          {
            mzm_size += save_robot_calculate_size(mzx_world, cur_robot,
             savegame, MZX_VERSION);
            num_robots_alloc++;
          }
        }
      }
    }

    // Now, we need to add the total overhead of the zip file.
    // File names are all "r##", so the max name length is 3.
    mzm_size += zip_bound_total_header_usage(num_robots_alloc, 3);

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

    mem_putw(MZX_VERSION, &bufferPtr);
    bufferPtr += 3;

    switch(mode)
    {
      // Board, raw
      case 0:
      {
        struct zip_archive *zp;

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

        // Get the zip redy 2 go
        zp = zip_open_mem_write(buffer, mzm_size);
        robot_table_position = bufferPtr - (unsigned char *)buffer;

        // Get the zip in position. Won't alter the bufferPtr
        zseek(zp, robot_table_position, SEEK_SET);

        // Go back to header to put robot table information
        if(num_robots)
        {
          struct robot **robot_list = src_board->robot_list;
          char name[4];

          bufferPtr = buffer;
          bufferPtr += 8;
          // Where the robots will be stored
          // (not actually necessary anymore)
          mem_putd(robot_table_position, &bufferPtr);
          // Number of robots
          mem_putc(num_robots, &bufferPtr);
          // Skip board storage mode
          bufferPtr += 1;
          // Savegame mode for robots
          mem_putc(savegame, &bufferPtr);

          // Write robots into the zip
          for(i = 0; i < num_robots; i++)
          {
            sprintf(name, "r%2.2X", (unsigned char) i);

            // Save each robot
            save_robot(mzx_world, robot_list[robot_numbers[i]], zp,
             savegame, MZX_VERSION, name);
          }
        }

        zip_close(zp, NULL);

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

static void *mem_allocate(size_t length, void **dest)
{
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
    save_mzm_common(mzx_world, start_x, start_y, width, height, mode, savegame,
     mem_allocate, (void **)(&buffer));
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

static void *str_allocate(size_t length, void **dest)
{
  struct str_allocate_data **dataPtr = (struct str_allocate_data **)dest;
  struct str_allocate_data *data = *dataPtr;

  data->str = new_string(data->mzx_world, data->name, length, data->id);
  
  return data->str->value;
}

void save_mzm_string(struct world *mzx_world, const char *name, int start_x,
 int start_y, int width, int height, int mode, int savegame, int id)
{
  struct str_allocate_data *data = malloc(sizeof(struct str_allocate_data));
  
  data->str = NULL;
  data->name = name;
  data->mzx_world = mzx_world;
  data->id = id;

  save_mzm_common(mzx_world, start_x, start_y, width, height, mode, savegame,
   str_allocate, (void **)(&data));
  free(data);
}

static int load_mzm_header(const unsigned char **_bufferptr, int file_length,
 int *width, int *height, int *storage_mode, int *savegame_mode,
 int *num_robots, int *robots_location, int *mzm_world_version)
{
  const unsigned char *bufferptr = *_bufferptr;
  char magic_string[5];

  if(file_length < 16)
    return -1;

  // MegaZeux 2.83 is the last version that won't save the ver,
  // so if we don't have a ver, just act like it's 2.83
  *mzm_world_version = V283;

  memcpy(magic_string, bufferptr, 4);
  magic_string[4] = 0;
  bufferptr += 4;

  if(!strncmp(magic_string, "MZMX", 4))
  {
    // An MZM1 file is always storage mode 0
    *storage_mode = 0;
    *savegame_mode = 0;
    *num_robots = 0;
    *robots_location = 0;
    *width = mem_getc(&bufferptr);
    *height = mem_getc(&bufferptr);
    bufferptr += 10;
  }
  else

  if(!strncmp(magic_string, "MZM2", 4))
  {
    *width = mem_getw(&bufferptr);
    *height = mem_getw(&bufferptr);
    *robots_location = mem_getd(&bufferptr);
    *num_robots = mem_getc(&bufferptr);
    *storage_mode = mem_getc(&bufferptr);
    *savegame_mode = mem_getc(&bufferptr);
    bufferptr += 1; // unused
  }
  else

  if(!strncmp(magic_string, "MZM3", 4))
  {
    if(file_length < 20)
      return -1;

    // MZM3 is like MZM2, except the robots are stored as source code if
    // savegame_mode is 0 and version >= VERSION_SOURCE.
    *width = mem_getw(&bufferptr);
    *height = mem_getw(&bufferptr);
    *robots_location = mem_getd(&bufferptr);
    *num_robots = mem_getc(&bufferptr);
    *storage_mode = mem_getc(&bufferptr);
    *savegame_mode = mem_getc(&bufferptr);
    *mzm_world_version = mem_getw(&bufferptr);
    bufferptr += 3; // unused
  }

  else
    return -1;

  *_bufferptr = bufferptr;
  return 0;
}

// This will clip.

static int load_mzm_common(struct world *mzx_world, const void *buffer,
 int file_length, int start_x, int start_y, int mode, int savegame, char *name)
{
  const unsigned char *bufferPtr = buffer;

  int storage_mode;
  int width;
  int height;
  int savegame_mode;
  int num_robots;
  int robots_location;
  int mzm_world_version;

  int data_start;
  int expected_data_size;

  if(load_mzm_header(&bufferPtr, file_length, &width, &height, &storage_mode,
   &savegame_mode, &num_robots, &robots_location, &mzm_world_version))
    goto err_invalid;

  data_start = bufferPtr - (const unsigned char *)buffer;
  expected_data_size = (width * height) * (storage_mode ? 2 : 6);

  // Validate
  if(
   (savegame_mode > 1) || (savegame_mode < 0) // Invalid save mode
   || (storage_mode > 1) || (storage_mode < 0) // Invalid storage mode
   || (file_length - data_start < expected_data_size) // not enough space
   || (file_length < robots_location) // The end of file is before the robots
   || (robots_location && (expected_data_size + data_start > robots_location))
    )
    goto err_invalid;

  // If the mzm version is newer than the MZX version, notify
  if(mzm_world_version > MZX_VERSION)
  {
    error_message(E_MZM_FILE_VERSION_TOO_RECENT, mzm_world_version, name);
  }

  // If the MZM is a save MZM but we're not loading at runtime, notify
  if(savegame_mode > savegame)
  {
    error_message(E_MZM_FILE_FROM_SAVEGAME, 0, name);
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
            struct zip_archive *zp;
            unsigned int file_id;
            unsigned int robot_id;
            int result;

            struct robot *cur_robot;
            int current_x, current_y;
            int offset;
            int new_param;
            int robot_calculated_size = 0;
            int robot_partial_size = 0;
            int current_position;
            int dummy = 0;

            // We suppress the errors that will generally occur here and barely
            // error check the zip functions. Why? This needs to run all the way
            // through, regardless of whether it finds errors. Otherwise, we'll
            // get invalid robots with ID=0 littered around the board.

            set_error_suppression(E_WORLD_ROBOT_MISSING, 1);
            set_error_suppression(E_BOARD_ROBOT_CORRUPT, 1);

            // Reset the error count.
            get_and_reset_error_count();

            if(mzm_world_version <= MZX_LEGACY_FORMAT_VERSION)
            {
              robot_partial_size =
               legacy_calculate_partial_robot_size(savegame_mode,
               mzm_world_version);

              bufferPtr = buffer;
              bufferPtr += robots_location;
              zp = NULL;
            }

            else
            {
              zp = zip_open_mem_read(buffer, file_length);
              zip_read_directory(zp);
              assign_fprops(zp, 1);
            }

            // If we're loading a "runtime MZM" then it means that we're loading
            // bytecode. And to do this we must both be in-game and must be
            // running the same version this was made in. But since loading
            // dynamically created MZMs in the editor is still useful, we'll just
            // dummy out the robots.

            if((savegame_mode > savegame) ||
              (MZX_VERSION < mzm_world_version))
            {
              dummy = 1;
            }

            for(i = 0; i < num_robots; i++)
            {
              cur_robot = cmalloc(sizeof(struct robot));

              current_x = robot_x_locations[i];
              current_y = robot_y_locations[i];

              // TODO: Skipped legacy robots aren't checked for, so the loaded
              // chars for dummy robots on clipped MZMs might be off. This
              // shouldn't matter too often though.

              if(mzm_world_version <= MZX_LEGACY_FORMAT_VERSION)
              {
                create_blank_robot(cur_robot);

                current_position = bufferPtr - (const unsigned char *)buffer;

                // If we fail, we have to continue, or robots won't be dummied
                // correctly. In this case, seek to the end.

                if(current_position + robot_partial_size <= file_length)
                {
                  robot_calculated_size =
                   legacy_load_robot_calculate_size(bufferPtr, savegame_mode,
                   mzm_world_version);

                  if(current_position + robot_calculated_size <= file_length)
                  {
                    legacy_load_robot_from_memory(mzx_world, cur_robot, bufferPtr,
                     savegame_mode, mzm_world_version, (int)current_position);
                  }
                  else
                  {
                    bufferPtr = (const unsigned char*)buffer + file_length;
                    dummy = 1;
                  }
                }
                else
                {
                  bufferPtr = (const unsigned char*)buffer + file_length;
                  dummy = 1;
                }

                bufferPtr += robot_calculated_size;
              }

              // Search the zip until a robot is found.
              else do
              {
                result = zip_get_next_prop(zp, &file_id, NULL, &robot_id);

                if(result != ZIP_SUCCESS)
                {
                  // We have to continue, or we'll get screwed up robots.
                  create_blank_robot(cur_robot);
                  create_blank_robot_program(cur_robot);
                  dummy = 1;
                  break;
                }
                else

                if(file_id != FPROP_ROBOT || (int)robot_id < i)
                {
                  // Not a robot or is a skipped robot
                  zip_skip_file(zp);
                }
                else

                if((int)robot_id > i)
                {
                  // There's a robot missing.
                  create_blank_robot(cur_robot);
                  create_blank_robot_program(cur_robot);
                  break;
                }

                else
                {
                  load_robot(mzx_world, cur_robot, zp, savegame_mode,
                   mzm_world_version);
                  break;
                }
              }
              while(1);

              if(dummy)
              {
                // Unfortunately, getting the actual character for the robot is
                // kind of a lot of work right now. We have to load it then
                // throw it away.

                // If this is from a futer version and the format changed, we'll
                // just end up with an 'R' char, but this shouldn't happen again
                if(current_x != -1)
                {
                  offset = current_x + (current_y * board_width);
                  level_id[offset] = CUSTOM_BLOCK;
                  level_param[offset] = cur_robot->robot_char;
                }
                clear_robot(cur_robot);
              }

              else
              {
                cur_robot->world_version = mzx_world->version;
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
            }

            if(mzm_world_version > MZX_LEGACY_FORMAT_VERSION)
            {
              zip_close(zp, NULL);
            }

            // If any errors were encountered, report
            if(get_and_reset_error_count())
              goto err_robots;
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

err_robots:
  // The main file loaded fine, but there was a problem handling robots
  error_message(E_MZM_ROBOT_CORRUPT, 0, name);
  return 0;

err_invalid:
  error_message(E_MZM_FILE_INVALID, 0, name);
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

    success = load_mzm_common(mzx_world, buffer, (int)file_size, start_x,
     start_y, mode, savegame, name);
    free(buffer);
    return success;
  }
  else
  {
    error_message(E_MZM_DOES_NOT_EXIST, 0, name);
    return -1;
  }
}

int load_mzm_memory(struct world *mzx_world, char *name, int start_x,
 int start_y, int mode, int savegame, const void *buffer, size_t length)
{
  return load_mzm_common(mzx_world, buffer, (int)length, start_x, start_y,
   mode, savegame, name);
}

void load_mzm_size(char *name, int *width, int *height)
{
  FILE *input_file;
  const unsigned char *bufferptr;
  char buffer[20];
  int read_length;
  int ignore;

  *width = -1;
  *height = -1;

  input_file = fopen_unsafe(name, "rb");
  if(input_file)
  {
    read_length = fread(buffer, 1, 20, input_file);
    fclose(input_file);

    bufferptr = (const unsigned char *)buffer;

    load_mzm_header(&bufferptr, read_length, width, height,
     &ignore, &ignore, &ignore, &ignore, &ignore);
  }

  return;
}
