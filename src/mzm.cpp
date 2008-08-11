/* $Id: idput.h,v 1.2 1999/01/17 20:35:41 mental Exp $
 * MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick - exophase@adelphia.net
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

#include <stdlib.h>
#include <string.h>

#include "mzm.h"
#include "idput.h"
#include "world.h"
#include "fsafeopen.h"

// This is assumed to not go over the edges.

void save_mzm(World *mzx_world, char *name, int start_x, int start_y,
 int width, int height, int mode, int savegame)
{
  FILE *output_file = fsafeopen(name, "wb");

  if(output_file)
  {
    int storage_mode = 0;

    if(mode)
      storage_mode = 1;

    if(savegame)
      savegame = 1;

    fwrite("MZM2", 4, 1, output_file);
    fputw(width, output_file);
    fputw(height, output_file);
    // Come back here later if there's robot information
    fputd(0, output_file);
    fputc(0, output_file);
    fputc(storage_mode, output_file);
    fputc(0, output_file);
    fseek(output_file, 1, SEEK_CUR);
  
    switch(mode)
    {
      // Board, raw
      case 0:
      {
        Board *src_board = mzx_world->current_board;
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
        char robot_numbers[256];
        int robot_table_position;
        int current_id;
        int i;

        for(y = 0; y < height; y++)
        {
          for(x = 0; x < width; x++, offset++)
          {           
            current_id = level_id[offset];

            if((current_id == 123) || (current_id == 124))
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

            if((current_id == 122) || (current_id >= 125))
            {
              // Sensor, scroll, sign, or player
              // Put customblock fake
              fputc(5, output_file);
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
          Robot **robot_list = src_board->robot_list;

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
            save_robot(robot_list[robot_numbers[i]], output_file, savegame);
          }
        }

        break;
      }

      // Overlay/Vlayer
      case 1:
      case 2:
      {
        Board *src_board = mzx_world->current_board;
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
        Board *src_board = mzx_world->current_board;
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

int load_mzm(World *mzx_world, char *name, int start_x, int start_y,
 int mode, int savegame)
{
  FILE *input_file = fsafeopen(name, "rb");

  if(input_file)
  {
    char magic_string[5];
    int storage_mode;
    int width, height;
    int savegame_mode;
    int num_robots;
    int robots_location;

    fread(magic_string, 4, 1, input_file);
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
    {
      // Failure, abort
      fclose(input_file);
      return -1;
    }
        
    switch(mode)
    {
      // Write to board
      case 0:
      {
        Board *src_board = mzx_world->current_board;
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
        int src_id;

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
            int current_id;
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
                current_id = fgetc(input_file);               
                if((current_id == 123) || (current_id == 124))
                {
                  robot_x_locations[current_robot_loaded] = x + start_x;
                  robot_y_locations[current_robot_loaded] = y + start_y;
                  current_robot_loaded++;
                }

                src_id = level_id[offset];

                if(src_id == 122)
                  clear_sensor_id(src_board, level_param[offset]);
                else

                if((src_id == 126) || (src_id == 125))
                  clear_scroll_id(src_board, level_param[offset]);
                else

                if((src_id == 123) || (src_id == 124))
                  clear_robot_id(src_board, level_param[offset]);

                // Don't allow the player to be overwritten
                if(src_id != 127)
                {
                  level_id[offset] = current_id;
                  level_param[offset] = fgetc(input_file);
                  level_color[offset] = fgetc(input_file);
                  level_under_id[offset] = fgetc(input_file);
                  level_under_param[offset] = fgetc(input_file);
                  level_under_color[offset] = fgetc(input_file);
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
                current_id = fgetc(input_file);
                fseek(input_file, 5, SEEK_CUR);

                if((current_id == 123) || (current_id == 124))
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
              Robot *cur_robot;
              int current_x, current_y;
              int offset;
              int new_param;

              fseek(input_file, robots_location, SEEK_SET);

              for(i = 0; i < num_robots; i++)
              {
                cur_robot = load_robot_allocate(input_file, savegame_mode);
                current_x = robot_x_locations[i];
                current_y = robot_y_locations[i];

                if(current_x != -1)
                {
                  new_param = find_free_robot(src_board);
                  offset = current_x + (current_y * board_width);
                  
                  if(new_param != -1)
                  {
                    if(level_id[offset] != 127)
                    {
                      add_robot_name_entry(src_board, cur_robot,
                       cur_robot->robot_name);
                      src_board->robot_list[new_param] = cur_robot;
                      cur_robot->xpos = current_x;
                      cur_robot->ypos = current_y;
                      level_param[offset] = 
                       new_param;
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
                src_id = level_id[offset];

                if(src_id == 122)
                  clear_sensor_id(src_board, level_param[offset]);
                else

                if((src_id == 126) || (src_id == 125))
                  clear_scroll_id(src_board, level_param[offset]);
                else

                if((src_id == 123) || (src_id == 124))
                  clear_robot_id(src_board, level_param[offset]);

                // Don't allow the player to be overwritten
                if(src_id != 127)
                {
                  level_id[offset] = 5;
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
        Board *src_board = mzx_world->current_board;
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

  return 0;
}


