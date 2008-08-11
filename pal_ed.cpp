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

/* Palette editor */

#include <stdlib.h>
#include <string.h>

#include "helpsys.h"
#include "pal_ed.h"
#include "window.h"
#include "data.h"
#include "graphics.h"
#include "hexchar.h"
#include "event.h"

//---------------------------------------------
//
// ##..##..##..##..##..##..##..##.. Color #00-
// ##..##..##..##..##..#1.1#1.1#1.1  Red 00/63
// #0.1#2.3#4.5#6.7#8.9#0.1#2.3#4.5  Grn 00/63
// ##..##..##..##..##..##..##..##..  Blu 00/63
// ^^
//
// %- Select color   Alt+D- Default palette
// R- Increase Red   Alt+R- Decrease Red
// G- Increase Grn   Alt+G- Decrease Grn
// B- Increase Blu   Alt+B- Decrease Blu
// A- Increase All   Alt+A- Decrease All
// 0- Blacken color     F2- Store color
// Q- Quit editing      F3- Retrieve color
//
//---------------------------------------------

//Mouse- Color=Change color
//       Menu=Do command
//       RGB #'s=Raise/Lower R/G/B

int saved_r = -1;
int saved_g = -1;
int saved_b = -1;

void palette_editor(World *mzx_world)
{
  int i, i2, key, chr, color;
  int current_color = 0;
  unsigned char current_r, current_g, current_b;

  cursor_off();
  set_context(93);
  save_screen();

  // Draw window
  draw_window_box(17, 3, 63, 19, 128, 143, 135, 1, 1);
  // Draw palette bars
  for(i = 0; i < 32; i++)
  {
    for(i2 = 0; i2 < 4; i2++)
    {
      chr = ' ';
      if((i & 1) && (i2 == 1) && (i > 19))
        chr = '1';
      if((i & 1) && (i2 == 2))
        chr = '0' + ((i / 2) % 10);
      color = ((i & 30) * 8);
      if(i < 20)
        color += 15;
      draw_char(chr, color, i + 19, i2 + 5);
    }
  }

  // Write rgb info stuff
  write_string("Color #00-\n Red 00/63\n Grn 00/63\n Blu 00/63",
   52, 5, 143, 0);

  // Write menu
  write_string
  (
    "- Select color   Alt+D- Default palette\n"
    "R- Increase Red   Alt+R- Decrease Red\n"
    "G- Increase Grn   Alt+G- Decrease Grn\n"
    "B- Increase Blu   Alt+B- Decrease Blu\n"
    "A- Increase All   Alt+A- Decrease All\n"
    "0- Blacken color\tF2- Store color\n"
    "Q- Quit editing\t F3- Retrieve color", 19, 11, 143, 1
  );

  do
  {
    // Write rgb and color #
    get_rgb(current_color, &current_r, &current_g, &current_b);
    write_number(current_color, 143, 60, 5, 2, 1);
    write_number(current_r, 143, 58, 6, 2, 1);
    write_number(current_g, 143, 58, 7, 2, 1);
    write_number(current_b, 143, 58, 8, 2, 1);
    // Draw '^^', get key, erase '^^'
    write_string("", (current_color * 2) + 19, 9, 143, 0);

    update_screen();
    update_event_status_delay();

    key = get_key(keycode_SDL);
    write_string("  ", (current_color * 2) + 19, 9, 143, 0);
    // Process

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_x > 18) && (mouse_x < 51) &&
       (mouse_y > 4) && (mouse_y < 9))
      {
        current_color = (mouse_x - 19) * 2;
        break;
      }
    }

    switch(key)
    {
      case SDLK_LEFT:
      case SDLK_MINUS:
      case SDLK_KP_MINUS:
      {
        if(current_color > 0)
          current_color--;
        break;
      }

      case SDLK_RIGHT:
      case SDLK_EQUALS:
      case SDLK_KP_PLUS:
      {
        if(current_color < 15)
          current_color++;
        break;
      }

      case SDLK_q:
      {
        key = SDLK_ESCAPE;
        break;
      }

      case SDLK_r:
      {
        if(get_alt_status(keycode_SDL))
        {
          if(current_r)
          {
            current_r--;
          }
        }
        else
        {
          if(current_r < 63)
          {
            current_r++;
          }
        }

        set_red_component(current_color, current_r);
        update_palette();
        break;
      }

      case SDLK_g:
      {
        if(get_alt_status(keycode_SDL))
        {
          if(current_g)
          {
            current_g--;
          }
        }
        else
        {
          if(current_g < 63)
          {
            current_g++;
          }
        }

        set_green_component(current_color, current_g);
        update_palette();
        break;
      }

      case SDLK_b:
      {
        if(get_alt_status(keycode_SDL))
        {
          if(current_b)
          {
            current_b--;
          }
        }
        else
        {
          if(current_b < 63)
          {
            current_b++;
          }
        }

        set_blue_component(current_color, current_b);
        update_palette();
        break;
      }

      case SDLK_a:
      {
        if(get_alt_status(keycode_SDL))
        {
          if(current_r)
            current_r--;
          if(current_g)
            current_g--;
          if(current_b)
            current_b--;
        }
        else
        {
          if(current_r < 63)
            current_r++;
          if(current_g < 63)
            current_g++;
          if(current_b < 63)
            current_b++;
        }

        set_rgb(current_color, current_r, current_g, current_b);
        update_palette();
        break;
      }

      case SDLK_0:
      {
        current_r = 0;
        current_g = 0;
        current_b = 0;
        set_rgb(current_color, current_r, current_g, current_b);
        update_palette();
        break;
      }

      case SDLK_d:
      {
        if(get_alt_status(keycode_SDL))
        {
          default_palette();
        }
        break;
      }

      case SDLK_F2:
      {
        saved_r = current_r;
        saved_g = current_g;
        saved_b = current_b;

        break;
      }

      case SDLK_F3:
      {
        if(saved_r != -1)
        {
          current_r = saved_r;
          current_g = saved_g;
          current_b = saved_b;
        }
        set_rgb(current_color, current_r, current_g, current_b);
        update_palette();
        break;
      }

      case SDLK_F1: // F1
      {
        m_show();
        help_system(mzx_world);
        break;
      }
    }
  } while(key != SDLK_ESCAPE);
  restore_screen();
  pop_context();
}
