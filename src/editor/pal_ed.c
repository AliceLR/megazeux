/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

/* Palette editor */

#include <stdlib.h>
#include <string.h>

#include "../helpsys.h"
#include "../window.h"
#include "../data.h"
#include "../graphics.h"
#include "../event.h"

#include "pal_ed.h"


static int content_x;
static int content_y;

static int saved_r = -1;
static int saved_g = -1;
static int saved_b = -1;

static int current_color = 0;
static int minimal_help = 0;

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

// Note: the Menu and RGB #s functionality has been broken since 2.80
// If mouse support is readded, it should probably be in a less awful way.

#define PAL_ED_16_X1          17
#define PAL_ED_16_Y1          3
#define PAL_ED_16_X2          63
#define PAL_ED_16_Y2          19

#define PAL_ED_16_Y1_MINIMAL  0
#define PAL_ED_16_Y2_MINIMAL  5

static void palette_editor_redraw_window_16(void)
{
  int y1, y2;
  int i;

  content_x = PAL_ED_16_X1 + 2;

  if(minimal_help)
  {
    y1 = PAL_ED_16_Y1_MINIMAL;
    y2 = PAL_ED_16_Y2_MINIMAL;
    content_y = y1 + 1;
  }

  else
  {
    y1 = PAL_ED_16_Y1;
    y2 = PAL_ED_16_Y2;
    content_y = y1 + 2;
  }

  // Draw window
  draw_window_box(PAL_ED_16_X1, y1, PAL_ED_16_X2, y2,
   DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, 1, 1);

  // Erase the spot where the palette will go and the line below it.
  for(i = 0; i < 32*4; i++)
    erase_char(i % 32 + content_x, i / 32 + content_y);

  // Write RBG info
  write_string(
   "Color #00-\n"
   " Red 00/63\n"
   " Grn 00/63\n"
   " Blu 00/63",
   content_x + 33,
   content_y,
   DI_GREY_TEXT,
   0
  );

  if(!minimal_help)
  {
    // Write menu
    write_string
    (
      "\x1d- Select color   Alt+D- Default palette\n"
      "R- Increase Red   Alt+R- Decrease Red\n"
      "G- Increase Grn   Alt+G- Decrease Grn\n"
      "B- Increase Blu   Alt+B- Decrease Blu\n"
      "A- Increase All   Alt+A- Decrease All\n"
      "0- Blacken color\tF2- Store color\n"
      "Alt+H- Hide help\tF3- Retrieve color",
      content_x,
      content_y + 6,
      DI_GREY_TEXT,
      1
    );
  }
}

static void palette_editor_update_window_16(void)
{
  int screen_mode = get_screen_mode();
  unsigned int bg_color;
  unsigned int fg_color;
  unsigned int chr;
  unsigned char r;
  unsigned char g;
  unsigned char b;
  int i;
  int x;
  int y;

  // Draw palette bars
  for(bg_color = 0; bg_color < 16; bg_color++)
  {
    x = bg_color * 2 + content_x;
    y = content_y;

    get_rgb(bg_color, &r, &g, &b);

    // The foreground color is white or black in the protected palette.

    fg_color = ((r * .3) + (g * .59) + (b * .11) < 32) ? 31 : 16;

    // FIXME: in SMZX modes 2 and 3, fg_color alters the color here
    // eventually, they should get their own palette editor.
    if(screen_mode >= 2)
      fg_color = 0;

    select_layer(OVERLAY_LAYER);

    // Draw the palette colors
    for(i = 0; i < 8; i++)
    {
      chr = ' ';

      // FIXME: These will look ugly in SMZX mode, so only draw them in regular mode.
      if(!screen_mode)
      {
        if((i == 5) && (bg_color >= 10))
          chr = '1';

        if(i == 6)
          chr = '0' + (bg_color % 10);
      }

      draw_char_mixed_pal_ext(chr, bg_color, fg_color,
       x + (i/4), y + (i%4), PRO_CH);
    }

    select_layer(UI_LAYER);

    // Clear the bottom
    if(minimal_help)
    {
      draw_char('\xC4', DI_GREY, x, y + 4);
      draw_char('\xC4', DI_GREY, x+1, y + 4);
    }
    else
    {
      draw_char(' ', DI_GREY, x, y + 4);
      draw_char(' ', DI_GREY, x+1, y + 4);
    }

    //write_string("\x20\x20", x, y + 4, DI_GREY_TEXT, 0);

    if((int)bg_color == current_color)
    {
      // Write rgb and color #
      write_number(current_color, DI_GREY_TEXT, content_x + 41, content_y, 2, 1, 10);
      write_number(r, DI_GREY_TEXT, content_x + 39, content_y + 1, 2, 1, 10);
      write_number(g, DI_GREY_TEXT, content_x + 39, content_y + 2, 2, 1, 10);
      write_number(b, DI_GREY_TEXT, content_x + 39, content_y + 3, 2, 1, 10);

      // Draw '^^'
      write_string("\x1e\x1e",
       x, y + 4, DI_GREY_TEXT, 0);
    }
  }
}

static int palette_editor_input_16(int key)
{
  if(get_mouse_press())
  {
    int mouse_x, mouse_y;
    get_mouse_position(&mouse_x, &mouse_y);

    // A position in the palette: select the color

    if((mouse_x >= content_x) && (mouse_x < content_x + 32) &&
     (mouse_y >= content_y) && (mouse_y < content_y + 4))
    {
      current_color = (mouse_x - content_x) / 2;
      return -1;
    }
  }

  switch(key)
  {
    case IKEY_LEFT:
    case IKEY_MINUS:
    case IKEY_KP_MINUS:
    {
      if(current_color > 0)
        current_color--;
      break;
    }

    case IKEY_RIGHT:
    case IKEY_EQUALS:
    case IKEY_KP_PLUS:
    {
      if(current_color < 15)
        current_color++;
      break;
    }

    default:
    {
      // Pass the key through to the common handler.
      return key;
    }
  }

  // Update the display.
  return -1;
}

static void palette_editor_redraw_window(void)
{
  restore_screen();
  save_screen();
  palette_editor_redraw_window_16();
}

static void palette_editor_update_window(void)
{
  palette_editor_update_window_16();
}

static int palette_editor_input(int key)
{
  return palette_editor_input_16(key);
}

void palette_editor(struct world *mzx_world)
{
  unsigned char current_r;
  unsigned char current_g;
  unsigned char current_b;
  int refresh_window = 1;
  int refresh_palette = 1;
  int key;

  cursor_off();
  set_context(CTX_PALETTE_EDITOR);
  save_screen();

  do
  {
    if(refresh_window)
    {
      // Draw the palette editor window.
      palette_editor_redraw_window();
      refresh_window = 0;
    }

    if(refresh_palette)
    {
      // Update the palette editor window.
      palette_editor_update_window();
    }
    refresh_palette = 1;

    update_screen();
    update_event_status_delay();

    // Get the current color
    get_rgb(current_color, &current_r, &current_g, &current_b);

    key = get_key(keycode_internal);

    // Exit event -- mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    // Run it through the editor-specific handling first.
    key = palette_editor_input(key);

    // A key of <0 means the display needs to be refreshed.
    if(key < 0)
      continue;

    switch(key)
    {
      case IKEY_q:
      {
        key = IKEY_ESCAPE;
        break;
      }

      case IKEY_h:
      {
        if(get_alt_status(keycode_internal))
        {
          minimal_help ^= 1;
          refresh_window = 1;
        }
        break;
      }

      case IKEY_r:
      {
        if(get_alt_status(keycode_internal))
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

      case IKEY_g:
      {
        if(get_alt_status(keycode_internal))
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

      case IKEY_b:
      {
        if(get_alt_status(keycode_internal))
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

      case IKEY_a:
      {
        if(get_alt_status(keycode_internal))
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

      case IKEY_0:
      {
        current_r = 0;
        current_g = 0;
        current_b = 0;
        set_rgb(current_color, current_r, current_g, current_b);
        update_palette();
        break;
      }

      case IKEY_d:
      {
        if(get_alt_status(keycode_internal))
        {
          default_palette();
        }
        break;
      }

      case IKEY_F2:
      {
        saved_r = current_r;
        saved_g = current_g;
        saved_b = current_b;

        break;
      }

      case IKEY_F3:
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

#ifdef CONFIG_HELPSYS
      case IKEY_F1:
      {
        m_show();
        help_system(mzx_world);
        break;
      }
#endif

      default:
      {
        refresh_palette = 0;
        break;
      }
    }
  } while(key != IKEY_ESCAPE);

  restore_screen();
  pop_context();
}
