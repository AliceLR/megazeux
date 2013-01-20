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

#include "mzm.h"
#include "data.h"
#include "idput.h"
#include "world.h"
#include "validation.h"

// This is assumed to not go over the edges.

void save_mzm(struct world *mzx_world, char *name, int start_x, int start_y,
 int width, int height, int mode, int savegame)
{
  FILE *output_file = fopen_unsafe(name, "wb");

  if(output_file)
  {
    int storage_mode = 0;

    if(mode)
      storage_mode = 1;

    if(savegame)
      savegame = 1;

    fwrite("MZM3", 4, 1, output_file);

    fputw(width, output_file);
    fputw(height, output_file);
    // Come back here later if there's robot information
    fputd(0, output_file);
    fputc(0, output_file);
    fputc(storage_mode, output_file);
    fputc(0, output_file);

    fputw(WORLD_VERSION, output_file);
    fseek(output_file, 3, SEEK_CUR);

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

              fputc(current_id, output_file);
              fputc(0, output_file);
              fputc(level_color[offset], output_file);
              fputc(level_under_id[offset], output_file);
              fputc(level_under_param[offset], output_file);
              fputc(level_under_color[offset], output_file);
            }
            else

            if((current_id == SENSOR) || is_signscroll(current_id) ||
             (current_id == PLAYER))
            {
              // Sensor, scroll, sign, or player
              // Put customblock fake
              fputc((int)CUSTOM_BLOCK, output_file);
              fputc(get_id_char(src_board, offset), output_file);
              fputc(get_id_color(src_board, offset), output_file);
              fputc(level_under_id[offset], output_file);
              fputc(level_under_param[offset], output_file);
              fputc(level_under_color[offset], output_file);
            }
            else
            {
              fputc(current_id, output_file);
              fputc(level_param[offset], output_file);
              fputc(level_color[offset], output_file);
              fputc(level_under_id[offset], output_file);
              fputc(level_under_param[offset], output_file);
              fputc(level_under_color[offset], output_file);
            }
          }
          offset += line_skip;
        }

        // Go back to header to put robot table information
        if(num_robots)
        {
          struct robot **robot_list = src_board->robot_list;

          robot_table_position = ftell(output_file);
          fseek(output_file, 8, SEEK_SET);
          // Where the robots will be stored
          fputd(robot_table_position, output_file);
          // Number of robots
          fputc(num_robots, output_file);
          // Skip board storage mode
          fseek(output_file, 1, SEEK_CUR);
          // Savegame mode for robots
          fputc(savegame, output_file);

          // Back to robot table position
          fseek(output_file, robot_table_position, SEEK_SET);

          for(i = 0; i < num_robots; i++)
          {
            // Save each robot
            save_robot(robot_list[robot_numbers[i]], output_file, savegame, WORLD_VERSION);
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
            fputc(chars[offset], output_file);
            fputc(colors[offset], output_file);
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
            fputc(get_id_char(src_board, offset), output_file);
            fputc(get_id_color(src_board, offset), output_file);
          }
          offset += line_skip;
        }


        break;
      }
    }

    fclose(output_file);
  }
}

// This will clip.

int load_mzm(struct world *mzx_world, char *name, int start_x, int start_y,
 int mode, int savegame)
{
  FILE *input_file;

  if(!(input_file = fopen_unsafe(name, "rb")))
  {
    val_error(FILE_DOES_NOT_EXIST, 0);
    goto err_out;
  }

  if(input_file)
  {
    char magic_string[5];
    int storage_mode;
    int width, height;
    int savegame_mode;
    int num_robots;
    int robots_location;

    int data_start;
    int expected_data_size;
    int file_length;

    // MegaZeux 2.83 is the last version that won't save the ver,
    // so if we don't have a ver, just act like it's 2.83
    int mzm_world_version = 0x0253;

    file_length = ftell_and_rewind(input_file);

    if(!fread(magic_string, 4, 1, input_file))
      goto err_invalid;

    magic_string[4] = 0;

    if(!strncmp(magic_string, "MZMX", 4))
    {
      // An MZM1 file is always storage mode 0
      storage_mode = 0;
      savegame_mode = 0;
      num_robots = 0;
      robots_location = 0;
      width = fgetc(input_file);
      height = fgetc(input_file);
      fseek(input_file, 10, SEEK_CUR);
    }
    else

    if(!strncmp(magic_string, "MZM2", 4))
    {
      width = fgetw(input_file);
      height = fgetw(input_file);
      robots_location = fgetd(input_file);
      num_robots = fgetc(input_file);
      storage_mode = fgetc(input_file);
      savegame_mode = fgetc(input_file);
      fseek(input_file, 1, SEEK_CUR);
    }
    else

    if(!strncmp(magic_string, "MZM3", 4))
    {
      // MZM3 is like MZM2, except the robots are stored as source code if
      // savegame_mode is 0 and version >= VERSION_PROGRAM_SOURCE.
      width = fgetw(input_file);
      height = fgetw(input_file);
      robots_location = fgetd(input_file);
      num_robots = fgetc(input_file);
      storage_mode = fgetc(input_file);
      savegame_mode = fgetc(input_file);
      mzm_world_version = fgetw(input_file);
      fseek(input_file, 3, SEEK_CUR);
    }

    else
      goto err_invalid;

    data_start = ftell(input_file);
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
      val_error(MZM_FILE_VERSION_TOO_RECENT, mzm_world_version);
    }

    // If the MZM is a save MZM but we're not loading at runtime, show a message and continue.
    if(savegame_mode > savegame)
    {
      val_error(MZM_FILE_FROM_SAVEGAME, 0);
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
                current_id = (enum thing)fgetc(input_file);

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
                  level_param[offset] = fgetc(input_file);
                  level_color[offset] = fgetc(input_file);
                  level_under_id[offset] = fgetc(input_file);
                  level_under_param[offset] = fgetc(input_file);
                  level_under_color[offset] = fgetc(input_file);

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
                  fseek(input_file, 5, SEEK_CUR);
                }
              }

              offset += line_skip;

              // Gotta run through and mark the next robots to be skipped
              for(i = 0; i < width_difference; i++)
              {
                current_id = (enum thing)fgetc(input_file);
                fseek(input_file, 5, SEEK_CUR);

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

              // If we're loading a "runtime MZM" then it means that we're loading
              // bytecode. And to do this we must both be in-game and must be
              // running the same version this was made in. But since loading
              // dynamically created MZMs in the editor is still useful, we'll just
              // dummy out the robots.

              if((savegame_mode > savegame) ||
               (WORLD_VERSION < mzm_world_version))
              {
                fseek(input_file, robots_location, SEEK_SET);

                set_validation_suppression(1);
                for(i = 0; i < num_robots; i++)
                {
                  current_x = robot_x_locations[i];
                  current_y = robot_y_locations[i];

                  // Unfortunately, getting the actual character for the robot is
                  // kind of a lot of work right now. We have to load it then
                  // throw it away.

                  // If this is from a futer version and the format changed, we'll
                  // just end up with an 'R' char, but we don't plan on changing
                  // the robot format until the EBML overhaul (which will use
                  // different code).
                  if(current_x != -1)
                  {
                    cur_robot = load_robot_allocate(input_file, savegame_mode,
                     mzm_world_version, mzx_world->version);

                    offset = current_x + (current_y * board_width);
                    level_id[offset] = CUSTOM_BLOCK;
                    level_param[offset] = cur_robot->robot_char;
                    clear_robot(cur_robot);
                  }
                }
                set_validation_suppression(-1);
              }
              else
              {
                fseek(input_file, robots_location, SEEK_SET);

                for(i = 0; i < num_robots; i++)
                {
                  int check_val = fgetw(input_file);
                  fseek(input_file, -2, SEEK_CUR);
                  if(check_val < 0)
                  {
                    val_error(WORLD_ROBOT_MISSING, ftell(input_file));
                    set_validation_suppression(1);
                  }

                  cur_robot = load_robot_allocate(input_file, savegame_mode,
                   mzm_world_version, mzx_world->version);
                  current_x = robot_x_locations[i];
                  current_y = robot_y_locations[i];
                  cur_robot->xpos = current_x;
                  cur_robot->ypos = current_y;

#ifdef CONFIG_DEBYTECODE
                  // If we're loading source code at runtime, we need to compile it
                  if(savegame_mode < savegame)
                    prepare_robot_bytecode(cur_robot);
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
                  level_param[offset] = fgetc(input_file);
                  level_color[offset] = fgetc(input_file);
                  level_under_id[offset] = 0;
                  level_under_param[offset] = 0;
                  level_under_color[offset] = 0;
                }
                else
                {
                  fseek(input_file, 2, SEEK_CUR);
                }
              }

              offset += line_skip;
              fseek(input_file, file_line_skip, SEEK_CUR);
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
                fseek(input_file, 1, SEEK_CUR);
                dest_chars[offset] = fgetc(input_file);
                dest_colors[offset] = fgetc(input_file);
                // Skip under parts
                fseek(input_file, 3, SEEK_CUR);
              }

              offset += line_skip;
              fseek(input_file, file_line_skip, SEEK_CUR);
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
                dest_chars[offset] = fgetc(input_file);
                dest_colors[offset] = fgetc(input_file);
              }

              offset += line_skip;
              fseek(input_file, file_line_skip, SEEK_CUR);
            }
            break;
          }

        }
        break;
      }
    }
    fclose(input_file);
  }

  set_validation_suppression(-1);
  return 0;

err_invalid:
  val_error(MZM_FILE_INVALID, 0);
  fclose(input_file);
err_out:
  set_validation_suppression(-1);
  return -1;
}


