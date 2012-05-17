/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

// Code to the intake function, which inputs text with friendliness.

#include <string.h>
#include <ctype.h>

#include "event.h"
#include "intake.h"
#include "graphics.h"
#include "window.h"

// Global status of insert
static char insert_on = 1;

static char last_char = 0;

// (returns the key used to exit) String points to your memory for storing
// the new string. The current "value" is used- clear the string before
// calling intake if you need a blank string. Max_len is the maximum length
// of the string. X, y, segment, and color are self-explanitory. Exit_type
// determines when intake exits- 0 means exit only on Enter, 1 on Enter or
// ESC, 2 on any non-display char except the editing keys. Filter_type is
// bits to turn on filters- 1 changes all alpha to upper, 2 changes all
// alpha to lower, 4 blocks all numbers, 8 blocks all alpha, 16 blocks
// spaces, 32 blocks graphics, (ascii>126) 64 blocks punctuation that isn't
// allowed in a filename/path/drive combo, 128 blocks punctuation that isn't
// allowed in a filename only combo (use w/64), 256 blocks all punctuation.
// (IE non-alphanumerics and spaces) The editing keys supported are as
// follows- Keys to enter characters, Enter/ESC to exit, Home/End to move to
// the front/back of the line, Insert to toggle insert/overwrite, Left/Right
// to move within the line, Bkspace/Delete to delete as usual, and Alt-Bksp
// to clear the entire line. No screen saving is performed. After this
// function, the cursor is automatically off. If password is set, all
// characters appear as x.

// Mouse support- Clicking inside string sends cursor there. Clicking
// outside string returns a MOUSE_EVENT to caller without acknowledging
// the event.

// Returns a backspace if attempted at start of line. (and exit_type==2)
// Returns a delete if attempted at end of line. (and exit_type==2)

// If robo_intk is set, only 76 chars are shown and symbols are altered
// on the left/right if there is more to the left/right. Scrolling
// of the line is supported. The current character (1 on) is shown at
// x = 32, y = 0, in color 79, min. 3 chars.

int intake(struct world *mzx_world, char *string, int max_len,
 int x, int y, char color, int exit_type, int filter_type,
 int *return_x_pos, bool robo_intk, char *macro)
{
  int currx, curr_len, macro_position = -1;
  int done = 0, place = 0;
  char cur_char = 0;
  char temp_char;
  int use_mask = mzx_world->conf.mask_midchars;
  int mouse_press;
  int key;

  if(macro != NULL)
    macro_position = 0;

  // Activate cursor
  if(insert_on)
    cursor_underline();
  else
    cursor_solid();
  // Put cursor at the end of the string...
  currx = curr_len = (int)strlen(string);

  // ...unless return_x_pos says not to.
  if((return_x_pos) && (*return_x_pos < currx))
    currx = *return_x_pos;

  if(robo_intk && (currx > 75))
    move_cursor(77, y);
  else
    move_cursor(x + currx, y);

  if(insert_on)
    cursor_underline();
  else
    cursor_solid();

  do
  {
    if(!robo_intk)
    {
      if(use_mask)
        write_string_mask(string, x, y, color, 0);
      else
        write_string_ext(string, x, y, color, 0, 0, 16);
    }
    else
    {
      draw_char('\x11', color, 79, y);
      if((curr_len < 76) || (currx < 76))
      {
        draw_char('\x10', color, 0, y);

        if(curr_len < 76)
        {
          if(use_mask)
            write_line_mask(string, x, y, color, 0);
          else
            write_line_ext(string, x, y, color, 0, 0, 16);
          fill_line(76 - curr_len, x + curr_len, y, 32, color);
        }
        else
        {
          temp_char = string[76];
          string[76] = 0;
          if(use_mask)
            write_line_mask(string, x, y, color, 0);
          else
            write_line_ext(string, x, y, color, 0, 0, 16);
          string[76] = temp_char;

          draw_char('\xaf', color, 79, y);
        }
      }
      else
      {
        draw_char('\x20', color, 77, y);
        if(strlen(string + currx - 75) > 78)
        {
          temp_char = string[currx + 1];
          string[currx + 1] = 0;
          if(use_mask)
          {
            write_line_mask(string + currx - 75, x, y, color, 0);
          }
          else
          {
            write_line_ext(string + currx - 75, x, y,
             color, 0, 0, 16);
          }
          string[currx + 1] = temp_char;
        }
        else
        {
          if(use_mask)
          {
            write_line_mask(string + currx - 75, x, y, color, 0);
          }
          else
          {
            write_line_ext(string + currx - 75, x, y,
             color, 0, 0, 16);
          }
        }
        draw_char('\xae', color, 0, y);
        if(currx < curr_len)
          draw_char('\xaf', color, 79, y);
      }
      draw_char('\x20', color, 78, y);
    }

    if(!robo_intk)
    {
      fill_line(max_len + 1 - curr_len, x + curr_len, y, 32, color);
    }
    else
    {
      write_number(currx + 1, 79, 32, 0, 3, 0, 10);
      write_number(curr_len + 1, 79, 36, 0, 3, 0, 10);
    }

    // Get key
    if(macro_position != -1)
    {
      key = macro[macro_position];
      cur_char = key;

      macro_position++;
      if(macro[macro_position] == 0)
        macro_position = -1;

      if(key == '^')
        key = IKEY_RETURN;
    }
    else
    {
      update_screen();
      update_event_status_delay();
      key = get_key(keycode_internal);
      place = 0;

      cur_char = get_key(keycode_unicode);
    }

    mouse_press = get_mouse_press_ext();

    if(get_mouse_press_ext())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);
      if((mouse_y == y) && (mouse_x >= x) &&
       (mouse_x <= (x + max_len)) && (mouse_press <= MOUSE_BUTTON_RIGHT))
      {
        // Yep, reposition cursor.
        currx = mouse_x - x;
        if(currx > curr_len)
          currx = curr_len;
      }
      else
      {
        key = -1;
        done = 1;
      }
    }

    // Handle key cases
    switch(key)
    {
      case IKEY_ESCAPE:
      {
        // ESC
        if(exit_type > 0)
        {
          done = 1;
        }
        break;
      }

      case IKEY_RETURN:
      {
        // Enter
        done = 1;
        break;
      }

      case IKEY_HOME:
      {
        if(get_alt_status(keycode_internal) && robo_intk)
        {
          done = 1;
        }
        else
        {
          // Home
          currx = 0;
        }
        break;
      }

      case IKEY_END:
      {
        if(get_alt_status(keycode_internal) && robo_intk)
        {
          done = 1;
        }
        else
        {
          // End
          currx = curr_len;
        }
        break;
      }

      case IKEY_LEFT:
      {
        if(get_ctrl_status(keycode_internal))
        {
          // Find nearest space to the left
          if(currx)
          {
            char *current_position = string + currx;

            if(currx)
              current_position--;

            if(!isalnum((int)*current_position))
            {
              while(currx && !isalnum((int)*current_position))
              {
                current_position--;
                currx--;
              }
            }

            do
            {
              current_position--;
              currx--;
            } while(currx && isalnum((int)*current_position));

            if(currx < 0)
              currx = 0;
          }
        }
        else
        {
          // Left
          if(currx > 0)
            currx--;
        }

        break;
      }

      case IKEY_RIGHT:
      {
        if(get_ctrl_status(keycode_internal))
        {
          // Find nearest space to the right
          if(currx < curr_len)
          {
            char *current_position = string + currx;
            char current_char = *current_position;
            if(!isalnum((int)current_char))
            {
              do
              {
                current_position++;
                currx++;
                current_char = *current_position;
              } while(current_char && !isalnum((int)current_char));
            }

            while(current_char && isalnum((int)current_char))
            {
              current_position++;
              currx++;
              current_char = *current_position;
            }
          }
        }
        else
        {
          // Right
          if(currx < curr_len)
            currx++;
        }

        break;
      }

      case IKEY_F1:
      case IKEY_F2:
      case IKEY_F3:
      case IKEY_F4:
      case IKEY_F5:
      case IKEY_F6:
      case IKEY_F7:
      case IKEY_F8:
      case IKEY_F9:
      case IKEY_F10:
      case IKEY_F11:
      case IKEY_F12:
      case IKEY_UP:
      case IKEY_DOWN:
      case IKEY_TAB:
      case IKEY_PAGEUP:
      case IKEY_PAGEDOWN:
      {
        done = 1;
        break;
      }

      case IKEY_INSERT:
      {
        if(get_alt_status(keycode_internal) && robo_intk)
        {
          done = 1;
        }
        else
        {
          // Insert
          if(insert_on)
            cursor_solid();
          else
            cursor_underline();

          insert_on ^= 1;
        }
        break;
      }

      case IKEY_BACKSPACE:
      {
        // Backspace, at 0 it might exit
        if(get_alt_status(keycode_internal))
        {
          // Alt-backspace, erase input
          curr_len = currx = 0;
          string[0] = 0;
        }
        else

        if(get_ctrl_status(keycode_internal))
        {
          // Find nearest space to the left
          if(currx)
          {
            int old_position = currx;

            if(!isalnum((int)string[currx]))
            {
              while(currx && !isalnum((int)string[currx]))
              {
                currx--;
              }
            }

            while(currx && isalnum((int)string[currx]))
            {
              currx--;
            }

            curr_len -= old_position - currx;

            memmove(string + currx, string + old_position,
             strlen(string + old_position) + 1);
          }
        }
        else

        if(currx == 0)
        {
          if(exit_type == 2)
          {
            done = 1;
          }
        }
        else
        {
          // Move all back 1, decreasing string length
          memmove(string + currx - 1, string + currx, curr_len - currx + 1);
          curr_len--;
          // Cursor back one
          currx--;
        }
        break;
      }

      case IKEY_DELETE:
      {
        // Delete, at the end might exit
        if(currx == curr_len)
        {
          if(exit_type == 2)
            done = 1;
        }
        else
        {
          if(curr_len)
          {
            // Move all back 1, decreasing string length
            memmove(string + currx, string + currx + 1, curr_len - currx);
            curr_len--;
          }
        }
        break;
      }

      case IKEY_c:
      {
        if(get_ctrl_status(keycode_internal) && robo_intk)
        {
          done = 1;
        }
        else

        if(get_alt_status(keycode_internal) && !filter_type)
        {
          // If alt - C is pressed, choose character
          int new_char = char_selection(last_char);
          if(new_char >= 32)
          {
            cur_char = new_char;
            last_char = new_char;
            place = 1;
          }
          else
          {
            place = 0;
          }
        }
        else
        {
          place = 1;
        }

        break;
      }

      case IKEY_t:
      {
        if(get_alt_status(keycode_internal))
        {
          done = 1;
        }
        else
        {
          place = 1;
        }
        break;
      }

      case IKEY_l:
      case IKEY_g:
      case IKEY_d:
      case IKEY_f:
      case IKEY_r:
      {
        if(get_ctrl_status(keycode_internal) && robo_intk)
        {
          done = 1;
        }
        else
        {
          place = 1;
        }
        break;
      }

      case IKEY_i:
      {
        if((get_ctrl_status(keycode_internal) ||
         get_alt_status(keycode_internal)) && robo_intk)
        {
          done = 1;
        }
        else
        {
          place = 1;
        }
        break;
      }

      case IKEY_u:
      case IKEY_o:
      case IKEY_x:
      case IKEY_b:
      case IKEY_s:
      case IKEY_e:
      case IKEY_v:
      case IKEY_p:
      case IKEY_h:
      case IKEY_m:
      {
        if(get_alt_status(keycode_internal) && robo_intk)
        {
          done = 1;
        }
        else
        {
          place = 1;
        }
        break;
      }

      case IKEY_LSHIFT:
      case IKEY_RSHIFT:
      case 0:
      {
        place = 0;
        break;
      }

      default:
      {
        // Place the char
        place = 1;
        break;
      }

      case -1:
      {
        break;
      }
    }

    if(place)
    {
      if((cur_char != 0) && (cur_char < 32) && (exit_type == 2))
      {
        done = 1;
        key = cur_char;
      }
      else

      // Keycode.. Filter.
      if(filter_type & 1)
      {
        if((cur_char >= 'a') && (cur_char <= 'z'))
          cur_char -= 32;
      }

      if(filter_type & 2)
      {
        if((cur_char >= 'A') && (cur_char <= 'Z'))
          cur_char += 32;
      }

      // Block numbers
      if((filter_type & 4) && ((cur_char >= '0') && (cur_char <= '9')))
      {
        place = 0;
      }

      // Block alpha
      if((filter_type & 8) &&
       (((cur_char >= 'a') && (cur_char <= 'z')) ||
       ((cur_char >= 'A') && (cur_char <= 'Z'))))
      {
        place = 0;
      }

      // Block spaces
      if((filter_type & 16) && (cur_char == ' '))
      {
        place = 0;
      }

      // Block high-ASCII
      if((filter_type & 32) && (cur_char > 126))
      {
        place = 0;
      }

      // Block these chars
      if((filter_type & 64) &&
       ((cur_char == '*') || (cur_char == '[') ||
       (cur_char == ']') || (cur_char == '>') ||
       (cur_char == '<') || (cur_char == ',') ||
       (cur_char == '|') || (cur_char == '?') ||
       (cur_char == '=') || (cur_char == ';') ||
       (cur_char == '\"') || (cur_char =='/')))
      {
        place = 0;
      }

      // Block these chars
      if((filter_type & 128) &&
       ((cur_char == ':') || (cur_char == '\\')))
      {
        place = 0;
      }

      // Block these chars
      if((filter_type & 256) &&
       (((cur_char > ' ') && (cur_char < '0')) ||
       ((cur_char > '9') && (cur_char < 'A')) ||
       ((cur_char > 'Z') && (cur_char < 'a')) ||
       ((cur_char > 'z') && (cur_char < 127))))
      {
        place = 0;
      }

      // Now, can it still be placed?
      if(place && (curr_len != max_len) && (!done) && cur_char)
      {
        // Overwrite or insert?
        if((insert_on) || (currx == curr_len))
        {
          // Insert- Move all ahead 1, increasing string length
          curr_len++;
          memmove(string + currx + 1, string + currx, curr_len - currx);
        }
        // Add character and move forward one
        string[currx++] = cur_char;
      }
    }

    // Move cursor
    if(robo_intk && (currx > 75))
      move_cursor(77, y);
    else
      move_cursor(x + currx, y);

    if(insert_on)
      cursor_underline();
    else
      cursor_solid();

    // Loop
  } while(!done);

  cursor_off();
  if(return_x_pos)
    *return_x_pos = currx;

  return key;
}
