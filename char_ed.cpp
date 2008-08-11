/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
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

// I went ahead and commented out the entire smzx char edit stuff -Koji
// Character editor

#include <stdlib.h>
#include <string.h>

#include "helpsys.h"
#include "char_ed.h"
#include "window.h"
#include "graphics.h"
#include "hexchar.h"
#include "data.h"
#include "event.h"

//-------------------------------------------------
//     +----------------+ Current char-     (#000)
// 000 |................|    ..........0..........
// 000 |................| %%      Move cursor
// 000 |................| -+      Change char
// 000 |................| Space   Toggle pixel
// 000 |................| Enter   Select char
// 000 |................| Del     Clear char
// 000 |................| N       'Negative'
// 000 |................| Alt+%%  Shift char
// 000 |................| M       'Mirror'
// 000 |................| F       'Flip'
// 000 |................| F2      Copy to buffer
// 000 |................| F3      Copy from buffer
// 000 |................| Tab     Draw (off)
// 000 |................|
//     +----------------+ F5      Revert to MZX
//-------------------------------------------------

//Mouse- Menu=Command
//       Grid=Toggle
//       "Current Char-"=Enter
//       "..0.."=Change char

char buffer[14];
char current_char = 1;

int char_editor(World *mzx_world)
{
  char matrix[14];
  int x = 3;
  int y = 6;
  int draw = 0;
  int draw_new;
  int bit;

  int i, i2, key;
  set_context(79);
  save_screen();
  draw_window_box(15, 3, 65, 20, 143, 128, 135, 1, 1);
  // Draw in static elements
  draw_window_box(21, 4, 38, 19, 128, 143, 135, 0, 0);
  write_string
  (
    "Current char-\t(#000)\n\n"
    "\t Move cursor\n"
    "-+\t Change char\n"
    "Space   Toggle pixel\n"
    "Enter   Select char\n"
    "Del\tClear char\n"
    "N\t  'Negative'\n"
    "Alt+  Shift char\n"
    "M\t  'Mirror'\n"
    "F\t  'Flip'\n"
    "F2\t Copy to buffer\n"
    "F3\t Copy from buffer\n"
    "Tab\tDraw\n"
    "F4\t Revert to ASCII\n"
    "F5\t Revert to MZX", 40, 4, 143, 1
  );

  m_show();
  // Get char
  ec_read_char(current_char, matrix);

  do
  {
    // Update char
    for(i = 0; i < 14; i++)
    {
      write_number(matrix[i], 135, 17, i + 5, 3);

      for(i2 = 0; i2 < 8; i2++)
      {
        bit = (matrix[i] >> (7 - i2)) & 0x01;
        if(bit)
          write_string("€€", (i2 * 2) + 22, i + 5, 135, 0);
        else
          write_string("˙˙", (i2 * 2) + 22, i + 5, 135, 0);
      }
    }

    switch(draw)
    {
      case 0:
        write_string("(off)   ", 53, 17, 143, 0);
        break;

      case 1:
        write_string("(set)   ", 53, 17, 143, 0);
        break;

      case 2:
        write_string("(clear) ", 53, 17, 143, 0);
        break;

      case 3:
        write_string("(toggle)", 53, 17, 143, 0);
        break;
    }

    // Highlight cursor
    color_line(2, (x * 2) + 22, y + 5, 27);
    // Current character
    write_number(current_char, 143, 60, 4, 3);

    for(i = -10; i < 11; i++)
    {
      draw_char(current_char + i, 128 + (i == 0) * 15, i + 53, 5);
    }

    update_screen();
    update_event_status_delay();

    key = get_key(keycode_SDL);

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_x > 21) && (mouse_x < 38) &&
       (mouse_y > 4) && (mouse_y < 19))
      {
        // Grid.
        x = (mouse_x - 22) / 2;
        y = mouse_y - 5;
        matrix[y] = matrix[y] ^ (0x80 >> x);
        ec_change_byte(current_char, y, matrix[y]);
      }
      else

      if(((mouse_x > 43) && (mouse_x < 65)) &&
       (mouse_y == 5))
      {
        current_char += mouse_x - 53;
        ec_read_char(current_char, matrix);
      }
    }

    draw_new = 0;

    switch(key)
    {
      case SDLK_LEFT:
      {
        if(get_alt_status(keycode_SDL))
        {
          char wrap_bit;

          for(i = 0; i < 14; i++)
          {
            wrap_bit = matrix[i] & 0x80;
            matrix[i] <<= 1;
            if(wrap_bit)
              matrix[i] |= 1;
          }
					ec_change_char(current_char, matrix);
        }
        else
        {
          x--;
          if(x < 0)
            x = 7;
          draw_new = 1;
        }
        break;
      }

      case SDLK_RIGHT:
      {
        if(get_alt_status(keycode_SDL))
        {
          char wrap_bit;

          for(i = 0; i < 14; i++)
          {
            wrap_bit = matrix[i] & 1;
            matrix[i] >>= 1;
            if(wrap_bit)
              matrix[i] |= 0x80;
          }
					ec_change_char(current_char, matrix);
        }
        else
        {
          x++;
          if(x > 7)
            x = 0;
          draw_new = 1;
        }
        break;
      }

      case SDLK_UP:
      {
        if(get_alt_status(keycode_SDL))
        {
          char wrap_row = matrix[0];

          for(i = 0; i < 13; i++)
          {
            matrix[i] = matrix[i + 1];
          }
          matrix[13] = wrap_row;
					ec_change_char(current_char, matrix);
        }
        else
        {
          y--;
          if(y < 0)
            y = 13;
          draw_new = 1;
        }
        break;
      }

      case SDLK_DOWN:
      {
        if(get_alt_status(keycode_SDL))
        {
          char wrap_row = matrix[13];

          for(i = 14; i > 0; i--)
          {
            matrix[i] = matrix[i - 1];
          }
          matrix[0] = wrap_row;
					ec_change_char(current_char, matrix);
        }
        else
        {
          y++;
          if(y > 13)
            y = 0;
          draw_new = 1;
        }
        break;
      }

      case SDLK_KP_MINUS:
      case SDLK_MINUS:
      {
        current_char--;
        ec_read_char(current_char, matrix);
        break;
      }

      case SDLK_KP_PLUS:
      case SDLK_EQUALS:
			case SDLK_PLUS:
      {
        current_char++;
        ec_read_char(current_char, matrix);
        break;
      }

      case SDLK_SPACE:
      {
        matrix[y] = matrix[y] ^ (0x80 >> x);
        ec_change_byte(current_char, y, matrix[y]);
        break;
      }

			case SDLK_KP_ENTER:
      case SDLK_RETURN:
      {
        int new_char = char_selection(current_char);
        if(new_char >= 0)
          current_char = new_char;

        ec_read_char(current_char, matrix);
        break;
      }

      case SDLK_q:
      {
        key = SDLK_ESCAPE;
        break;
      }

      case SDLK_DELETE:
      {
        memset(matrix, 0, 14);
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_F2:
      {
        memcpy(buffer, matrix, 14);
        break;
      }

      case SDLK_F3:
      {
        memcpy(matrix, buffer, 14);
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_m:
      {
        char current_row;
        for(i = 0; i < 14; i++)
        {
          current_row = matrix[i];
          current_row = (current_row << 4) | (current_row >> 4);
          current_row = ((current_row & 0xCC) >> 2) |
           ((current_row & 0x33) << 2);
          current_row = ((current_row & 0xAA) >> 1) |
           ((current_row & 0x55) << 1);

          matrix[i] = current_row;
        }
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_f:
      {
        char current_row;
        for(i = 0; i < 7; i++)
        {
          current_row = matrix[i];
          matrix[i] = matrix[13 - i];
          matrix[13 - i] = current_row;
        }
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_TAB:
      {
        draw++;
        if(draw > 3)
          draw = 0;

        draw_new = 1;
        break;
      }

      case SDLK_n:
      {
        for(i = 0; i < 14; i++)
        {
          matrix[i] ^= 0xFF;
        }
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_F4:
      {
        ec_load_char_ascii(current_char);
        ec_read_char(current_char, matrix);
        break;
      }

      case SDLK_F5:
      {
        ec_load_char_mzx(current_char);
        ec_read_char(current_char, matrix);
        break;
      }

      case SDLK_F1: // F1
      {
        m_show();
        help_system(mzx_world);
        break;
      }
    }

    if(draw_new)
    {
      switch(draw)
      {
        case 1:
        {
          matrix[y] = matrix[y] | (0x80 >> x);
          break;
        }

        case 2:
        {
          matrix[y] = matrix[y] & ~(0x80 >> x);
          break;
        }

        case 3:
        {
          matrix[y] = matrix[y] ^ (0x80 >> x);
          break;
        }
      }
      ec_change_byte(current_char, y, matrix[y]);
    }
  } while(key != SDLK_ESCAPE);
  restore_screen();
  pop_context();
  return current_char;
}

int smzx_char_editor(World *mzx_world)
{
  char matrix[14];
  int x = 2;
  int y = 6;
  int draw = 0;
  int draw_new;
  int bit;
  int current_type = 0;

  int i, i2, key;
  set_context(79);
  save_screen();
  draw_window_box(15, 3, 65, 20, 143, 128, 135, 1, 1);
  // Draw in static elements
  draw_window_box(21, 4, 38, 19, 128, 143, 135, 0, 0);
  write_string
  (
    "Current char-\t(#000)\n\n"
    "\t Move cursor\n"
    "-+\t Change char\n"
    "Space   Toggle pixel\n"
    "Enter   Select char\n"
    "Del\tClear char\n"
    "N\t  'Negative'\n"
    "Alt+  Shift char\n"
    "M\t  'Mirror'\n"
    "F\t  'Flip'\n"
    "F2\t Copy to buffer\n"
    "F3\t Copy from buffer\n"
    "Tab\tDraw\n"
    "1-4\tSelect\n"
    "F5\t Revert to SMZX", 40, 4, 143, 1
  );

  m_show();
  // Get char
  ec_read_char(current_char, matrix);

  do
  {
    // Update char
    for(i = 0; i < 14; i++)
    {
      write_number(matrix[i], 135, 17, i + 5, 3);

      for(i2 = 0; i2 < 8; i2 += 2)
      {
        bit = (matrix[i] >> (6 - i2)) & 0x03;
        switch(bit)
        {
          case 3:
            write_string("€€€€", (i2 * 2) + 22, i + 5, 135, 0);
            break;

          case 1:
            write_string("≤≤≤≤", (i2 * 2) + 22, i + 5, 135, 0);
            break;

          case 2:
            write_string("∞∞∞∞", (i2 * 2) + 22, i + 5, 135, 0);
            break;

          case 0:
            write_string("˙˙˙˙", (i2 * 2) + 22, i + 5, 135, 0);
            break;
        }
      }
    }

    switch(draw)
    {
      case 0:
        write_string("(off)   ", 53, 17, 143, 0);
        break;

      case 1:
        write_string("(set)   ", 53, 17, 143, 0);
        break;
    }

    switch(current_type)
    {
      case 3:
        write_string("€€€€", 57, 18, 135, 0);
        break;

      case 1:
        write_string("≤≤≤≤", 57, 18, 135, 0);
        break;

      case 2:
        write_string("∞∞∞∞", 57, 18, 135, 0);
        break;

      case 0:
        write_string("˙˙˙˙", 57, 18, 135, 0);
        break;
    }

    // Highlight cursor
    color_line(4, (x * 2) + 22, y + 5, 27);
    // Current character
    write_number(current_char, 143, 60, 4, 3);

    for(i = -10; i < 11; i++)
    {
      draw_char(current_char + i, 128 + (i == 0) * 15, i + 53, 5);
    }

    update_screen();
    update_event_status_delay();

    key = get_key(keycode_SDL);

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_x > 21) && (mouse_x < 38) &&
       (mouse_y > 4) && (mouse_y < 19))
      {
        // Grid.
        x = ((mouse_x - 22) / 2) & 0xFE;
        y = mouse_y - 5;

        if(get_mouse_status() == SDL_BUTTON(3))
        {
          current_type = (matrix[y] >> (6 - x)) & 0x03;
        }
        else
        {
          matrix[y] = (matrix[y] & ~(0xC0 >> x)) | (current_type << (6 - x));
          ec_change_byte(current_char, y, matrix[y]);
        }
      }
      else

      if(((mouse_x > 43) && (mouse_x < 65)) &&
       (mouse_y == 5))
      {
        current_char += mouse_x - 53;
        ec_read_char(current_char, matrix);
      }
    }

    draw_new = 0;

    switch(key)
    {
      case SDLK_LEFT:
      {
        if(get_alt_status(keycode_SDL))
        {
          char wrap_section;

          for(i = 0; i < 14; i++)
          {
            wrap_section = (matrix[i] & 0xC0) >> 6;
            matrix[i] <<= 2;
            matrix[i] |= wrap_section;
          }
					ec_change_char(current_char, matrix);
        }
        else
        {
          x -= 2;
          if(x < 0)
            x = 6;
          draw_new = 1;
        }
        break;
      }

      case SDLK_RIGHT:
      {
        if(get_alt_status(keycode_SDL))
        {
          char wrap_section;

          for(i = 0; i < 14; i++)
          {
            wrap_section = matrix[i] & 0x03;
            matrix[i] >>= 2;
            matrix[i] |= (wrap_section << 6);
          }
					ec_change_char(current_char, matrix);
        }
        else
        {
          x += 2;
          if(x > 6)
            x = 0;
          draw_new = 1;
        }
        break;
      }

      case SDLK_UP:
      {
        if(get_alt_status(keycode_SDL))
        {
          char wrap_row = matrix[0];

          for(i = 0; i < 13; i++)
          {
            matrix[i] = matrix[i + 1];
          }
          matrix[13] = wrap_row;
					ec_change_char(current_char, matrix);
        }
        else
        {
          y--;
          if(y < 0)
            y = 13;
          draw_new = 1;
        }
        break;
      }

      case SDLK_DOWN:
      {
        if(get_alt_status(keycode_SDL))
        {
          char wrap_row = matrix[13];

          for(i = 14; i > 0; i--)
          {
            matrix[i] = matrix[i - 1];
          }
          matrix[0] = wrap_row;
					ec_change_char(current_char, matrix);
        }
        else
        {
          y++;
          if(y > 13)
            y = 0;
          draw_new = 1;
        }
        break;
      }

      case SDLK_KP_MINUS:
      case SDLK_MINUS:
      {
        current_char--;
        ec_read_char(current_char, matrix);
        break;
      }

			case SDLK_PLUS:
      case SDLK_KP_PLUS:
      case SDLK_EQUALS:
      {
        current_char++;
        ec_read_char(current_char, matrix);
        break;
      }

      case SDLK_SPACE:
      {
        matrix[y] = (matrix[y] & ~(0xC0 >> x)) | (current_type << (6 - x));
        ec_change_byte(current_char, y, matrix[y]);
        break;
      }

			case SDLK_KP_ENTER:
      case SDLK_RETURN:
      {
        int new_char = char_selection(current_char);
        if(new_char >= 0)
          current_char = new_char;

        ec_read_char(current_char, matrix);
        break;
      }

      case SDLK_q:
      {
        key = SDLK_ESCAPE;
        break;
      }

      case SDLK_DELETE:
      {
        memset(matrix, 0, 14);
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_F2:
      {
        memcpy(buffer, matrix, 14);
        break;
      }

      case SDLK_F3:
      {
        memcpy(matrix, buffer, 14);
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_m:
      {
        char current_row;
        for(i = 0; i < 14; i++)
        {
          current_row = matrix[i];
          current_row = (current_row << 4) | (current_row >> 4);
          current_row = ((current_row & 0xCC) >> 2) |
           ((current_row & 0x33) << 2);

          matrix[i] = current_row;
        }
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_f:
      {
        char current_row;
        for(i = 0; i < 7; i++)
        {
          current_row = matrix[i];
          matrix[i] = matrix[13 - i];
          matrix[13 - i] = current_row;
        }
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_TAB:
      {
        draw ^= 1;
        draw_new = 1;
        break;
      }

      case SDLK_n:
      {
        for(i = 0; i < 14; i++)
        {
          matrix[i] ^= 0xFF;
        }
        ec_change_char(current_char, matrix);
        break;
      }

      case SDLK_F5:
      {
        ec_load_char_smzx(current_char);
        ec_read_char(current_char, matrix);
        break;
      }

      case SDLK_1:
      {
        current_type = 0;
        break;
      }

      case SDLK_2:
      {
        current_type = 2;
        break;
      }

      case SDLK_3:
      {
        current_type = 1;
        break;
      }

      case SDLK_4:
      {
        current_type = 3;
        break;
      }

      case SDLK_INSERT:
      {
        current_type = (matrix[y] >> (6 - x)) & 0x03;
        break;
      }

      case SDLK_F1: // F1
      {
        m_show();
        help_system(mzx_world);
        break;
      }
    }

    if((draw_new) && draw)
    {
      matrix[y] = (matrix[y] & ~(0xC0 >> x)) | (current_type << (6 - x));
      ec_change_byte(current_char, y, matrix[y]);
    }
  } while(key != SDLK_ESCAPE);
  restore_screen();
  pop_context();
  return current_char;
}

