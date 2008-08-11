/* $Id$
 * MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick - exophase@adelphia.net
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

// Reconstructed robot editor. This is only a shell - the actual
// robot assembly/disassembly code is in rasm.cpp.

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "robo_ed.h"
#include "rasm.h"
#include "world.h"
#include "event.h"
#include "window.h"
#include "graphics.h"
#include "intake.h"
#include "param.h"
#include "char_ed.h"
#include "helpsys.h"
#include "edit.h"
#include "fsafeopen.h"

#define combine_colors(a, b)  \
  (a) | (b << 4)              \

char key_help[(81 * 3) + 1] =
{
  " F1:Help  F2:Color  F3:Char  F4:Param  F5:Char edit  F6-F10:Macros  (see Alt+O) \n"
  " Alt+Home/S:Mark start  Alt+End/E:Mark end  Alt+U:Unmark   Alt+B:Block Action   \n"
  " Alt+Ins:Paste  Alt+X:Export  Alt+I:Import  Alt+V:Verify  Ctrl+I/D/C:Valid Mark \n"
};

char key_help_hide[82] =
  "     Press Alt + H to view hotkey information.  Press F1 for extended help.     \n";

const char top_line_connect = 194;
const char bottom_line_connect = 193;
const char vertical_line = 179;
const char horizontal_line = 196;
const char top_char = 219;
const char bottom_char = 219;
const char bg_char = 32;
const char bg_color = 8;
const char bg_color_solid = combine_colors(0, 8);
const char top_color = 4;
const char bottom_color = 1;
const char line_color = combine_colors(15, 8);
const char top_text_color = combine_colors(15, 4);
const char bottom_text_color = combine_colors(15, 1);
const char top_highlight_color = combine_colors(14, 4);
const char current_line_color = combine_colors(11, 8);
const char mark_color = combine_colors(0, 7);

// Default colors for color coding:
// current line - 11
// immediates - 10
// characters - 14
// colors - color box or 2
// directions - 3
// things - 11
// params - 2
// strings - 14
// equalities - 0
// conditions - 15
// items - 11
// extras - 7
// commands and command fragments - 15

const int max_size = 65535;

char **copy_buffer = NULL;
int copy_buffer_lines;

char macros[5][64];

char search_string[256];
char replace_string[256];
char wrap_option[1] = { 1 };
char case_option[1] = { 0 };
int last_find_option = -1;

void robot_editor(World *mzx_world, Robot *cur_robot)
{
  int key;
  int i;
  int line_text_length, line_bytecode_length;
  char arg_types[32];
  char str_buffer[32], max_size_buffer[32];
  char *current_robot_pos = cur_robot->program + 1;
  char *next;
  char *object_code_position;
  int first_line_draw_position;
  int first_line_count_back;
  int new_line;
  int arg_count;
  int last_color = 0;
  int last_char = 0;
  robot_line base;
  robot_line *current_rline = NULL;
  robot_line *previous_rline = &base;
  robot_line *draw_rline;
  robot_line *next_line;
  int mark_current_line;
  int def_palette = mzx_world->conf.redit_dpalette;
  char previous_palette[16][3];
  char text_buffer[512], error_buffer[512];
  int current_line_color;

  robot_state rstate;

  rstate.current_line = 0;
  rstate.current_rline = &base;
  rstate.total_lines = 0;
  rstate.size = 2;
  rstate.max_size = 65535;
  rstate.include_ignores = mzx_world->conf.disassemble_extras;
  rstate.disassemble_base = mzx_world->conf.disassemble_base;
  rstate.default_invalid =
   (validity_types)(mzx_world->conf.default_invalid_status);
  rstate.ccodes = mzx_world->conf.color_codes;
  rstate.mark_mode = 0;
  rstate.mark_start = -1;
  rstate.mark_end = -1;
  rstate.mark_start_rline = NULL;
  rstate.mark_end_rline = NULL;
  rstate.show_line_numbers = 0;
  rstate.color_code = mzx_world->conf.color_coding_on;
  rstate.current_x = 0;
  rstate.active_macro = NULL;
  rstate.base = &base;
  rstate.command_buffer = rstate.command_buffer_space;

  base.previous = NULL;
  base.line_bytecode_length = -1;

  current_line_color = combine_colors(rstate.ccodes[0], bg_color);

  if(def_palette)
  {
    save_palette_mem(previous_palette, 16);
    default_palette();
    update_palette();
  }

  // Disassemble robots into lines
  do
  {
    new_line = disassemble_line(current_robot_pos, &next,
     text_buffer, error_buffer, &line_text_length, rstate.include_ignores,
     arg_types, &arg_count, rstate.disassemble_base);

    if(new_line)
    {
      current_rline = (robot_line *)malloc(sizeof(robot_line));

      line_bytecode_length = next - current_robot_pos;
      current_rline->line_text_length = line_text_length;
      current_rline->line_bytecode_length = line_bytecode_length;
      current_rline->line_text = (char *)malloc(line_text_length + 1);
      current_rline->line_bytecode = (char *)malloc(line_bytecode_length);
      current_rline->num_args = arg_count;
      current_rline->validity_status = valid;

      memcpy(current_rline->arg_types, arg_types, 20);
      memcpy(current_rline->line_bytecode, current_robot_pos,
       line_bytecode_length);
      memcpy(current_rline->line_text, text_buffer, line_text_length + 1);

      previous_rline->next = current_rline;
      current_rline->previous = previous_rline;

      rstate.total_lines++;

      current_robot_pos = next;
      previous_rline = current_rline;

      rstate.size += line_bytecode_length;
    }
    else
    {
      previous_rline->next = NULL;
    }
  } while(new_line);

  // Add a blank line to the end too if there aren't any lines
  if(!rstate.total_lines)
  {
    add_blank_line(&rstate, 1);
  }
  else
  {
    move_line_down(&rstate, 1);
  } 

  save_screen();
  fill_line(80, 0, 0, top_char, top_color);

  if(mzx_world->conf.redit_hhelp)
  {
    rstate.scr_hide_mode = 1;
    write_string(key_help_hide, 0, 24, bottom_text_color, 0);
    rstate.scr_line_start = 1;
    rstate.scr_line_middle = 12;
    rstate.scr_line_end = 23;
  }
  else
  {
    rstate.scr_hide_mode = 0;
    write_string(key_help, 0, 22, bottom_text_color, 0);
    rstate.scr_line_start = 2;
    rstate.scr_line_middle = 11;
    rstate.scr_line_end = 20;
  }

  sprintf(max_size_buffer, "%05d", rstate.max_size);
  strcpy(rstate.command_buffer, rstate.current_rline->line_text);

  do
  {
    draw_char(top_line_connect, line_color, 0, 1);
    draw_char(top_line_connect, line_color, 79, 1);
    draw_char(bottom_line_connect, line_color, 0, 21);
    draw_char(bottom_line_connect, line_color, 79, 21);

    for(i = 1; i < 79; i++)
    {
      draw_char(horizontal_line, line_color, i, 1);
      draw_char(horizontal_line, line_color, i, 21);
    }

    for(i = rstate.scr_line_start; i < rstate.scr_line_middle; i++)
    {
      draw_char(vertical_line, line_color, 0, i);
      draw_char(vertical_line, line_color, 79, i);
      fill_line(78, 1, i, bg_char, bg_color_solid);
    }

    for(i++; i <= rstate.scr_line_end; i++)
    {
      draw_char(vertical_line, line_color, 0, i);
      draw_char(vertical_line, line_color, 79, i);
      fill_line(78, 1, i, bg_char, bg_color_solid);
    }

    write_string("Line:", 1, 0, top_highlight_color, 0);
    sprintf(str_buffer, "%05d", rstate.current_line);
    write_string(str_buffer, 7, 0, top_text_color, 0);
    write_string("/", 12, 0, top_highlight_color, 0);
    sprintf(str_buffer, "%05d", rstate.total_lines);
    write_string(str_buffer, 13, 0, top_text_color, 0);
    write_string("Character:", 26, 0, top_highlight_color, 0);
    write_string("/", 40, 0, top_highlight_color, 0);
    write_string("Size:", 51, 0, top_highlight_color, 0);
    sprintf(str_buffer, "%05d", rstate.size);
    write_string(str_buffer, 57, 0, top_text_color, 0);
    write_string("/", 62, 0, top_highlight_color, 0);
    write_string(max_size_buffer, 63, 0, top_text_color, 0);

    // Now, draw the lines. Start with 9 back from the current.

    if(rstate.current_line > 
      (rstate.scr_line_middle - rstate.scr_line_start))
    {
      first_line_draw_position = rstate.scr_line_start;
      first_line_count_back = 
       rstate.scr_line_middle - rstate.scr_line_start;
    }
    else
    {
      // Or go back as many as we can
      first_line_draw_position = 1 +
       (rstate.scr_line_middle - rstate.current_line);
      first_line_count_back = rstate.current_line - 1;
    }

    draw_rline = rstate.current_rline;
    for(i = 0; i < first_line_count_back; i++)
    {
      draw_rline = draw_rline->previous;
    }

    // Now draw start - scr_end + 1 lines, or until bails
    for(i = first_line_draw_position; 
     (i <= rstate.scr_line_end) && draw_rline; i++)
    {
      if(i != rstate.scr_line_middle)
        display_robot_line(&rstate, draw_rline, i);

      draw_rline = draw_rline->next;
    }

    // Mark block selection area
    mark_current_line = 0;
    if(rstate.mark_mode)
    {
      int draw_start = rstate.current_line - 
       (rstate.scr_line_middle - rstate.scr_line_start);
      int draw_end = rstate.current_line + 
       (rstate.scr_line_end - rstate.scr_line_middle) + 1;

      if(rstate.mark_start <= draw_end)
      {
        if(rstate.mark_start >= draw_start)
        {
          draw_start = rstate.mark_start +
           rstate.scr_line_middle - rstate.current_line;
          draw_char('S', mark_color, 0, draw_start);
        }
        else
        {
          draw_start += rstate.scr_line_middle - rstate.current_line;
        }

        if(rstate.mark_end <= draw_end)
        {
          draw_end = rstate.mark_end + rstate.scr_line_middle - 
          rstate.current_line;
        }
        else
        {
          draw_end += rstate.scr_line_middle - rstate.current_line;
        }

        if(draw_start < rstate.scr_line_start)
        {
          draw_start = rstate.scr_line_start;
        }

        if(draw_end > rstate.scr_line_end)
        {
          draw_end = rstate.scr_line_end;
        }
        else
        {
          if((rstate.mark_start != rstate.mark_end) && 
           (draw_end >= rstate.scr_line_start))
            draw_char('E', mark_color, 0, draw_end);
        }

        if(draw_end >= rstate.scr_line_start)
        {
          for(i = draw_start; i <= draw_end; i++)
          {
            color_line(80, 0, i, mark_color);
          }

          if((draw_start <= rstate.scr_line_middle) &&
           (draw_end >= rstate.scr_line_middle))
            mark_current_line = 1;
        }
      }
    }

    update_screen();

    if(mark_current_line)
    {
      draw_char(bg_char, mark_color, 1, rstate.scr_line_middle);
      key = intake(rstate.command_buffer, 240, 2, 
       rstate.scr_line_middle, mark_color, 2, 0, 0, &rstate.current_x,
       1, rstate.active_macro);
    }
    else
    {
      draw_char(bg_char, bg_color_solid, 1, rstate.scr_line_middle);
      key = intake(rstate.command_buffer, 240, 2, 
       rstate.scr_line_middle, current_line_color, 2, 0, 0,
       &rstate.current_x, 1, rstate.active_macro);
    }

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_y >= rstate.scr_line_start) &&
       (mouse_y <= rstate.scr_line_end) && (mouse_x >= 2) &&
       (mouse_x <= 78))
      {
        move_and_update(&rstate, mouse_y - rstate.scr_line_middle);
        rstate.current_x = mouse_x - 2;
        warp_mouse(mouse_x, rstate.scr_line_middle);
      }
    }

    rstate.active_macro = NULL;

    switch(key)
    {
      case SDLK_UP:
      {
        move_and_update(&rstate, -1);
        break;
      }

      case SDLK_DOWN:
      {
        move_and_update(&rstate, 1);
        break;
      }

      case SDLK_PAGEUP:
      {
        move_and_update(&rstate, -9);
        break;
      }

      case SDLK_PAGEDOWN:
      {
        move_and_update(&rstate, 9);
        break;
      }

      case SDLK_BACKSPACE:
      {
        if(rstate.current_x == 0)
        {
          rstate.current_x = 10000;
          if(rstate.command_buffer[0] == 0)
          {
            update_current_line(&rstate);
            delete_current_line(&rstate, -1);
          }
          else
          {
            move_and_update(&rstate, -1);
          }
        }
        break;
      }

      case SDLK_DELETE:
      {

        if(rstate.command_buffer[0] == 0)
        {
          update_current_line(&rstate);
          delete_current_line(&rstate, 1);
        }
        break;
      }

      case SDLK_KP_ENTER:
      case SDLK_RETURN:
      {
        if(get_alt_status(keycode_SDL))
        {
          block_action(mzx_world, &rstate);
        }
        else

        if(rstate.current_x)
        {
          move_and_update(&rstate, 1);
        }
        else
        {
          add_blank_line(&rstate, -1);
        }
        break;
      }

      case SDLK_F1: // F1
      {
        m_show();
        help_system(mzx_world);
        break;
      }

      case SDLK_F2:
      {
        int new_color;

        if(def_palette)
        {
          load_palette_mem(previous_palette, 16);
          update_palette();
        }

        new_color = color_selection(last_color, 1);
        if(new_color >= 0)
        {
          char color_buffer[16];
          last_color = new_color;
          print_color(new_color, color_buffer);
          insert_string(rstate.command_buffer, color_buffer, &rstate.current_x);
        }

        if(def_palette)
        {
          default_palette();
          update_palette();
        }

        break;
      }

      case SDLK_F3:
      {
        int new_char = char_selection(last_char);

        if(new_char >= 0)
        {
          char char_buffer[16];
          last_char = new_char;
          sprintf(char_buffer, "%c", new_char);
          insert_string(rstate.command_buffer, char_buffer, &rstate.current_x);
        }
        break;
      }

      case SDLK_F4:
      {
        int start_x = rstate.current_x;
        search_entry_short *matched_arg;
        int end_x;
        char temp_char;

        if(!rstate.command_buffer[start_x])
          start_x--;

        while(start_x && (rstate.command_buffer[start_x] == ' '))
          start_x--;

        end_x = start_x + 1;
        temp_char = rstate.command_buffer[end_x];

        while(start_x && (rstate.command_buffer[start_x] != ' '))
          start_x--;

        if(start_x)
          start_x++;

        temp_char = rstate.command_buffer[end_x];
        rstate.command_buffer[end_x] = 0;
        matched_arg = find_argument(rstate.command_buffer + start_x);
        rstate.command_buffer[end_x] = temp_char;

        if(matched_arg && (matched_arg->type == S_THING) &&
         (matched_arg->offset < 122))
        {
          int new_param = edit_param(mzx_world, matched_arg->offset, -1);
          if(new_param >= 0)
          {
            char param_buffer[16];
            sprintf(param_buffer, " p%02x", new_param);
            insert_string(rstate.command_buffer, param_buffer, &rstate.current_x);
          }
        }
        break;
      }

      case SDLK_F5:
      {
        int char_edited;
        char char_buffer[14];
        char char_string_buffer[64];
        char *char_string_buffer_position = char_string_buffer;

        if(!get_screen_mode())
          char_edited = char_editor(mzx_world);
        else
          char_edited = smzx_char_editor(mzx_world);

        ec_read_char(char_edited, char_buffer);

        for(i = 0; i < 13; i++, char_string_buffer_position += 4)
        {
          sprintf(char_string_buffer_position, "$%02x ", char_buffer[i]);
        }
        sprintf(char_string_buffer_position, "$%02x", char_buffer[i]);

        insert_string(rstate.command_buffer, char_string_buffer, &rstate.current_x);
        break;
      }

      case SDLK_F6:
      {
        rstate.active_macro = macros[0];
        break;
      }

      case SDLK_F7:
      {
        rstate.active_macro = macros[1];
        break;
      }

      case SDLK_F8:
      {
        rstate.active_macro = macros[2];
        break;
      }

      case SDLK_F9:
      {
        rstate.active_macro = macros[3];
        break;
      }

      case SDLK_F10:
      {
        rstate.active_macro = macros[4];
        break;
      }

      case SDLK_v:
      {
        if(get_alt_status(keycode_SDL))
        {
          update_current_line(&rstate);
          validate_lines(mzx_world, &rstate, 1);
          update_current_line(&rstate);        
        }
        break;
      }

      case SDLK_ESCAPE:
      {
        update_current_line(&rstate);
        if(validate_lines(mzx_world, &rstate, 0))
          key = 0;

        update_current_line(&rstate);
        break;
      }

      // Mark start/end
      case SDLK_s:
      case SDLK_e:
      case SDLK_HOME:
      case SDLK_END:
      {
        if(get_alt_status(keycode_SDL))
        {
          int mark_switch;

          update_current_line(&rstate);

          if(rstate.mark_mode == 2)
          {
            if((key == SDLK_HOME) || (key == SDLK_s))
              mark_switch = 0;
            else
              mark_switch = 1;
          }
          else
          {
            mark_switch = rstate.mark_mode;
            rstate.mark_mode++;
          }

          switch(mark_switch)
          {
            case 0:
            {
              // Always mark the start
              rstate.mark_start = rstate.current_line;
              rstate.mark_start_rline = rstate.current_rline;
              if(rstate.mark_mode == 1)
              {
                // If this is the real start, mark the end too
                rstate.mark_end = rstate.current_line;
                rstate.mark_end_rline = rstate.current_rline;
              }
              break;
            }

            case 1:
            {
              // Always mark the end
              rstate.mark_end = rstate.current_line;
              rstate.mark_end_rline = rstate.current_rline;
              break;
            }
          }

          // We have more than a start, so possibly swap
          if(rstate.mark_mode && (rstate.mark_start > rstate.mark_end))
          {
            int mark_swap;
            robot_line *mark_swap_rline;

            mark_swap = rstate.mark_start;
            rstate.mark_start = rstate.mark_end;
            rstate.mark_end = mark_swap;
            mark_swap_rline = rstate.mark_start_rline;
            rstate.mark_start_rline = rstate.mark_end_rline;
            rstate.mark_end_rline = mark_swap_rline;
          }
        }
        break;
      }

      // Block action menu
      case SDLK_b:
      {
        if(get_alt_status(keycode_SDL))
        {
          block_action(mzx_world, &rstate);
        }
        break;
      }

      case SDLK_g:
      {
        if(get_ctrl_status(keycode_SDL))
        {
          char line_number[16];

          line_number[0] = 0;

          save_screen();
          draw_window_box(18, 12, 47, 14, EC_DEBUG_BOX, EC_DEBUG_BOX_DARK,
           EC_DEBUG_BOX_CORNER, 1, 1);
          write_string("Goto line number:", 20, 13, EC_DEBUG_LABEL, 0);

          if(intake(line_number, 7, 38, 13, 15, 1, 0) != SDLK_ESCAPE)
          {
            int line = strtol(line_number, NULL, 10);
            goto_line(&rstate, line);
          }
          restore_screen();
        }
        break;
      }

      case SDLK_l:
      {
        if(get_ctrl_status(keycode_SDL))
          rstate.show_line_numbers ^= 1;

        break;
      }

      case SDLK_d:
      {
        if(rstate.current_rline->validity_status != valid)
        {
          rstate.current_rline->validity_status = invalid_discard;
          update_current_line(&rstate);
        }
    
        break;
      }

      case SDLK_c:
      {
        if(rstate.current_rline->validity_status != valid)
        {
          rstate.current_rline->validity_status = invalid_comment;
          update_current_line(&rstate);
        }
        else

        if(rstate.command_buffer[0] != '.')
        {
          char comment_buffer[512];
          char current_char;
          char *in_position = rstate.command_buffer;
          char *out_position = comment_buffer + 3;

          comment_buffer[0] = '.';
          comment_buffer[1] = ' ';
          comment_buffer[2] = '"';

          do
          {
            current_char = *in_position;
            if(current_char == '"')
            {
              *out_position = '\\';
              out_position++;
            }

            *out_position = current_char;
            out_position++;
            in_position++;
          } while(current_char);

          *(out_position - 1) = '"';
          *out_position = 0;

          strcpy(rstate.command_buffer, comment_buffer);
        }
        else

        if(get_ctrl_status(keycode_SDL) &&
         (rstate.command_buffer[0] == '.') && (rstate.command_buffer[1] == ' ') &&
         (rstate.command_buffer[2] == '"') &&
         (rstate.command_buffer[strlen(rstate.command_buffer) - 1] == '"'))
        {
          char uncomment_buffer[512];
          char current_char;
          char *in_position = rstate.command_buffer + 3;
          char *out_position = uncomment_buffer;

          do
          {
            current_char = *in_position;
            if((current_char == '\\') && (in_position[1] == '"'))
            {
              current_char = '"';
              in_position++;
            }

            *out_position = current_char;
            out_position++;
            in_position++;
          } while(current_char);

          *(out_position - 2) = 0;

          strcpy(rstate.command_buffer, uncomment_buffer);
        }

        break;
      }

      case SDLK_p:
      case SDLK_INSERT:
      {
        if(get_alt_status(keycode_SDL) && copy_buffer)
        {
          paste_buffer(&rstate);
        }
        break;
      }

      // Import file
      case SDLK_i:
      {
        if(get_alt_status(keycode_SDL))
        {
          import_block(mzx_world, &rstate);
        }
        else

        if(get_ctrl_status(keycode_SDL))
        {
          if(rstate.current_rline->validity_status != valid)
          {
            rstate.current_rline->validity_status = invalid_uncertain;
          }
        }
        break;
      }

      // Export file
      case SDLK_x:
      {
        if(get_alt_status(keycode_SDL))
        {
          if(rstate.mark_mode)
          {
            export_block(mzx_world, &rstate, 1);
          }
          else
          {
            export_block(mzx_world, &rstate, 0);
          }
        }
        break;
      }

      // Options
      case SDLK_o:
      {
        if(get_alt_status(keycode_SDL))
          edit_settings(mzx_world);

        break;
      }

      // Unmark
      case SDLK_u:
      {
        if(get_alt_status(keycode_SDL))
          rstate.mark_mode = 0;

        break;
      }

      // Hide
      case SDLK_h:
      {
        if(get_alt_status(keycode_SDL))
        {
          if(rstate.scr_hide_mode)
          {
            rstate.scr_hide_mode = 0;
            rstate.scr_line_start = 2;
            rstate.scr_line_middle = 11;
            rstate.scr_line_end = 20;
            write_string(key_help, 0, 22, bottom_text_color, 0);
          }
          else
          {
            rstate.scr_hide_mode = 1;
            rstate.scr_line_start = 1;
            rstate.scr_line_middle = 12;
            rstate.scr_line_end = 23;
            write_string(key_help_hide, 0, 24, bottom_text_color, 0);
          }
        }         
        break;
      }

      case SDLK_f:
      {
        if(get_ctrl_status(keycode_SDL))
          find_replace_action(mzx_world, &rstate);

        break;
      }

      case SDLK_r:
      {
        if(get_ctrl_status(keycode_SDL))
        {
          switch(last_find_option)
          {
            case 0:
            {
              // Find next
              int l_pos;
              int l_num = find_string(&rstate, search_string, wrap_option[0],
               &l_pos, case_option[0]);
      
              if(l_num != -1)
              {
                goto_line(&rstate, l_num);
                rstate.current_x = l_pos;
              }
              break;
            }

            case 1:
            case 2:
            {
              // Find and replace next
              int l_pos;
              int l_num = find_string(&rstate, search_string, wrap_option[0],
               &l_pos, case_option[0]);
        
              if(l_num != -1)
              {
                goto_line(&rstate, l_num);
                rstate.current_x = l_pos;
                replace_current_line(&rstate, l_pos, search_string,
                 replace_string);
              }
              break;
            }
          }
        }
        break;
      }
    }
  } while(key != SDLK_ESCAPE);

  // Package time
  reallocate_robot(cur_robot, rstate.size);

  current_rline = base.next;
  object_code_position = cur_robot->program;
  object_code_position[0] = 0xFF;
  object_code_position++;

  while(current_rline)
  {
    if((current_rline->next != NULL) || (current_rline->line_text[0]))
    {
      if(current_rline->validity_status == invalid_comment)
      {
        object_code_position[0] = current_rline->line_text_length + 3;
        object_code_position[1] = 107;
        object_code_position[2] = current_rline->line_text_length + 1;

        strcpy(object_code_position + 3, current_rline->line_text);
        object_code_position[current_rline->line_text_length + 4] =
         current_rline->line_text_length + 3;
        object_code_position += current_rline->line_text_length + 5;
      }
      else

      if(current_rline->validity_status == valid)
      {
        memcpy(object_code_position, current_rline->line_bytecode,
         current_rline->line_bytecode_length);
        object_code_position += current_rline->line_bytecode_length;
      }
    }
    else
    {
      // Get rid of trailing three bytes if present
      rstate.size -= current_rline->line_bytecode_length;
      reallocate_robot(cur_robot, rstate.size);
      object_code_position = cur_robot->program + rstate.size - 1;
    }

    free(current_rline->line_bytecode);
    free(current_rline->line_text);
    next_line = current_rline->next;
    free(current_rline);
    current_rline = next_line;
  }

  object_code_position[0] = 0;

  if(rstate.size > 2)
  {
    cur_robot->used = 1;
    cur_robot->cur_prog_line = 1;
    cur_robot->label_list = cache_robot_labels(cur_robot, &(cur_robot->num_labels));
  }

  if(def_palette)
  {
    load_palette_mem(previous_palette, 16);
    update_palette();
  }

  restore_screen();
}

void insert_string(char *dest, char *string, int *position)
{
  int n_pos = *position;
  int remainder = strlen(dest + n_pos) + 1;
  int insert_length = strlen(string);

  memmove(dest + n_pos + insert_length, dest + n_pos, remainder);
  memcpy(dest + n_pos, string, insert_length);
  *position = n_pos + insert_length;
}

void move_line_up(robot_state *rstate, int count)
{
  int i;

  for(i = 0; (i < count); i++)
  {
    if(rstate->current_rline->previous->line_bytecode_length == -1)
      break;

    rstate->current_rline = rstate->current_rline->previous;
  }

  rstate->current_line -= i;
}

void move_line_down(robot_state *rstate, int count)
{
  int i;

  for(i = 0; (i < count); i++)
  {
    if(rstate->current_rline->next == NULL)
    {
      // Add a new line if the last line isn't blank.
      if(rstate->current_rline->line_text[0])
        add_blank_line(rstate, 1);

      break;
    }

    rstate->current_rline = rstate->current_rline->next;
  }

  rstate->current_line += i;
}

void display_robot_line(robot_state *rstate, robot_line *current_rline,
 int y)
{
  int i;
  int x = 2;
  int current_color, current_arg;
  int color_code = rstate->color_code;
  char *color_codes = rstate->ccodes;
  char temp_buffer[512];
  char temp_char;
  char *line_pos = current_rline->line_text;
  int arg_length;

  if(current_rline->line_text[0] != 0)
  {
    if(current_rline->validity_status != valid)
    {
      current_color =
       combine_colors(color_codes[S_CMD + current_rline->validity_status + 1],
       bg_color);

      if(strlen(current_rline->line_text) > 76)
      {
        temp_char = current_rline->line_text[76];
        current_rline->line_text[76] = 0;
        write_string(current_rline->line_text, x, y, current_color, 0);
        current_rline->line_text[76] = temp_char;
      }
      else
      {
        write_string(current_rline->line_text, x, y, current_color, 0);
      }
    }
    else
    {
      arg_length = 0;

      if(!color_code)
      {
        current_color =
         combine_colors(color_codes[S_STRING + 1], bg_color);
      }

      if(color_code)
        current_color = combine_colors(color_codes[S_CMD + 1], bg_color);

      get_word(temp_buffer, line_pos, ' ');
      arg_length = strlen(temp_buffer) + 1;

      write_string(temp_buffer, x, y, current_color, 0);

      line_pos += arg_length;
      x += arg_length;

      for(i = 0; i < current_rline->num_args; i++)
      {
        current_arg = current_rline->arg_types[i];

        if(current_arg == S_STRING)
        {
          temp_buffer[0] = '"';
          get_word(temp_buffer + 1, line_pos + 1, '"');
          arg_length = strlen(temp_buffer);
          temp_buffer[arg_length] = '"';
          temp_buffer[arg_length + 1] = 0;
          arg_length += 2;
        }
        else

        if(current_arg == S_CHARACTER)
        {
          memcpy(temp_buffer, line_pos, 3);
          temp_buffer[3] = 0;
          arg_length = 4;
        }
        else
        {
          get_word(temp_buffer, line_pos, ' ');
          arg_length = strlen(temp_buffer) + 1;
        }

        if((current_arg == S_COLOR) && (color_codes[current_arg + 1] == 255))
        {
          unsigned int color = get_color(line_pos);
          draw_color_box(color & 0xFF, color >> 8, x, y);
        }
        else
        {
          if(color_code)
          {
            current_color =
             combine_colors(color_codes[current_arg + 1], bg_color);
          }

          if((x + arg_length) > 78)
          {
            temp_buffer[78 - x] = 0;
            write_string(temp_buffer, x, y, current_color, 0);
            break;
          }

          write_string(temp_buffer, x, y, current_color, 0);
        }
        line_pos += arg_length;
        x += arg_length;
      }
    }
  }
}

void move_and_update(robot_state *rstate, int count)
{
  update_current_line(rstate);
  if(count < 0)
  {
    move_line_up(rstate, -count);
  }
  else
  {
    move_line_down(rstate, count);
  }

  strcpy(rstate->command_buffer, rstate->current_rline->line_text);
}


void update_current_line(robot_state *rstate)
{
  char bytecode_buffer[512];
  char error_buffer[512];
  char new_command_buffer[512];
  char arg_types[32];
  robot_line *current_rline = rstate->current_rline;
  char *command_buffer = rstate->command_buffer;
  int arg_count;
  int bytecode_length =
   assemble_line(command_buffer, bytecode_buffer, error_buffer,
   arg_types, &arg_count);
  int last_bytecode_length = current_rline->line_bytecode_length;
  int current_size = rstate->size;
  validity_types use_type = rstate->default_invalid;

  if((bytecode_length != -1) && 
   (current_size + bytecode_length - last_bytecode_length) <=
   rstate->max_size)
  {
    char *next;
    int line_text_length;

    disassemble_line(bytecode_buffer, &next, new_command_buffer,
     error_buffer, &line_text_length, rstate->include_ignores, 
     arg_types, &arg_count, rstate->disassemble_base);
  
    if(line_text_length < 241)
    {
      current_rline->line_text_length = line_text_length;
      current_rline->line_text =
       (char *)realloc(current_rline->line_text, line_text_length + 1);

      current_rline->line_bytecode_length = bytecode_length;
      current_rline->line_bytecode =
       (char *)realloc(current_rline->line_bytecode, bytecode_length);
      current_rline->num_args = arg_count;
      current_rline->validity_status = valid;
  
      memcpy(current_rline->arg_types, arg_types, 20);
      strcpy(current_rline->line_text, new_command_buffer);
      memcpy(current_rline->line_bytecode, bytecode_buffer, bytecode_length);
  
      rstate->size = current_size + bytecode_length - last_bytecode_length;
    }
    else
    {
      current_rline->line_text_length = 240;
      current_rline->line_text =
       (char *)realloc(current_rline->line_text, 240);
      memcpy(current_rline->line_text, new_command_buffer, 240);
      current_rline->line_text[240] = 0;

      if((current_rline->validity_status != valid) &&
       (current_rline->validity_status != invalid_comment))
      {
        use_type = current_rline->validity_status;
      }

      current_rline->line_bytecode_length = 0;
      current_rline->validity_status = use_type;
      rstate->size = current_size - last_bytecode_length;
    }
  }
  else
  {
    int line_text_length;

    command_buffer[240] = 0;
    line_text_length = strlen(command_buffer);

    if(current_rline->validity_status != valid)
      use_type = current_rline->validity_status;

    current_rline->line_text =
     (char *)realloc(current_rline->line_text, line_text_length + 1);

    if(use_type == invalid_comment &&
     (current_size + (line_text_length + 5) - last_bytecode_length) >
     rstate->max_size)
    {
      use_type = invalid_uncertain;   
    }

    if((use_type == invalid_comment) && (line_text_length > 236))
    {
      line_text_length = 236;
      command_buffer[236] = 0;
    }
      
    current_rline->line_text_length = line_text_length;
    strcpy(current_rline->line_text, command_buffer);
    
    if(use_type != invalid_comment)
    {
      current_rline->line_bytecode_length = 0;
      current_rline->validity_status = use_type;
      rstate->size = current_size - last_bytecode_length;
    }
    else
    {
      current_rline->validity_status = invalid_comment;
      current_rline->line_bytecode_length = line_text_length + 5;
      rstate->size =
       current_size + (line_text_length + 5) - last_bytecode_length;
    }
  }
}

void add_blank_line(robot_state *rstate, int relation)
{
  if(rstate->size + 3 < rstate->max_size)
  {
    robot_line *new_rline = (robot_line *)malloc(sizeof(robot_line));
    robot_line *current_rline = rstate->current_rline;
    new_rline->line_text_length = 0;
    new_rline->line_text = (char *)malloc(1);
    new_rline->line_bytecode = (char *)malloc(3);
    new_rline->line_text[0] = 0;
    new_rline->line_bytecode[0] = 1;
    new_rline->line_bytecode[1] = 47;
    new_rline->line_bytecode[2] = 1;
    new_rline->line_bytecode_length = 3;
    new_rline->validity_status = valid;
    int current_line = rstate->current_line;
  
    rstate->total_lines++;
    rstate->size += 3;
  
    if(relation > 0)
    {
      new_rline->next = current_rline->next;
      new_rline->previous = current_rline;
  
      if(current_rline->next)
        current_rline->next->previous = new_rline;
  
      current_rline->next = new_rline;  

      rstate->current_rline = new_rline;

      if(rstate->mark_mode)
      {
        if(rstate->mark_start > current_line)
          rstate->mark_start++;

        if(rstate->mark_end > current_line)
          rstate->mark_end++;
      }
    }
    else
    {
      new_rline->next = current_rline;
      new_rline->previous = current_rline->previous;
  
      current_rline->previous->next = new_rline;
      current_rline->previous = new_rline;

      if(rstate->mark_mode)
      {
        if(rstate->mark_start >= current_line)
          rstate->mark_start++;

        if(rstate->mark_end >= current_line)
          rstate->mark_end++;
      }
    }

    rstate->current_line = current_line + 1;
  }
}

void delete_current_line(robot_state *rstate, int move)
{
  if(rstate->total_lines != 1)
  {
    robot_line *current_rline = rstate->current_rline;
    int last_size = current_rline->line_bytecode_length;
    robot_line *next = current_rline->next;
    robot_line *previous = current_rline->previous;

    previous->next = next;

    if(next)
      next->previous = previous;

    free(current_rline->line_text);
    free(current_rline->line_bytecode);
    free(current_rline);
  
    rstate->size -= last_size;

    if(rstate->mark_mode)
    {
      if(rstate->mark_start_rline == current_rline)
        rstate->mark_start_rline = current_rline->next;

      if(rstate->mark_end_rline == current_rline)
        rstate->mark_end_rline = current_rline->previous;

      if(rstate->mark_start > rstate->current_line)
        rstate->mark_start--;

      if(rstate->mark_end >= rstate->current_line)
        rstate->mark_end--;

      if(rstate->mark_start > rstate->mark_end)
        rstate->mark_mode = 0;
    }

    if(move > 0)
    {
      if(next)
      {
        rstate->current_rline = next;
      }
      else
      {
        rstate->current_rline = previous;
        rstate->current_line--;
      }
    }
    else
    {
      if(rstate->current_line != 1)
      {
        rstate->current_rline = previous;
        rstate->current_line--;
      }
      else
      {
        rstate->current_rline = next; 
      }
    }

    strcpy(rstate->command_buffer, rstate->current_rline->line_text);

    rstate->total_lines--;
  }
}

void add_line(robot_state *rstate)
{
  robot_line *new_rline = (robot_line *)malloc(sizeof(robot_line));
  robot_line *current_rline = rstate->current_rline;
  new_rline->line_text = NULL;
  new_rline->line_bytecode = NULL;
  new_rline->line_bytecode_length = 0;
  new_rline->validity_status = valid;

  rstate->current_rline = new_rline;
  update_current_line(rstate);
  rstate->current_rline = current_rline;

  new_rline->next = current_rline;
  new_rline->previous = current_rline->previous;

  current_rline->previous->next = new_rline;
  current_rline->previous = new_rline;

  if(rstate->mark_mode)
  {
    if(rstate->mark_start >= rstate->current_line)
      rstate->mark_start++;

    if(rstate->mark_end >= rstate->current_line)
      rstate->mark_end++;
  }

  rstate->total_lines++;
  rstate->current_line++;
}

#define VALIDATE_ELEMENTS 60
#define MAX_ERRORS 256

// This will only check for errors after current_rline, so the
// base should be passed.

int validate_lines(World *mzx_world, robot_state *rstate,
 int show_none)
{
  // 70x5-21 square that displays up to 13 error messages,
  // and up to 48 buttons for delete/comment out/ignore. In total,
  // the elements are text display to tell how many erroroneous
  // lines there are, 13 text bars to display the error message
  // and validity status, 13 delete buttons, 13 comment buttons,
  // 13 ignore buttons, a next button, a previous button,
  // and yes/no buttons. There can thus be up to 57 or so elements.
  // The first one is always text, and the second are always
  // yes/no buttons. The rest are variable depending on how
  // many errors there are. The text button positions cannot be
  // determined until the number of errors is.
  // Doesn't take any more errors after the number of erroneous
  // lines exceeds MAX_ERRORS.

  char information[64];
  char di_types[VALIDATE_ELEMENTS] = { DE_TEXT, DE_BUTTON, DE_BUTTON };
  char di_xs[VALIDATE_ELEMENTS] = { 5, 28, 38 };
  char di_ys[VALIDATE_ELEMENTS] = { 2, 18, 18 };
  char *di_strs[VALIDATE_ELEMENTS] = { information, "OK", "Cancel" };
  int di_p1s[VALIDATE_ELEMENTS] = { 0, 0, -1 };
  int di_p2s[VALIDATE_ELEMENTS] = { 0 };
  void *di_storage[VALIDATE_ELEMENTS] = { NULL };
  int start_line = 0;
  int num_errors = 0;
  char error_messages[MAX_ERRORS][64];
  char null_buffer[256];
  robot_line *line_pointers[MAX_ERRORS];
  robot_line *current_rline = rstate->base->next;
  validity_types validity_options[MAX_ERRORS];
  int redo = 1;
  int element_pos;
  int errors_shown;
  int multi_pages = 0;
  int line_number = 0;
  int dialog_result;
  int num_ignore = 0;
  int current_size = rstate->size;
  int i;

  dialog di =
  {
    5, 2, 74, 22, "Command summary", 3, di_types, di_xs, di_ys,
    di_strs, di_p1s, di_p2s, di_storage, 0
  };

  // First, collect the number of errors, and process error messages
  // by calling assemble_line.

  while(current_rline != NULL)
  {
    if(current_rline->validity_status != valid)
    {
      memset(error_messages[num_errors], ' ', 64);
      sprintf(error_messages[num_errors], "%05d: ", line_number + 1);
      assemble_line(current_rline->line_text, null_buffer,
       error_messages[num_errors] + 7, NULL, NULL);
      error_messages[num_errors][strlen(error_messages[num_errors])] = ' ';
      line_pointers[num_errors] = current_rline;
      validity_options[num_errors] = current_rline->validity_status;

      if(current_rline->validity_status == invalid_uncertain)
        num_ignore++;

      if(num_errors == MAX_ERRORS)
        break;

      num_errors++;
    }
    current_rline = current_rline->next;
    line_number++;
  }

  // The button return messages are as such:
  // 0 - ok
  // -1 - cancel
  // 1 - previous
  // 2 - next
  // 100-113 - ignore line
  // 200-213 - delete line
  // 300-313 - ignore line

  if(num_errors > 12)
  {
    multi_pages = 1;
  }

  if(num_ignore || show_none)
  {
    do
    {
      errors_shown = num_errors - start_line;

      if(errors_shown > 12)
        errors_shown = 12;

      for(i = 0, element_pos = 3; i < errors_shown; i++,
       element_pos += 4)
      {
        // Text line
        di_types[element_pos] = DE_TEXT;

        // Validation buttons
        di_types[element_pos + 1] = DE_BUTTON;
        di_types[element_pos + 2] = DE_BUTTON;
        di_types[element_pos + 3] = DE_BUTTON;

        di_strs[element_pos] = error_messages[start_line + i];
        switch(validity_options[start_line + i])
        {
          case invalid_uncertain:
          {
            sprintf(error_messages[start_line + i] + 44, " (ignore)");
            break;
          }

          case invalid_discard:
          {
            sprintf(error_messages[start_line + i] + 44, " (delete)");
            break;
          }

          case invalid_comment:
          {
            sprintf(error_messages[start_line + i] + 44, "(comment)");
            break;
          }

          default:
          {
            break;
          }
        }

        di_strs[element_pos + 1] = "I";
        di_strs[element_pos + 2] = "D";
        di_strs[element_pos + 3] = "C";

        di_xs[element_pos] = 2;
        di_xs[element_pos + 1] = 56;
        di_xs[element_pos + 2] = 60;
        di_xs[element_pos + 3] = 64;

        di_ys[element_pos] = i + 4;
        di_ys[element_pos + 1] = i + 4;
        di_ys[element_pos + 2] = i + 4;
        di_ys[element_pos + 3] = i + 4;

        di_p1s[element_pos + 1] = start_line + i + 100;
        di_p1s[element_pos + 2] = start_line + i + 200;
        di_p1s[element_pos + 3] = start_line + i + 300;
      }

      if(multi_pages)
      {
        if(start_line)
        {
          di_types[element_pos] = DE_BUTTON;
          di_strs[element_pos] = "Previous";
          di_xs[element_pos] = 5;
          di_ys[element_pos] = 17;
          di_p1s[element_pos] = 1;
          element_pos++;
        }

        if((start_line + 12) < num_errors)
        {
          di_types[element_pos] = DE_BUTTON;
          di_strs[element_pos] = "Next";
          di_xs[element_pos] = 61;
          di_ys[element_pos] = 17;
          di_p1s[element_pos] = 2;
          element_pos++;
        }

        sprintf(information, "%d errors found; displaying %d through %d.\n", num_errors,
         start_line + 1, start_line + errors_shown);
      }
      else
      {
        if(num_errors == 1)
        {
          sprintf(information, "1 error found.");
        }
        else

        if(!num_errors)
        {
          sprintf(information, "No errors found.");
        }
        else
        {
          sprintf(information, "%d errors found.", num_errors);
        }
      }

      di.num_elements = element_pos;

      dialog_result = run_dialog(mzx_world, &di, 0, 0);

      if(dialog_result == -1)
      {
        // Cancel - bails
        redo = 0;
      }
      else

      if(dialog_result == 0)
      {
        // Okay, update validity options
        redo = 0;
        for(i = 0; i < num_errors; i++)
        {
          (line_pointers[i])->validity_status = validity_options[i];

          if(validity_options[i] == invalid_comment)
          {
            (line_pointers[i])->line_bytecode_length =
             (line_pointers[i])->line_text_length + 5;
          }
          else
          {
            (line_pointers[i])->line_bytecode_length = 0;
            (line_pointers[i])->line_text_length = 0;
          }
        }

        rstate->size = current_size;
      }
      else

      if((dialog_result >= 100) && (dialog_result < 200))
      {
        // If the last is comment, adjust size
        if(validity_options[dialog_result - 100] == invalid_comment)
        {
          current_size -=
           (line_pointers[dialog_result - 100])->line_text_length + 5;
        }

        // Make it ignore
        validity_options[dialog_result - 100] = invalid_uncertain;
      }
      else

      if((dialog_result >= 200) && (dialog_result < 300))
      {
        // If the last is comment, adjust size
        if(validity_options[dialog_result - 200] == invalid_comment)
        {
          current_size -=
           (line_pointers[dialog_result - 200])->line_text_length + 5;
        }

        // Make it delete
        validity_options[dialog_result - 200] = invalid_discard;
      }
      else

      if((dialog_result >= 300) && (dialog_result < 400))
      {
        if(current_size +
         ((line_pointers[dialog_result - 300])->line_text_length + 5) <
         MAX_OBJ_SIZE)
        {
          // If the last is not comment, adjust size
          if(validity_options[dialog_result - 300] != invalid_comment)
          {
            current_size +=
             (line_pointers[dialog_result - 300])->line_text_length + 5;
            
          }

          // Make it comment
          validity_options[dialog_result - 300] = invalid_comment;
        }
      }
      else

      if(dialog_result == 1)
      {
        start_line -= 12;
      }
      else

      if(dialog_result == 2)
      {
        start_line += 12;
      }
    } while(redo);
  }

  return num_ignore;
}

int block_menu(World *mzx_world)
{
  char di_types[3] = { DE_RADIO, DE_BUTTON, DE_BUTTON };
  char di_xs[3] = { 2, 5, 15 };
  char di_ys[3] = { 2, 7, 7 };

  char *di_strs[3] =
  {
    "Copy block\n"
    "Cut block\n"
    "Clear block\n"
    "Export block\n",
    "OK", "Cancel"
  };

  int di_p1s[3] = { 4, 0, -1 };
  int di_p2s[1] = { 21 };
  int block_op = 0;
  void *di_storage[1] = { &block_op };

  dialog di =
  {
    26, 6, 53, 15, "Choose block command", 3, di_types, di_xs,
    di_ys, di_strs, di_p1s, di_p2s, di_storage, 0
  };

  if(run_dialog(mzx_world, &di, 0, 0) == -1)
    return -1;
  else
    return block_op;
}

void copy_block_to_buffer(robot_state *rstate)
{
  robot_line *current_rline = rstate->mark_start_rline;
  int num_lines = (rstate->mark_end - rstate->mark_start) + 1;
  int line_length;
  int i;

  // First, if there's something already there, clear all the lines and
  // the buffer.
  if(copy_buffer)
  {
    for(i = 0; i < copy_buffer_lines; i++)
    {
      free(copy_buffer[i]);
    }

    free(copy_buffer);
  }

  copy_buffer = (char **)malloc(sizeof(char *) * num_lines);
  copy_buffer_lines = num_lines;

  for(i = 0; i < num_lines; i++)
  {
    line_length = current_rline->line_text_length + 1;
    copy_buffer[i] = (char *)malloc(line_length);
    memcpy(copy_buffer[i], current_rline->line_text, line_length);
    current_rline = current_rline->next;
  }
}

void paste_buffer(robot_state *rstate)
{
  int i;

  for(i = 0; i < copy_buffer_lines; i++)
  {
    rstate->command_buffer = copy_buffer[i];
    add_line(rstate);
  }

  rstate->command_buffer = rstate->command_buffer_space;
}

void clear_block(robot_state *rstate)
{
  robot_line *current_rline = rstate->mark_start_rline;
  robot_line *line_after = rstate->mark_end_rline->next;
  robot_line *line_before = current_rline->previous;
  robot_line *next_rline;
  int num_lines = rstate->mark_end - rstate->mark_start + 1;
  int i;

  for(i = 0; i < num_lines; i++)
  {
    next_rline = current_rline->next;

    rstate->size -= current_rline->line_bytecode_length;
    free(current_rline->line_text);
    free(current_rline->line_bytecode);
    free(current_rline);

    current_rline = next_rline;
  }

  line_before->next = line_after;
  if(line_after)
    line_after->previous = line_before;

  rstate->total_lines -= num_lines;

  if(!rstate->total_lines)
  {
    rstate->current_line = 0;
    rstate->current_rline = rstate->base;
    add_blank_line(rstate, 1);
  }
  else
  {
    if(rstate->current_line >= rstate->mark_start)
    {
      if(rstate->current_line <= rstate->mark_end)
      {
        // Current line got swallowed.
        if(line_after)
        {
          rstate->current_line = rstate->mark_start;
          rstate->current_rline = line_after;
        }
        else
        {
          rstate->current_line = rstate->mark_start - 1;
          rstate->current_rline = line_before;
        }
      }
      else
      {
        rstate->current_line -= num_lines;
      }
    }
  }

  strcpy(rstate->command_buffer, rstate->current_rline->line_text);
}

void export_block(World *mzx_world, robot_state *rstate,
 int region_default)
{
  robot_line *current_rline;
  robot_line *end_rline;

  char di_types[6] = { DE_INPUT, DE_RADIO, DE_BUTTON, DE_BUTTON, DE_RADIO };
  char di_xs[5] = { 5, 31, 15, 31, 5 };
  char di_ys[5] = { 2, 4, 7, 7, 4 };

  char *di_strs[5] =
  {
    "Export robot as: ",
    "Text\n"
    "Bytecode",
    "OK", "Cancel",
    "Entire robot\n"
    "Current block"
  };

  int di_p1s[5] = { 32, 2, 0, -1, 2 };
  int di_p2s[5] = { 192, 8, 0, 0, 13 };
  int export_region = region_default;
  int export_type = 0;
  char export_name[64];
  void *di_storage[5] = { export_name, &export_type, NULL, NULL, &export_region };

  dialog di =
  {
    10, 8, 69, 17, "Export Robot", 5, di_types, di_xs,
    di_ys, di_strs, di_p1s, di_p2s, di_storage, 0
  };

  export_name[0] = 0;

  if(!region_default)
  {
    di.num_elements = 4;
  }

  if(!run_dialog(mzx_world, &di, 0, 0))
  {
    FILE *export_file;

    if(!export_region)
    {
      current_rline = rstate->base->next;
      end_rline = NULL;
    }
    else
    {
      current_rline = rstate->mark_start_rline;
      end_rline = rstate->mark_end_rline;
    }

    if(!export_type)
    {
      add_ext(export_name, ".txt");
      export_file = fopen(export_name, "wt");

      while(current_rline != end_rline)
      {
        fputs(current_rline->line_text, export_file);
        fputc('\n', export_file);
        current_rline = current_rline->next;
      }
    }
    else
    {
      add_ext(export_name, ".bc");
      export_file = fopen(export_name, "wb");

      fputc(0xFF, export_file);

      while(current_rline != end_rline)
      {
        fwrite(current_rline->line_bytecode,
         current_rline->line_bytecode_length, 1, export_file);
        current_rline = current_rline->next;
      }

      fputc(0, export_file);
    }

    fclose(export_file);
  }
}

void import_block(World *mzx_world, robot_state *rstate)
{
  char import_name[128];
  char *txt_ext[] = { ".TXT", NULL };

  if(!choose_file(txt_ext, import_name, "Import Robot", 0))
  {
    FILE *import_file = fsafeopen(import_name, "rt");
    char line_buffer[256];
    rstate->command_buffer = line_buffer;

    while(fgets(line_buffer, 255, import_file))
    {
      line_buffer[strlen(line_buffer) - 1] = 0;
      add_line(rstate);
    }

    rstate->command_buffer = rstate->command_buffer_space;
  }
}

void edit_settings(World *mzx_world)
{
  // 38 x 12
  char di_types[8] = { DE_TEXT, DE_INPUT, DE_INPUT, DE_INPUT, DE_INPUT,
   DE_INPUT, DE_BUTTON, DE_BUTTON };
  char di_xs[8] = { 5, 5, 5, 5, 5, 5, 15, 37 };
  char di_ys[8] = { 2, 4, 5, 6, 7, 8, 10, 10 };
  char new_macros[5][64];

  memcpy(new_macros, macros, 64 * 5);

  char *di_strs[8] =
  {
    "Macros:",
    "F6-  ", "F7-  ", "F8-  ", "F9-  ", "F10- ",
    "OK", "Cancel"
  };

  int di_p1s[8] = { 0, 43, 43, 43, 43, 43, 0, -1 };
  int di_p2s[8] = { 0 };
  void *di_storage[8] = { 0, new_macros[0], new_macros[1], new_macros[2],
   new_macros[3], new_macros[4] };

  dialog di =
  {
    10, 6, 69, 17, "Edit settings", 8, di_types, di_xs,
    di_ys, di_strs, di_p1s, di_p2s, di_storage, 1
  };

  if(!run_dialog(mzx_world, &di, 0, 0))
    memcpy(macros, new_macros, 64 * 5);
}

void block_action(World *mzx_world, robot_state *rstate)
{
  int block_command = block_menu(mzx_world);

  if(!rstate->mark_mode)
  {
    rstate->mark_start_rline = rstate->current_rline;
    rstate->mark_end_rline = rstate->current_rline;
    rstate->mark_start = rstate->current_line;
    rstate->mark_end = rstate->current_line;
  }

  switch(block_command)
  {
    case 0:
    {
      // Copy block into buffer
      copy_block_to_buffer(rstate);
      break;
    }

    case 1:
    {
      // Copy block into buffer, then clear block
      copy_block_to_buffer(rstate);
      clear_block(rstate);
      rstate->mark_mode = 0;
      break;
    }

    case 2:
    {
      // Clear block
      clear_block(rstate);
      rstate->mark_mode = 0;
      break;
    }

    case 3:
    {
      // Export block - text only
      export_block(mzx_world, rstate, 1);
      break;
    }
  }
}

void goto_line(robot_state *rstate, int line)
{
  if(line > rstate->total_lines)
    line = rstate->total_lines;

  if(line < 1)
    line = 1;

  move_and_update(rstate, line - rstate->current_line);
}

void find_replace_action(World *mzx_world, robot_state *rstate)
{
  char di_types[8] = { DE_INPUT, DE_INPUT, DE_CHECK, DE_CHECK, 
   DE_BUTTON, DE_BUTTON, DE_BUTTON, DE_BUTTON };
  char di_xs[8] = { 2, 2, 3, 30, 5, 15, 27, 44, };
  char di_ys[8] = { 2, 3, 5, 5, 7, 7, 7, 7 };

  char *di_strs[8] =
  {
    "Find:    ", "Replace: ", "Wrap around end", "Match case",
    "Search", "Replace", "Replace All", "Cancel"
  };

  int di_p1s[8] = { 46, 46, 1, 1, 0, 1, 2, -1 };
  int di_p2s[8] = { 0, 0, 15, 10, 0, 0, 0, 0 };
  void *di_storage[8] = { search_string, replace_string, wrap_option,
   case_option, NULL, NULL, NULL, NULL };

  dialog di =
  {
    10, 7, 69, 16, "Search and Replace", 8, di_types, di_xs,
    di_ys, di_strs, di_p1s, di_p2s, di_storage, 0
  };

  last_find_option = run_dialog(mzx_world, &di, 0, 0);

  switch(last_find_option)
  {
    case 0:
    {
      // Find
      int l_pos;
      int l_num = find_string(rstate, search_string, wrap_option[0],
       &l_pos, case_option[0]);

      if(l_num != -1)
      {
        goto_line(rstate, l_num);
        rstate->current_x = l_pos;
      }

      break;
    }

    case 1:
    {
      // Find & Replace
      int l_pos;
      int l_num = find_string(rstate, search_string, wrap_option[0],
       &l_pos, case_option[0]);

      if(l_num != -1)
      {
        goto_line(rstate, l_num);
        rstate->current_x = l_pos;
        replace_current_line(rstate, l_pos, search_string, replace_string);
      }

      break;
    }

    case 2:
    {
      int l_pos;
      int l_num;
      int r_len = strlen(replace_string);
      int s_len = strlen(search_string);
      int start_line = rstate->current_line;
      int start_pos = rstate->current_x;
      int last_line = start_line;
      int last_pos = start_pos;
      int dif = r_len - s_len;
      int wrapped = 0;

      do
      {
        l_num = find_string(rstate, search_string, wrap_option[0],
         &l_pos, case_option[0]);

        // Is it on the starting line and below the starting cursor?
        // If so modify the starting cursor because the line was
        // changed.
        if((l_num == start_line) && (l_num <= start_pos))
        {
          start_pos += dif;
        }

        // A decrease had better indicate a wrap, and you only get one
        if(((l_num == last_line) && (l_pos < last_pos)) || (l_num < last_line))
        {
          if((((l_num == start_line) && (l_pos <= start_pos)) || 
           (l_num < start_line)) && (!wrapped))
          {
            wrapped = 1;
          }
          else
          {
            break;
          }
        }

        // If it has already wrapped don't let it past the start
        if((((l_num == start_line) && (l_pos > start_pos)) || 
         (l_num > start_line)) && wrapped)
        {
          break;
        }

        if(l_num != -1)
        {
          goto_line(rstate, l_num);
          rstate->current_x = l_pos;
          replace_current_line(rstate, l_pos, search_string, replace_string);

          l_pos += r_len - 1;
          rstate->current_x = l_pos;

          last_line = l_num;
          last_pos = l_pos;
        }
        else
        {
          break;
        }
      } while(1);

      break;
    }
  }
}

void replace_current_line(robot_state *rstate, int r_pos, char *str,
 char *replace)
{
  robot_line *current_rline = rstate->current_rline;

  int replace_size = strlen(replace);
  int str_size = strlen(str);
  char new_buffer[512];
  
  strcpy(new_buffer, current_rline->line_text);
  memmove(new_buffer + r_pos + replace_size,
   new_buffer + r_pos + str_size, strlen(new_buffer) - r_pos);
  memcpy(new_buffer + r_pos, replace, replace_size);

  rstate->command_buffer = new_buffer;
  update_current_line(rstate);
  strcpy(rstate->command_buffer_space, new_buffer);
  rstate->command_buffer = rstate->command_buffer_space;  
}

int find_string(robot_state *rstate, char *str, int wrap,
 int *position, int case_sensitive)
{
  robot_line *current_rline = rstate->current_rline;
  int current_line = rstate->current_line;
  char *pos = NULL;
  char line_buffer[512];
  char *use_buffer = line_buffer;

  update_current_line(rstate);
  strcpy(rstate->command_buffer, current_rline->line_text);

  if(!str[0])
    return -1;

  if(!case_sensitive)
    str_lower_case(str, str);

  // Check the first line first

  if(!case_sensitive)
    str_lower_case(current_rline->line_text, line_buffer);
  else
    use_buffer = current_rline->line_text;

  pos = strstr(use_buffer + rstate->current_x + 1, str);

  if(pos == NULL)
  {
    current_rline = current_rline->next;
    current_line++;

    // Now check the next lines
    while(current_rline != NULL)
    {
      if(!case_sensitive)
        str_lower_case(current_rline->line_text, line_buffer);
      else
        use_buffer = current_rline->line_text;
  
      pos = strstr(use_buffer, str);
  
      if(pos)
        break;
  
      current_rline = current_rline->next;
      current_line++;
    }
  }

  // Wrap around?
  if(wrap && (pos == NULL))
  {
    current_rline = rstate->base->next;
    current_line = 1;
    while(current_rline != rstate->current_rline->next)
    {
      if(!case_sensitive)
        str_lower_case(current_rline->line_text, line_buffer);
      else
        use_buffer = current_rline->line_text;

      pos = strstr(use_buffer, str);

      if(pos)
        break;

      current_rline = current_rline->next;
      current_line++;
    }
  }

  if(pos)
  {
    *position = pos - use_buffer;
    return current_line;
  }

  return -1;
}

void str_lower_case(char *str, char *dest)
{
  int i = 0;
  char c = str[0];

  while(c)
  {
    dest[i] = tolower(c);
    i++;
    c = str[i];
  }

  dest[i] = 0;
}
