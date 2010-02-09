/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
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

// Windowing code- Save/restore screen and draw windows and other elements,
//                 also displays and runs dialog boxes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef __WIN32__
#include <windows.h>
#endif

#include "platform.h"
#include "event.h"
#include "helpsys.h"
#include "sfx.h"
#include "error.h"
#include "intake.h"
#include "window.h"
#include "graphics.h"
#include "const.h"
#include "data.h"
#include "helpsys.h"
#include "robot.h"
#include "board.h"
#include "world.h"
#include "util.h"

// This context stuff was originally in helpsys, but it's actually
// more of a property of the windowing system.

static int contexts[128];
static int curr_context;

// 72 = "No" context link
static int context = 72;

void set_context(int c)
{
  contexts[curr_context++] = context;
  context = c;
}

void pop_context(void)
{
  if(curr_context > 0)
    curr_context--;
  context = contexts[curr_context];
}

#ifdef CONFIG_HELPSYS

int get_context(void)
{
  return context;
}

#endif // CONFIG_HELPSYS

#define NUM_SAVSCR 6

// Big fat hack. This will be initialized to a bunch of 0's, or NULLs.
// Use this to replace the null strings before they get dereferenced.

static char_element screen_storage[NUM_SAVSCR][80 * 25];
int cur_screen = 0; // Current space for save_screen and restore_screen

// Free up memory.

// The following functions do NOT check to see if memory is reserved, in
// the interest of time, space, and simplicity. Make sure you have already
// called window_cpp_init.

// Saves current screen to buffer and increases buffer count. Returns
// non-0 if the buffer for screens is already full. (IE 6 count)

int save_screen(void)
{
  if(cur_screen >= NUM_SAVSCR)
  {
    cur_screen = 0;
    error("Windowing code bug", 2, 4, 0x1F01);
  }

  get_screen(screen_storage[cur_screen]);
  cur_screen++;
  return 0;
}

// Restores top screen from buffer to screen and decreases buffer count.
// Returns non-0 if there are no screens in the buffer.

int restore_screen(void)
{
  if(cur_screen == 0)
    error("Windowing code bug", 2, 4, 0x1F02);
  cur_screen--;
  set_screen(screen_storage[cur_screen]);
  return 0;
}

// Draws an outlined and filled box from x1/y1 to x2/y2. Color is used to
// fill box and for upper left lines. Dark_color is used for lower right
// lines. Corner_color is used for the single chars in the upper right and
// lower left corners. If Corner_color equals 0, these are auto calculated.
// Returns non-0 for invalid parameters. Shadow (black spaces) is drawn and
// clipped if specified. Operation on a size smaller than 3x3 is undefined.
// This routine is highly unoptimized. Center is not filled if fill_center
// is set to 0. (defaults to 1)

int draw_window_box(int x1, int y1, int x2, int y2, int color,
 int dark_color, int corner_color, int shadow, int fill_center)
{
  return draw_window_box_ext(x1, y1, x2, y2, color, dark_color,
   corner_color, shadow, fill_center, 256, 16);
}

int draw_window_box_ext(int x1, int y1, int x2, int y2, int color,
 int dark_color, int corner_color, int shadow, int fill_center,
 int offset, int c_offset)
{
  int t1, t2;
  //Validate parameters
  if((x1 < 0) || (y1 < 0) || (x1 > 79) || (y1 > 24)) return 1;
  if((x2 < 0) || (y2 < 0) || (x2 > 79) || (y2 > 24)) return 1;
  if(x2 < x1)
  {
    t1 = x1;
    x1 = x2;
    x2 = t1;
  }
  if(y2 < y1)
  {
    t1 = y1;
    y1 = y2;
    y2 = t1;
  }

  // Fill center
  if(fill_center)
  {
    for(t1 = x1 + 1; t1 < x2; t1++)
    {
      for(t2 = y1 + 1; t2 < y2; t2++)
      {
        draw_char_ext(' ', color, t1, t2, offset, c_offset);
      }
    }
  }

  // Draw top and bottom edges
  for(t1 = x1 + 1; t1 < x2; t1++)
  {
    draw_char_ext('\xC4', color, t1, y1, offset, c_offset);
    draw_char_ext('\xC4', dark_color, t1, y2, offset, c_offset);
  }

  // Draw left and right edges
  for(t2 = y1 + 1; t2 < y2; t2++)
  {
    draw_char_ext('\xB3', color, x1, t2, offset, c_offset);
    draw_char_ext('\xB3', dark_color, x2, t2, offset, c_offset);
  }

  // Draw corners
  draw_char_ext('\xDA', color, x1, y1, offset, c_offset);
  draw_char_ext('\xD9', dark_color, x2, y2, offset, c_offset);
  if(corner_color)
  {
    draw_char_ext('\xC0', corner_color, x1, y2, offset, c_offset);
    draw_char_ext('\xBF', corner_color, x2, y1, offset, c_offset);
  }
  else
  {
    draw_char_ext('\xC0', color, x1, y2, offset, c_offset);
    draw_char_ext('\xBF', dark_color, x2, y1, offset, c_offset);
  }

  // Draw shadow if applicable
  if(shadow)
  {
    // Right edge
    if(x2 < 79)
    {
      for(t2 = y1 + 1; t2 <= y2; t2++)
      {
        draw_char_ext(' ', 0, x2 + 1, t2, offset, c_offset);
      }
    }

    // Lower edge
    if(y2 < 24)
    {
      for(t1 = x1 + 1; t1 <= x2; t1++)
      {
        draw_char_ext(' ', 0, t1, y2 + 1, offset, c_offset);
      }
    }

    // Lower right corner
    if((y2 < 24) && (x2 < 79))
    {
      draw_char_ext(' ', 0, x2 + 1, y2 + 1, offset, c_offset);
    }
  }
  return 0;
}

// Strings for drawing different dialog box elements.
// All parts are assumed 3 wide.

#define check_on "[\xFB]"
#define check_off "[ ]"
#define radio_on "(\x07)"
#define radio_off "( )"
#define color_blank ' '
#define color_wild '\x3F'
#define color_dot '\xFE'
#define list_button " \x1F "
#define num_buttons " \x18  \x19 "

#define char_sel_arrows_0 '\x1E'
#define char_sel_arrows_1 '\x1F'
#define char_sel_arrows_2 '\x10'
#define char_sel_arrows_3 '\x11'

#define arrow_char '\x10'
#define pc_top_arrow '\x1E'
#define pc_bottom_arrow '\x1F'
#define pc_filler '\xB1'
#define pc_dot '\xFE'
#define pc_meter 219

// Char selection screen colors- Window colors same as dialog, non-current
// characters same as nonactive, current character same as active, arrows
// along edges same as text, title same as title.

// Put up a character selection box. Returns selected, or negative selected
// for ESC, -256 for -0. Returns OUT_OF_WIN_MEM if out of window mem.
// Display is 32 across, 8 down. All work done on given page.

// Mouse support- Click on a character to select it. If it is the current
// character, it exits.

__editor_maybe_static int char_selection_ext(int current, int allow_multichar,
 int *width_ptr, int *height_ptr)
{
  int width = 1;
  int height = 1;
  int x, y;
  int i, i2;
  int key;
  int shifted = 0;
  int highlight_x = 0;
  int highlight_y = 0;
  int start_x = 0;
  int start_y = 0;

  if(allow_multichar)
  {
    width = *width_ptr;
    height = *height_ptr;
  }

  // Save screen
  save_screen();

  if(context == 72)
    set_context(98);
  else
    set_context(context);

  cursor_off();

  // Draw box
  draw_window_box(20, 5, 57, 16, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  // Add title
  if(allow_multichar)
    write_string(" Select characters ", 22, 5, DI_TITLE, 0);
  else
    write_string(" Select a character ", 22, 5, DI_TITLE, 0);

  do
  {
    // Draw character set
    for(x = 0; x < 32; x++)
    {
      for(y = 0; y < 8; y++)
      {
        draw_char_ext(x + (y * 32), DI_NONACTIVE,
         x + 23, y + 7, 0, 16);
      }
    }

    // Calculate x/y
    x = (current & 31) + 23;
    y = (current >> 5) + 7;
    if(get_shift_status(keycode_internal) && allow_multichar)
    {
      if(!shifted)
      {
        highlight_x = x;
        highlight_y = y;
        start_x = x;
        start_y = y;
        shifted = 1;
      }
      else
      {
        start_x = highlight_x;
        start_y = highlight_y;

        if(start_x > x)
        {
          start_x = x;
          width = highlight_x - x + 1;
        }
        else
        {
          width = x - highlight_x + 1;
        }

        if(start_y > y)
        {
          start_y = y;
          height = highlight_y - y + 1;
        }
        else
        {
          height = y - highlight_y + 1;
        }

        for(i = 0; i < height; i++)
        {
          color_line(width, start_x, start_y + i, 0x9F);
        }
      }
    }
    else
    {
      // Highlight active character(s)
      int char_offset;
      int skip = 32 - width;

      if(shifted == 1)
      {
        int size = width * height;

        if((size > 18) || (size == 7) || (size == 11) ||
         (size == 13) || (size == 14) || (size == 16) ||
         (size == 17))
        {
          width = 1;
          height = 1;
        }

        x = start_x;
        y = start_y;
        current = ((y - 7) * 32) + (x - 23);
      }

      char_offset = current;
      shifted = 0;

      for(i = 0; i < height; i++, char_offset += skip)
      {
        for(i2 = 0; i2 < width; i2++, char_offset++)
        {
          draw_char_ext(char_offset, DI_ACTIVE,
           (char_offset & 31) + 23, ((char_offset & 255) >> 5) + 7, 0, 16);
        }
      }
    }
    // Draw arrows
    draw_window_box(22, 6, 55, 15, DI_DARK, DI_MAIN, DI_CORNER, 0, 0);
    draw_char(char_sel_arrows_0, DI_TEXT, x, 15);
    draw_char(char_sel_arrows_1, DI_TEXT, x, 6);
    draw_char(char_sel_arrows_2, DI_TEXT, 22, y);
    draw_char(char_sel_arrows_3, DI_TEXT, 55, y);
    // Write number of character
    write_number(current, DI_MAIN, 53, 16, 3, 0, 10);

    update_screen();

    // Get key
    update_event_status_delay();
    key = get_key(keycode_internal);

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_x >= 23) && (mouse_x <= 54) &&
       (mouse_y >= 7) && (mouse_y <= 14))
      {
        int new_pos = mouse_x - 23 + ((mouse_y - 7) << 5);
        if(current == new_pos)
        {
          pop_context();
          restore_screen();
          return current;
        }
        current = new_pos;
      }
    }

    // Process key

    switch(key)
    {
      case IKEY_ESCAPE:
      {
        // ESC
        pop_context();
        restore_screen();

        if(current == 0)
          return -256;
        else
          return -current;
      }

      case IKEY_SPACE:
      case IKEY_RETURN:
      {
        if(get_shift_status(keycode_internal))
        {
          int size = width * height;

          if((size > 18) || (size == 7) || (size == 11) ||
           (size == 13) || (size == 14) || (size == 16) ||
           (size == 17))
          {
            width = 1;
            height = 1;
          }

          x = start_x;
          y = start_y;
          current = ((y - 7) * 32) + (x - 23);
        }

        // Selected
        pop_context();
        restore_screen();

        if(allow_multichar)
        {
          *width_ptr = width;
          *height_ptr = height;
        }

        return current;
      }

      case IKEY_UP:
      {
        current = (current - 32) & 255;
        break;
      }

      case IKEY_DOWN:
      {
        current = (current + 32) & 255;
        break;
      }

      case IKEY_LEFT:
      {
        current = (current - 1) & 255;
        break;
      }

      case IKEY_RIGHT:
      {
        current = (current + 1) & 255;
        break;
      }

      case IKEY_HOME:
      {
        current = 0;
        break;
      }

      case IKEY_END:
      {
        current = 288 - width - (height * 32);
        break;
      }

      default:
      {
        // If this is from 32 to 255, jump there.
        int key_char = get_key(keycode_unicode);

        if(key_char >= 32)
          current = key_char;

        break;
      }
    }
  } while(1);
}

int char_selection(int current)
{
  return char_selection_ext(current, 0, NULL, NULL);
}

static void construct_element(element *e, int x, int y,
 int width, int height,
 void (* draw_function)(World *mzx_world, dialog *di,
  element *e, int color, int active),
 int (* key_function)(World *mzx_world, dialog *di,
  element *e, int key),
 int (* click_function)(World *mzx_world, dialog *di,
  element *e, int mouse_button, int mouse_x, int mouse_y,
  int new_active),
 int (* drag_function)(World *mzx_world, dialog *di,
  element *e, int mouse_button, int mouse_x, int mouse_y),
 int (* idle_function)(World *mzx_world, dialog *di,
  element *e))
{
  e->x = x;
  e->y = y;
  e->width = width;
  e->height = height;
  e->draw_function = draw_function;
  e->key_function = key_function;
  e->click_function = click_function;
  e->drag_function = drag_function;
  e->idle_function = idle_function;
}

#ifdef CONFIG_EDITOR

//Foreground colors that look nice for each background color
static char fg_per_bk[16] =
 { 15, 15, 15, 15, 15, 15, 15, 0, 15, 15, 0, 0, 15, 15, 0, 0 };

// For list/choice menus-

// List/choice menu colors- Box same as dialog, elements same as
// inactive question, current element same as active question, pointer
// same as text, title same as title.

// Put up a box listing choices. Move pointer and enter to pick. choice
// size is the size in bytes per choice in the array of choices. This size
// includes null terminator and any junk padding. All work is done on
// page given, nopage flipping. Choices are drawn using color_string.
// On ESC, returns negative current choice, unless current choice is 0,
// then returns -32767. Returns OUT_OF_WIN_MEM if out of screen storage space.

// A progress column is displayed directly to the right of the list, using
// the following characters. This is mainly used for mouse support.

// Mouse support- Click on a choice to move there auto, click on progress
// column arrows to move one line, click on progress meter itself to move
// there percentage-wise. If you click on the current choice, it exits.
// If you click on a choice to move there, the mouse cursor moves with it.
int list_menu(const char **choices, int choice_size, const char *title,
 int current, int num_choices, int xpos)
{
  char key_buffer[64];
  int mouse_press;
  int key_position = 0;
  int last_keypress_time = 0;
  int ticks;
  int width = choice_size + 6;
  int key;
  int i;

  cursor_off();

  if(strlen(title) > (unsigned int)choice_size)
    width = strlen(title) + 6;

  // Save screen
  save_screen();

  // Display box

  draw_window_box(xpos, 2, xpos + width - 1, 22, DI_MAIN, DI_DARK,
   DI_CORNER, 1, 1);
  // Add title
  write_string(title, xpos + 3, 2, DI_TITLE, 0);
  draw_char(' ', DI_TITLE, xpos + 2, 2);
  draw_char(' ', DI_TITLE, xpos + 3 + strlen(title), 2);

  // Add pointer
  draw_char(arrow_char, DI_TEXT, xpos + 2, 12);

  // Add meter arrows
  if(num_choices > 1)
  {
    draw_char(pc_top_arrow, DI_PCARROW, xpos + 4 + choice_size, 3);
    draw_char(pc_bottom_arrow,  DI_PCARROW, xpos + 4 + choice_size, 21);
  }
  do
  {
    // Fill over old menu
    draw_window_box(xpos + 3, 3, xpos + 3 + choice_size, 21,
     DI_DARK,DI_MAIN, DI_CORNER, 0, 1);
    // Draw current
    color_string(choices[current], xpos + 4, 12, DI_ACTIVE);
    // Draw above current
    for(i = 1; i < 9; i++)
    {
      if((current - i) < 0)
        break;
      color_string(choices[current - i], xpos + 4,
       12 - i, DI_NONACTIVE);
    }
    // Draw below current
    for(i = 1; i < 9; i++)
    {
      if((current + i) >= num_choices)
        break;
      color_string(choices[current + i], xpos + 4,
       12 + i, DI_NONACTIVE);
    }

    // Draw meter (xpos 9 + choice_size, ypos 4 thru 20)
    if(num_choices > 1)
    {
      for(i = 4; i < 21; i++)
        draw_char(pc_filler, DI_PCFILLER, xpos + 4 + choice_size, i);
      // Add progress dot
      i = (current * 16) / (num_choices - 1);
      draw_char(pc_dot, DI_PCDOT, xpos + 4 + choice_size, i + 4);
    }

    update_screen();

    // Get keypress
    update_event_status_delay();

    // Act upon it
    key = get_key(keycode_internal);

    mouse_press = get_mouse_press_ext();

    if(mouse_press && (mouse_press <= MOUSE_BUTTON_RIGHT))
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);
      // Clicking on- List, column, arrow, or nothing?
      if((mouse_x == xpos + 4 + choice_size )&& (mouse_y > 3) &&
       (mouse_y < 21))
      {
        // Column
        int ny = mouse_y - 4;
        current=(ny * (num_choices - 1)) / 16;
      }
      else

       if((mouse_y > 3) && (mouse_y < 21) &&
       (mouse_x > xpos + 3) && (mouse_x < xpos + 3 + choice_size))
      {
        // List
        if(mouse_y == 12)
          key = IKEY_RETURN;

        current += (mouse_y) - 12;
        if(current < 0)
          current = 0;
        if(current >= num_choices)
          current = num_choices - 1;
        // Move mouse with choices
        warp_mouse(mouse_x, 12);
      }
      else

      if((mouse_x == xpos + 4 + choice_size) &&
       (mouse_y == 3))
      {
        // Top arrow
        if(current > 0)
          current--;
      }
      else

      if((mouse_x == xpos + 4 + choice_size) &&
       (mouse_y == 21))
      {
        // Bottom arrow
        if(current < (num_choices - 1))
         current++;
      }
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELUP)
    {
      key = IKEY_UP;
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELDOWN)
    {
      key = IKEY_DOWN;
    }

    switch(key)
    {
      case IKEY_ESCAPE:
      {
        // ESC
        restore_screen();
        if(current == 0)
          return -32767;
        else
          return -current;
      }

      case IKEY_BACKSPACE:
      {
        // Goto .. if it's there
        for(i = 0; i < num_choices; i++)
        {
          if(!strncmp(choices[i], " .. ", 4))
            break;
        }

        if(i == num_choices)
          break;

        current = i;
        restore_screen();
        return current;
      }

      case IKEY_RETURN:
      {
        // Selected
        restore_screen();
        return current;
      }

      case IKEY_UP:
      {
        if(current > 0)
          current--;
        break;
      }

      case IKEY_DOWN:
      {
        if(current < (num_choices - 1))
          current++;
        break;
      }

      case IKEY_PAGEUP:
      {
        current -= 8;
        if(current < 0)
          current = 0;
        break;
      }

      case IKEY_PAGEDOWN:
      {
        current += 8;
        if(current >= num_choices)
          current = num_choices - 1;
        break;
      }

      case IKEY_HOME:
      {
        current = 0;
        break;
      }

      case IKEY_END:
      {
        current = num_choices - 1;
        break;
      }

      default:
      {
        // Not necessarily invalid. Might be an alphanumeric; seek the
        // sucker.

        if(((key >= IKEY_a) && (key <= IKEY_z)) ||
         ((key >= IKEY_0) && (key <= IKEY_9)))
        {
          ticks = get_ticks();
          if(((ticks - last_keypress_time) >= TIME_SUSPEND) ||
           (key_position == 63))
          {
            // Go back to zero
            key_position = 0;
          }
          last_keypress_time = ticks;

          key_buffer[key_position] = key;
          key_position++;
          key_buffer[key_position] = 0;

          if(get_shift_status(keycode_internal))
          {
            for(i = 0; i < num_choices; i++)
            {
              if((choices[i][0] == ' ') &&
               !strncasecmp(choices[i] + 1, key_buffer,
               key_position))
              {
                // It's good!
                current = i;
                break;
              }
            }
          }
          else
          {
            for(i = 0; i < num_choices; i++)
            {
              if(!strncasecmp(choices[i], key_buffer,
               key_position))
              {
                // It's good!
                current = i;
                break;
              }
            }
          }
        }
        break;
      }
    }
  } while(1);
}

// For color selection screen

#define color_sel_char '\xFE'
#define color_sel_wild '\x3F'

// Color selection screen colors- see char selection screen.

// Put up a color selection box. Returns selected, or negative selected
// for ESC, -512 for -0. Returns OUT_OF_WIN_MEM if out of window mem.
// Display is 16 across, 16 down. All work done on given page. Current
// uses bit 256 for the wild bit.

// Mouse support- Click on a color to select it. If it is the current color,
// it exits.

int color_selection(int current, int allow_wild)
{
  int x, y;
  int key;
  int selected;
  int currx, curry;

  // Save screen
  save_screen();

  if(context == 72)
    set_context(98);
  else
    set_context(context);

  // Ensure allow_wild is 0 or 1
  if(allow_wild)
    allow_wild = 1;

  // Calculate current x/y
  if((current & 256) && (allow_wild))
  {
    // Wild
    current -= 256;
    if(current < 16)
    {
      currx = current;
      curry = 16;
    }
    else
    {
      curry = current - 16;
      currx = 16;
    }
  }
  else

  if(current & 256)
  {
    // Wild when not allowed to be
    current -= 256;
    currx = current & 15;
    curry = current >> 4;
  }
  else
  {
    // Normal
    currx = current & 15;
    curry = current >> 4;
  }

  // Draw box
  draw_window_box(12, 2, 33 + allow_wild, 21 + allow_wild,
   DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  // Add title
  write_string(" Select a color ", 14, 2, DI_TITLE, 0);

  do
  {
    // Draw outer edge
    draw_window_box(14, 3, 31 + allow_wild, 20 + allow_wild,
     DI_DARK, DI_MAIN, DI_CORNER, 0, 0);
    // Draw colors
    for(y = 0; y < 16 + allow_wild; y++)
    {
      for(x = 0; x < 16 + allow_wild; x++)
      {
        if(x == 16)
        {
          if(y == 16)
            draw_char_ext(color_sel_wild, 135, 31, 20, 256, 0);
          else
            draw_char_ext(color_sel_wild, fg_per_bk[y] + y * 16,
             31, y + 4, 256, 0);
        }
        else

        if(y == 16)
        {
          if(x == 0)
            draw_char_ext(color_sel_wild, 128, 15, 20, 256, 0);
          else
            draw_char_ext(color_sel_wild, x, x + 15, 20, 256, 0);
        }
        else
        {
          draw_char_ext(color_sel_char, x + (y * 16), x + 15,
           y + 4, 256, 0);
        }
      }
    }

    // Add selection box
    draw_window_box(currx + 14, curry + 3, currx + 16,
     curry + 5, DI_MAIN, DI_MAIN, DI_MAIN, 0, 0);

    // Write number of color
    if((allow_wild) && ((currx == 16) || (curry == 16)))
    {
      // Convert wild
      if(currx == 16)
      {
        if(curry == 16)
          selected = 288;
        else
          selected = 272 + curry;
      }
      else
      {
        selected = 256 + currx;
      }
    }
    else
    {
      selected = currx + (curry * 16);
    }

    write_number(selected, DI_MAIN, 28 + allow_wild, 21 + allow_wild, 3, 0, 10);
    update_screen();

    // Get key

    update_event_status_delay();
    key = get_key(keycode_internal);

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_x >= 15) && (mouse_x <= (30 + allow_wild)) &&
       (mouse_y >= 4) && (mouse_y <= (19 + allow_wild)))
      {
        int new_x = mouse_x - 15;
        int new_y = mouse_y - 4;
        if((currx == new_x) && (curry == new_y))
          key = IKEY_RETURN;

        currx = new_x;
        curry = new_y;
      }
    }

    // Process
    switch(key)
    {
      // ESC
      case IKEY_ESCAPE:
      case IKEY_SPACE:
      case IKEY_RETURN:
      {
        pop_context();
        // Selected
        restore_screen();
        if((allow_wild) && ((currx == 16) || (curry == 16)))
        {
          // Convert wild
          if(currx == 16)
          {
            if(curry == 16)
              current = 288;
            else
              current = 272 + curry;
          }
          else
          {
            current = 256 + currx;
          }
        }
        else
        {
          current = currx + curry * 16;
        }

        if(key == IKEY_ESCAPE)
        {
          if(current == 0)
            current = -512;
          else
            current =- current;
        }
        return current;
      }

      case IKEY_UP:
      {
        // Up
        if(curry > 0)
          curry--;

        break;
      }

      case IKEY_DOWN:
      {
        // Down
        if(curry < (15 + allow_wild))
          curry++;

        break;
      }

      case IKEY_LEFT:
      {
        // Left
        if(currx > 0)
          currx--;

        break;
      }

      case IKEY_RIGHT:
      {
        // Right
        if(currx < (15 + allow_wild))
          currx++;

        break;
      }

      case IKEY_HOME:
      {
        // Home
        currx = 0;
        curry = 0;
        break;
      }

      case IKEY_END:
      {
        // End
        currx = 15 + allow_wild;
        curry = 15 + allow_wild;
        break;
      }

      default:
      {
        break;
      }
    }
  } while(1);
}

// Short function to display a color as a colored box
void draw_color_box(int color, int q_bit, int x, int y)
{
  // If q_bit is set, there are unknowns
  if(q_bit)
  {
    if(color < 16)
    {
      // Unknown background
      // Use black except for black fg, which uses grey
      if(color == 0)
        color = 8;

      draw_char_ext(color_wild, color, x, y, 256, 0);
      draw_char_ext(color_dot, color, x + 1, y, 256, 0);
      draw_char_ext(color_wild, color, x + 2, y, 256, 0);
    }
    else

    if(color < 32)
    {
      // Unkown foreground
      // Use foreground from array
      color -= 16;
      color = (color << 4) + fg_per_bk[color];
      draw_char_ext(color_wild, color, x, y, 256, 0);
      draw_char_ext(color_wild, color, x + 1, y, 256, 0);
      draw_char_ext(color_wild, color, x + 2, y, 256, 0);
    }
    else
    {
      // Both unknown
      draw_char(color_wild, 8, x, y);
      draw_char(color_wild, 135, x + 1, y);
      draw_char(color_wild, 127, x + 2, y);
    }
  }
  else
  {
    draw_char_ext(color_blank, color, x, y, 256, 0);
    draw_char_ext(color_dot, color, x + 1, y, 256, 0);
    draw_char_ext(color_blank, color, x + 2, y, 256, 0);
  }
}

static void draw_check_box(World *mzx_world, dialog *di, element *e,
 int color, int active)
{
  check_box *src = (check_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int i;

  for(i = 0; i < src->num_choices; i++)
  {
    if(src->results[i])
      write_string(check_on, x, y + i, DI_NONACTIVE, 0);
    else
      write_string(check_off, x, y + i, DI_NONACTIVE, 0);

    write_string(src->choices[i], x + 4, y + i,
     DI_NONACTIVE, 0);
  }

  if(active)
  {
    color_line(src->max_length + 4, x, y + src->current_choice,
     DI_ACTIVE);
  }
}

static void draw_char_box(World *mzx_world, dialog *di, element *e,
 int color, int active)
{
  char_box *src = (char_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int question_len = strlen(src->question) + di->pad_space;

  write_string(src->question, x, y, color, 0);
  draw_char_ext(*(src->result), DI_CHAR,
   x + question_len + 1, y, 0, 16);
  draw_char(' ', DI_CHAR, x + question_len, y);
  draw_char(' ', DI_CHAR, x + question_len + 2, y);
}

static void draw_color_box_element(World *mzx_world, dialog *di,
 element *e, int color, int active)
{
  color_box *src = (color_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int current_color = *(src->result);

  write_string(src->question, x, y, color, 0);
  draw_color_box(current_color & 0xFF, current_color >> 8,
   x + strlen(src->question) + di->pad_space, y);
}

static void draw_board_list(World *mzx_world, dialog *di,
 element *e, int color, int active)
{
  board_list *src = (board_list *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int current_board = *(src->result);

  Board *src_board;
  char board_name[BOARD_NAME_SIZE] = "(none)";

  if(current_board || (!src->board_zero_as_none))
  {
    if(current_board <= mzx_world->num_boards)
    {
      src_board = mzx_world->board_list[current_board];
      strncpy(board_name, src_board->board_name, BOARD_NAME_SIZE - 1);
    }
    else
    {
      strncpy(board_name, "(no board)", BOARD_NAME_SIZE - 1);
    }
    board_name[BOARD_NAME_SIZE - 1] = '\0';
  }

  write_string(src->title, x, y, color, 0);
  fill_line(BOARD_NAME_SIZE + 1, x, y + 1, 32, DI_LIST);

  color_string(board_name, x + 1, y + 1, DI_LIST);

  // Draw button
  write_string(list_button, x + BOARD_NAME_SIZE + 1, y + 1,
   DI_ARROWBUTTON, 0);
}

static int key_check_box(World *mzx_world, dialog *di, element *e, int key)
{
  check_box *src = (check_box *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      src->results[src->current_choice] ^= 1;
      break;
    }

    case IKEY_PAGEUP:
    {
      src->current_choice = 0;
      break;
    }

    case IKEY_LEFT:
    case IKEY_UP:
    {
      if(src->current_choice)
        src->current_choice--;

      break;
    }

    case IKEY_PAGEDOWN:
    {
      src->current_choice = src->num_choices - 1;
      break;
    }

    case IKEY_RIGHT:
    case IKEY_DOWN:
    {
      if(src->current_choice < (src->num_choices - 1))
        src->current_choice++;

      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_char_box(World *mzx_world, dialog *di, element *e, int key)
{
  char_box *src = (char_box *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      int current_char =
       char_selection(*(src->result));

      if((current_char == 0) ||
       ((current_char == 255) && (!src->allow_char_255)))
      {
        current_char = 1;
      }

      if(current_char >= 0)
        *(src->result) = current_char;
      break;
    }

    default:
    {
      int key_char = get_key(keycode_unicode);

      if(key_char >= 32)
      {
        *(src->result) = key_char;
        break;
      }

      return key;
    }
  }

  return 0;
}

static int key_color_box(World *mzx_world, dialog *di, element *e, int key)
{
  color_box *src = (color_box *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      int current_color =
       color_selection(*(src->result), src->allow_wildcard);

      if(current_color >= 0)
        *(src->result) = current_color;
      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_board_list(World *mzx_world, dialog *di, element *e, int key)
{
  board_list *src = (board_list *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      int current_board =
       choose_board(mzx_world, *(src->result),
       src->title, src->board_zero_as_none);

      if(current_board >= 0)
        *(src->result) = current_board;

      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int click_check_box(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  check_box *src = (check_box *)e;

  src->current_choice = mouse_y;
  src->results[src->current_choice] ^= 1;

  return 0;
}

static int click_char_box(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  return IKEY_RETURN;
}

static int click_color_box(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  return IKEY_RETURN;
}

element *construct_check_box(int x, int y, const char **choices,
 int num_choices, int max_length, int *results)
{
  check_box *src = malloc(sizeof(check_box));
  src->current_choice = 0;
  src->choices = choices;
  src->num_choices = num_choices;
  src->results = results;
  src->max_length = max_length;
  construct_element(&(src->e), x, y, max_length + 4, num_choices,
   draw_check_box, key_check_box, click_check_box, NULL, NULL);

  return (element *)src;
}

element *construct_char_box(int x, int y, const char *question,
 int allow_char_255, int *result)
{
  char_box *src = malloc(sizeof(char_box));
  src->question = question;
  src->allow_char_255 = allow_char_255;
  src->result = result;
  construct_element(&(src->e), x, y, strlen(question) + 4,
   1, draw_char_box, key_char_box, click_char_box, NULL, NULL);

  return (element *)src;
}

element *construct_color_box(int x, int y,
 const char *question, int allow_wildcard, int *result)
{
  color_box *src = malloc(sizeof(color_box));
  src->question = question;
  src->allow_wildcard = allow_wildcard;
  src->result = result;
  construct_element(&(src->e), x, y, strlen(question) + 4,
   1, draw_color_box_element, key_color_box, click_color_box,
   NULL, NULL);

  return (element *)src;
}

static int click_board_list(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  return IKEY_RETURN;
}

element *construct_board_list(int x, int y,
 const char *title, int board_zero_as_none, int *result)
{
  board_list *src = malloc(sizeof(board_list));
  src->title = title;
  src->board_zero_as_none = board_zero_as_none;
  src->result = result;
  construct_element(&(src->e), x, y, BOARD_NAME_SIZE + 3,
   2, draw_board_list, key_board_list, click_board_list,
   NULL, NULL);

  return (element *)src;
}

int add_board(World *mzx_world, int current)
{
  Board *new_board;
  char temp_board_str[BOARD_NAME_SIZE];
  // ..get a name...
  save_screen();
  draw_window_box(16, 12, 64, 14, 79, 64, 70, 1, 1);
  write_string("Name for new board:", 18, 13, 78, 0);
  temp_board_str[0] = 0;
  if(intake(mzx_world, temp_board_str, BOARD_NAME_SIZE - 1,
   38, 13, 15, 1, 0, NULL, 0, NULL) == IKEY_ESCAPE)
  {
    restore_screen();
    return -1;
  }
  if(mzx_world->num_boards == mzx_world->num_boards_allocated)
  {
    mzx_world->num_boards_allocated *= 2;
    mzx_world->board_list = realloc(mzx_world->board_list,
     mzx_world->num_boards_allocated * sizeof(Board *));
  }

  mzx_world->num_boards++;
  new_board = create_blank_board();
  mzx_world->board_list[current] = new_board;
  strncpy(new_board->board_name, temp_board_str, BOARD_NAME_SIZE - 1);
  new_board->board_name[BOARD_NAME_SIZE - 1] = '\0';

  // Link global robot!
  new_board->robot_list[0] = mzx_world->global_robot;
  restore_screen();

  return current;
}

// Shell for list_menu()
int choose_board(World *mzx_world, int current, const char *title,
 int board0_none)
{
  char **board_names = calloc(mzx_world->num_boards + 1, sizeof(char *));
  int num_boards = mzx_world->num_boards;
  int i;

  // Go through - blank boards get a (no board) marker. t2 keeps track
  // of the number of boards.
  for(i = 0; i < num_boards; i++)
  {
    board_names[i] = malloc(BOARD_NAME_SIZE);

    if(mzx_world->board_list[i] == NULL)
      strncpy(board_names[i], "(no board)", BOARD_NAME_SIZE - 1);
    else
      strncpy(board_names[i], (mzx_world->board_list[i])->board_name,
       BOARD_NAME_SIZE - 1);

    board_names[i][BOARD_NAME_SIZE - 1] = '\0';
  }

  board_names[i] = malloc(BOARD_NAME_SIZE);

  if((current < 0) || (current >= mzx_world->num_boards))
    current = 0;

  // Add (new board) to bottom
  if(i < MAX_BOARDS)
  {
    strncpy(board_names[i], "(add board)", BOARD_NAME_SIZE - 1);
    board_names[i][BOARD_NAME_SIZE - 1] = '\0';
    i++;
  }

  // Change top to (none) if needed
  if(board0_none)
  {
    strncpy(board_names[0], "(no board)", BOARD_NAME_SIZE - 1);
    board_names[0][BOARD_NAME_SIZE - 1] = '\0';
  }

  // Run the list_menu()
  current = list_menu((const char **)board_names,
   BOARD_NAME_SIZE, title, current, i, 27);

  // New board? (if select no board or add board)
  if((current == num_boards) ||
   ((current >= 0) && (mzx_world->board_list[current] == NULL)))
  {
    current = add_board(mzx_world, current);
  }

  for(i = 0; i < num_boards + 1; i++)
  {
    free(board_names[i]);
  }

  free(board_names);

  if(board0_none && (current == 0))
  {
    return NO_BOARD;
  }

  return current;
}

int choose_file(World *mzx_world, const char **wildcards, char *ret,
 const char *title, int dirs_okay)
{
  return file_manager(mzx_world, wildcards, ret, title, dirs_okay,
   0, NULL, 0, 0, 0);
}

#endif // CONFIG_EDITOR

static void fill_vid_usage(dialog *di, element *e,
 signed char *vid_usage, int vid_fill)
{
  int x = di->x + e->x;
  int y = di->y + e->y;
  signed char *vid_ptr = vid_usage + (y * 80) + x;
  int i;

  for(i = 0; i < e->height; i++)
  {
    memset(vid_ptr, vid_fill, e->width);
    vid_ptr += 80;
  }
}

// Prototype- Internal function that displays a dialog box element.
void display_element(World *mzx_world, int type, int x, int y,
 char *str, int p1, int p2, void *value, int active, int curr_check,
 int set_vid_usage, int space_label);

static void unhighlight_element(World *mzx_world, dialog *di,
 int current_element_num)
{
  (di->elements[current_element_num])->draw_function(mzx_world, di,
   di->elements[current_element_num], DI_NONACTIVE, 0);
}

static void highlight_element(World *mzx_world, dialog *di,
 int current_element_num)
{
  (di->elements[current_element_num])->draw_function(mzx_world, di,
   di->elements[current_element_num], DI_ACTIVE, 1);
}

static int find_first_element(World *mzx_world, dialog *di,
 int current_element_num)
{
  int i;

  unhighlight_element(mzx_world, di, current_element_num);

  for(i = 0; i < di->num_elements; i++)
  {
    if((di->elements[i])->key_function)
      return i;
  }

  highlight_element(mzx_world, di, i);

  return -1;
}

static int find_last_element(World *mzx_world, dialog *di,
 int current_element_num)
{
  int i;

  unhighlight_element(mzx_world, di, current_element_num);

  for(i = di->num_elements - 1; i >= 0; i--)
  {
    if((di->elements[i])->key_function)
      return i;
  }

  highlight_element(mzx_world, di, i);

  return -1;
}

static int change_current_element(World *mzx_world, dialog *di,
 int current_element_num, int displacement)
{
  int increment = 1;
  int i = 0;

  unhighlight_element(mzx_world, di, current_element_num);

  if(displacement < 0)
    increment = -1;

  while(i != displacement)
  {
    current_element_num += increment;

    if(current_element_num < 0)
      current_element_num = di->num_elements - 1;

    if(current_element_num == di->num_elements)
      current_element_num = 0;

    if((di->elements[current_element_num])->key_function)
      i += increment;
  }

  highlight_element(mzx_world, di, current_element_num);

  return current_element_num;
}

int run_dialog(World *mzx_world, dialog *di)
{
  int mouse_press;
  int x = di->x;
  int y = di->y;
  int title_x_offset = x + (di->width / 2) - (strlen(di->title) / 2);
  element *current_element = di->elements[di->current_element];

  int current_element_num = di->current_element;
  signed char vid_usage[2000];
  int current_key, new_key;
  int i;

  if(context == 72)
    set_context(98);
  else
    set_context(context);

  cursor_off();

  save_screen();

  di->done = 0;

  // Draw main box and title

  draw_window_box(x, y, x + di->width - 1,
   y + di->height - 1, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);

  write_string(di->title, title_x_offset, y, DI_TITLE, 0);
  draw_char(' ', DI_TITLE, title_x_offset - 1, y);
  draw_char(' ', DI_TITLE, title_x_offset + strlen(di->title), y);

  memset(vid_usage, -1, 2000);

  // Draw elements and set vid usage
  for(i = 0; i < di->num_elements; i++)
  {
    unhighlight_element(mzx_world, di, i);
    fill_vid_usage(di, di->elements[i], vid_usage, i);
  }

  do
  {
    highlight_element(mzx_world, di, current_element_num);
    update_screen();

    current_element = di->elements[current_element_num];
    update_event_status_delay();
    current_key = get_key(keycode_internal);

    new_key = 0;

    if(current_element->idle_function)
    {
      new_key =
       current_element->idle_function(mzx_world, di,
       current_element);

      if(new_key > 0)
        current_key = new_key;
    }

    mouse_press = get_mouse_press_ext();

    if(get_mouse_drag() &&
     (mouse_press <= MOUSE_BUTTON_RIGHT) &&
     (current_element->drag_function))
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      new_key = current_element->drag_function(mzx_world,
       di, current_element, mouse_press,
       mouse_x - di->x - current_element->x,
       mouse_y - di->y - current_element->y);

      if(new_key > 0)
        current_key = new_key;
    }
    else

    if((mouse_press && (mouse_press <= MOUSE_BUTTON_RIGHT))
     || (new_key == -1))
    {
      do
      {
        int mouse_x, mouse_y;
        int element_under;
        get_mouse_position(&mouse_x, &mouse_y);

        element_under = vid_usage[(mouse_y * 80) + mouse_x];
        new_key = 0;

        if((element_under != -1) &&
         (element_under != current_element_num) &&
         ((di->elements[element_under])->key_function))
        {
          unhighlight_element(mzx_world, di, current_element_num);
          current_element_num = element_under;
          current_element = di->elements[element_under];
          highlight_element(mzx_world, di, element_under);

          new_key = current_element->click_function(mzx_world, di,
           current_element, mouse_press,
           mouse_x - di->x - current_element->x,
           mouse_y - di->y - current_element->y, 1);

          if(new_key > 0)
            current_key = new_key;
        }
        else

        if((element_under != -1) &&
         (current_element->click_function))
        {
          current_element_num = element_under;
          current_element = di->elements[element_under];

          if(current_element->click_function)
          {
            new_key = current_element->click_function(mzx_world, di,
             current_element, mouse_press,
             mouse_x - di->x - current_element->x,
             mouse_y - di->y - current_element->y, 0);

            if(new_key > 0)
              current_key = new_key;
          }
        }
      } while(new_key == -1);
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELUP)
    {
      current_key = IKEY_UP;
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELDOWN)
    {
      current_key = IKEY_DOWN;
    }

    if(current_element->key_function && current_key)
    {
      current_key =
       current_element->key_function(mzx_world, di,
       current_element, current_key);
    }

    di->current_element = current_element_num;

    if(di->idle_function)
    {
      current_key =
       di->idle_function(mzx_world, di, current_key);

      current_element_num = di->current_element;
    }

    switch(current_key)
    {
      case IKEY_TAB: // Tab
      {
        if(get_shift_status(keycode_internal))
        {
          current_element_num = change_current_element(mzx_world,
           di, current_element_num, -1);
        }
        else
        {
          current_element_num = change_current_element(mzx_world,
           di, current_element_num, 1);
        }
        break;
      }

      case IKEY_SPACE:
      case IKEY_RETURN:
      case IKEY_PAGEDOWN:
      case IKEY_RIGHT:
      case IKEY_DOWN:
      {
        current_element_num = change_current_element(mzx_world,
         di, current_element_num, 1);
        break;
      }

      case IKEY_PAGEUP:
      case IKEY_LEFT:
      case IKEY_UP:
      {
        current_element_num = change_current_element(mzx_world,
         di, current_element_num, -1);
        break;
      }

      case IKEY_HOME:
      {
        current_element_num = find_first_element(mzx_world, di,
         current_element_num);
        break;
      }

      case IKEY_END:
      {
        current_element_num = find_last_element(mzx_world, di,
         current_element_num);
        break;
      }

      case IKEY_ESCAPE: // ESC
      {
        // Restore screen, set current, and return -1
        pop_context();
        return -1;
      }

#ifdef CONFIG_HELPSYS
      case IKEY_F1: // F1
      {
        help_system(mzx_world);
        break;
      }
#endif

      default:
      {
        break;
      }
    }
  } while(di->done != 1);

  pop_context();

  return di->return_value;
}

static int find_entry(const char **choices, char *name, int total_num)
{
  int current_entry;
  int cmpval = 0;
  int name_length = strlen(name);

  for(current_entry = 0; current_entry < total_num; current_entry++)
  {
    // Hack to avoid seeking to drive letters under Windows. This
    // is a terrible place for it, and I'd like to tear it out, but
    // it would likely upset a lot of the existing code.
    // (note: Exo added this hack, not me!)
#ifdef __WIN32__
    if(choices[current_entry][1] == ':')
      continue;
    else
#endif

    cmpval = strncasecmp(name, choices[current_entry], name_length);

    if(cmpval == 0)
    {
      return current_entry;
    }
  }

  return -1;
}

// "Member" functions for GUI elements

static void draw_label(World *mzx_world, dialog *di, element *e,
 int color, int active)
{
  label *src = (label *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;

  color_string(src->text, x, y, DI_TEXT);
}

static void draw_input_box(World *mzx_world, dialog *di, element *e,
 int color, int active)
{
  input_box *src = (input_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int question_length = strlen(src->question) + di->pad_space;

  write_string(src->question, x, y, color, 0);
  fill_line(src->max_length + 1, x + question_length, y,
   32, DI_INPUT);
  write_string(src->result, x + question_length, y,
   DI_INPUT, 0);
}

static void draw_radio_button(World *mzx_world, dialog *di, element *e,
 int color, int active)
{
  radio_button *src = (radio_button *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int i;

  for(i = 0; i < src->num_choices; i++)
  {
    if(i != *(src->result))
    {
      write_string(radio_off, x, y + i, DI_NONACTIVE, 0);
      write_string(src->choices[i], x + 4, y + i,
       DI_NONACTIVE, 0);
    }
    else
    {
      write_string(radio_on, x, y + *(src->result),
       DI_NONACTIVE, 0);
      write_string(src->choices[i], x + 4, y + i,
       DI_NONACTIVE, 0);
    }
  }

  if(active)
  {
    color_line(src->max_length + 4, x, y + *(src->result),
     DI_ACTIVE);
  }
}

static void draw_button(World *mzx_world, dialog *di, element *e,
 int color, int active)
{
  button *src = (button *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;

  if(active)
    color = DI_ACTIVEBUTTON;
  else
    color = DI_BUTTON;

  write_string(src->label, x + 1, y, color, 0);
  draw_char(' ', color, x, y);
  draw_char(' ', color, x + strlen(src->label) + 1, y);
}

static void draw_number_box(World *mzx_world, dialog *di,
 element *e, int color, int active)
{
  number_box *src = (number_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int increment = 1;
  int i;

  if(src->mult_five)
    increment = 5;

  write_string(src->question, x, y, color, 0);

  x += strlen(src->question) + di->pad_space;

  if((src->lower_limit == 1) && (src->upper_limit < 10) &&
   (increment == 1))
  {
    // Draw a number line
    for(i = 1; i <= src->upper_limit; i++)
    {
      if(i == *(src->result))
        draw_char('0' + i, DI_ACTIVE, x + i - 1, y);
      else
        draw_char('0' + i, DI_NONACTIVE, x + i - 1, y);
    }
  }
  else
  {
    // Draw a number
    char num_buffer[32];
    sprintf(num_buffer, "%d", *(src->result) * increment);
    fill_line(7, x, y, 32, DI_NUMERIC);
    write_string(num_buffer, x + 6 - strlen(num_buffer), y,
     DI_NUMERIC, 0);
    // Buttons
    write_string(num_buttons, x + 7, y, DI_ARROWBUTTON, 0);
  }
}

#define MAX_NAME_BUFFER 512

static void draw_list_box(World *mzx_world, dialog *di,
 element *e, int color, int active)
{
  list_box *src = (list_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;
  int choice_length = src->choice_length;
  int num_choices = src->num_choices;
  int num_choices_visible = src->num_choices_visible;
  int current_choice = *(src->result);
  int scroll_offset = src->scroll_offset;
  const char **choices = src->choices;
  int i, num_draw;
  int draw_width = choice_length;
  int current_in_window = current_choice - scroll_offset;
  char name_buffer[MAX_NAME_BUFFER];

  num_draw = num_choices_visible;

  if(num_choices_visible >= num_choices)
    num_draw = num_choices;

  current_in_window = current_choice - scroll_offset;

  // Draw choices
  for(i = 0; i < num_draw; i++)
  {
    fill_line(choice_length, x, y + i, 32, DI_LIST);
    strncpy(name_buffer, choices[scroll_offset + i], MAX_NAME_BUFFER - 1);
    name_buffer[MAX_NAME_BUFFER - 1] = '\0';
    name_buffer[draw_width - 1] = 0;
    color_string(name_buffer, x, y + i, DI_LIST);
  }

  for(; i < num_choices_visible; i++)
  {
    fill_line(choice_length, x, y + i, 32, DI_LIST);
  }

  if(num_choices)
  {
    if(active)
      color = DI_ACTIVELIST;
    else
      color = DI_SEMIACTIVELIST;

    fill_line(choice_length, x, y + current_in_window,
     32, color);

    strncpy(name_buffer, choices[current_choice], MAX_NAME_BUFFER - 1);

    if(draw_width < MAX_NAME_BUFFER)
      name_buffer[draw_width - 1] = '\0';
    else
      name_buffer[MAX_NAME_BUFFER - 1] = '\0';

    color_string(name_buffer, x,
     y + current_in_window, color);
  }

  if(num_choices > num_choices_visible)
  {
    int side_length = num_choices_visible - 2;
    int progress_bar_size = (side_length * side_length +
     (side_length / 2)) / num_choices;
    int progress_bar_offset = ((scroll_offset *
     (side_length - progress_bar_size)) +
     ((num_choices - num_choices_visible) / 2)) /
     (num_choices - num_choices_visible) + y + 1;

    draw_char(pc_top_arrow, DI_PCARROW, x + choice_length, y);
    draw_char(pc_bottom_arrow, DI_PCARROW, x + choice_length,
     y + side_length + 1);

    for(i = 0; i < side_length; i++)
    {
      draw_char(pc_filler, DI_PCFILLER, x + choice_length, y + i + 1);
    }

    for(i = 0; i < progress_bar_size; i++,
     progress_bar_offset++)
    {
      draw_char(pc_meter, DI_PCDOT, x + choice_length,
       progress_bar_offset);
    }
  }
}

static int key_input_box(World *mzx_world, dialog *di, element *e, int key)
{
  input_box *src = (input_box *)e;

  if(get_alt_status(keycode_internal) && (key == IKEY_t) &&
   di->sfx_test_for_input)
  {
    // Play a sfx
    clear_sfx_queue();
    play_str(src->result, 0);
  }

  return key;
}

static int key_radio_button(World *mzx_world, dialog *di, element *e, int key)
{
  radio_button *src = (radio_button *)e;

  switch(key)
  {
    case IKEY_PAGEUP:
    {
      *(src->result) = 0;
      break;
    }

    case IKEY_LEFT:
    case IKEY_UP:
    {
      if(*(src->result))
        (*(src->result))--;

      break;
    }

    case IKEY_PAGEDOWN:
    {
      *(src->result) = src->num_choices - 1;
      break;
    }

    case IKEY_RIGHT:
    case IKEY_DOWN:
    {
      if(*(src->result) < (src->num_choices - 1))
        (*(src->result))++;

      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_button(World *mzx_world, dialog *di, element *e, int key)
{
  button *src = (button *)e;

  switch(key)
  {
    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      // Flag that the dialog is done processing
      di->done = 1;
      di->return_value = src->return_value;
      break;
    }

    default:
    {
      return key;
    }
  }

  return 0;
}

static int key_number_box(World *mzx_world, dialog *di, element *e, int key)
{
  number_box *src = (number_box *)e;
  int increment_value = 0;
  int current_value = *(src->result);

  switch(key)
  {
    case IKEY_HOME:
    {
      *(src->result) = src->lower_limit;
      break;
    }

    case IKEY_END:
    {
      *(src->result) = src->upper_limit;
      break;
    }

    case IKEY_RIGHT:
    case IKEY_UP:
    {
      if(get_alt_status(keycode_internal) ||
        get_ctrl_status(keycode_internal))
      {
        increment_value = 10;
      }
      else
      {
        increment_value = 1;
      }

      break;
    }

    case IKEY_PAGEUP:
    {
      increment_value = 100;
      break;
    }

    case IKEY_LEFT:
    case IKEY_DOWN:
    {
      if(get_alt_status(keycode_internal) ||
       get_ctrl_status(keycode_internal))
      {
        increment_value = -10;
      }
      else
      {
        increment_value = -1;
      }

      break;
    }

    case IKEY_PAGEDOWN:
    {
      increment_value = -100;
      break;
    }

    case IKEY_BACKSPACE:
    {
      Sint32 result = current_value / 10;
      if(result < src->lower_limit)
        result = src->lower_limit;

      *(src->result) = result;
    }

    default:
    {
      int key_char = get_key(keycode_unicode);

      if((key >= '0') && (key <= '9'))
      {
        if(current_value == src->upper_limit)
        {
          current_value = (key_char - '0');
        }
        else
        {
          current_value = (current_value * 10) +
           (key_char - '0');
        }

        if(src->mult_five)
          current_value -= current_value % 5;

        if(current_value < src->lower_limit)
          current_value = src->lower_limit;

        if(current_value > src->upper_limit)
          current_value = src->upper_limit;

        *(src->result) = current_value;
        break;
      }

      return key;
    }
  }

  if(increment_value)
  {
    current_value += increment_value;

    if(current_value < src->lower_limit)
      current_value = src->lower_limit;

    if(current_value > src->upper_limit)
      current_value = src->upper_limit;

    *(src->result) = current_value;
  }

  return 0;
}

static int key_list_box(World *mzx_world, dialog *di, element *e, int key)
{
  list_box *src = (list_box *)e;
  int current_choice = *(src->result);
  int num_choices = src->num_choices;
  int num_choices_visible = src->num_choices_visible;

  switch(key)
  {
    case IKEY_UP:
    {
      if(current_choice)
      {
        if(src->scroll_offset == current_choice)
          (src->scroll_offset)--;
        current_choice--;
      }
      break;
    }

    case IKEY_DOWN:
    {
      if(current_choice < (num_choices - 1))
      {
        if(current_choice ==
         (src->scroll_offset + num_choices_visible - 1))
        {
          (src->scroll_offset)++;
        }
        current_choice++;
      }
      break;
    }

    case IKEY_LEFT:
    {
      return IKEY_LEFT;
    }

    case IKEY_RIGHT:
    {
      return IKEY_RIGHT;
    }

    case IKEY_HOME:
    {
      current_choice = 0;
      src->scroll_offset = 0;
      break;
    }

    case IKEY_END:
    {
      current_choice = num_choices - 1;
      src->scroll_offset =
       num_choices - num_choices_visible;
      if(src->scroll_offset < 0)
        src->scroll_offset = 0;
      break;
    }

    case IKEY_PAGEUP:
    {
      current_choice -= num_choices_visible;
      src->scroll_offset -= num_choices_visible;

      if(src->scroll_offset < 0)
      {
        src->scroll_offset = 0;
      }

      if(current_choice < 0)
      {
        current_choice = 0;
      }
      break;
    }

    case IKEY_PAGEDOWN:
    {
      current_choice += num_choices_visible;
      src->scroll_offset += num_choices_visible;

      if(src->scroll_offset + num_choices_visible >
       num_choices)
      {
        src->scroll_offset =
         num_choices - num_choices_visible;
        if(src->scroll_offset < 0)
          src->scroll_offset = 0;
      }

      if(current_choice >= num_choices)
      {
        current_choice = num_choices - 1;
      }

      break;
    }

    case IKEY_SPACE:
    case IKEY_RETURN:
    {
      di->return_value = src->return_value;
      di->done = 1;
      break;
    }

    default:
    {
      int key_char = get_key(keycode_unicode);
      if(!get_alt_status(keycode_internal) && (key_char >= 32))
      {
        char *key_buffer = src->key_buffer;
        int key_position = src->key_position;
        int last_keypress_time = src->last_keypress_time;
        int ticks = get_ticks();
        int new_choice;

        if(((ticks - last_keypress_time) >= TIME_SUSPEND) ||
         (key_position == 63))
        {
          // Go back to zero
          key_position = 0;
        }

        src->last_keypress_time = ticks;
        key_buffer[key_position] = key_char;
        key_position++;
        key_buffer[key_position] = 0;
        src->key_position = key_position;

        new_choice =
         find_entry(src->choices, key_buffer, num_choices);

        if(new_choice != -1)
          current_choice = new_choice;

        if((current_choice < src->scroll_offset) ||
         (current_choice >= (src->scroll_offset +
         num_choices_visible)))
        {
          src->scroll_offset =
           current_choice - num_choices_visible / 2;

          if(src->scroll_offset < 0)
            src->scroll_offset = 0;

          if(src->scroll_offset + num_choices_visible >
           num_choices)
          {
            src->scroll_offset =
             num_choices - num_choices_visible;
            if(src->scroll_offset < 0)
              src->scroll_offset = 0;
          }
        }
      }
      else
      {
        return key;
      }
      break;
    }
  }

  *(src->result) = current_choice;

  return 0;
}

static int click_input_box(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  input_box *src = (input_box *)e;
  int question_len = strlen(src->question);
  int start_x = mouse_x - question_len;
  int x = di->x + e->x;
  int y = di->y + e->y;

  if(start_x >= 0)
  {
    return intake(mzx_world, src->result, src->max_length, x +
     question_len + di->pad_space, y, DI_INPUT, 2,
     src->input_flags, &start_x, 0, NULL);
  }
  else
  {
    return 0;
  }
}

static int click_radio_button(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  radio_button *src = (radio_button *)e;
  *(src->result) = mouse_y;

  return 0;
}

static int click_button(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  if(!new_active)
  {
    di->done = 1;
    return IKEY_RETURN;
  }

  return 0;
}

static int click_number_box(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  number_box *src = (number_box *)e;
  mouse_x -= strlen(src->question) + 7;

  if((src->lower_limit == 1) &&
    (src->upper_limit < 10) && (!src->mult_five))
  {
    // Select number IF on the number line itself
    mouse_x -= strlen(src->question);
    if((mouse_x < src->upper_limit) && (mouse_x >= 0))
      *(src->result) = mouse_x + 1;
  }
  else

  if((mouse_x >= 0) && (mouse_x <= 2))
  {
    return IKEY_UP;
  }
  else

  if((mouse_x >= 3) && (mouse_y <= 5))
  {
    return IKEY_DOWN;
  }

  return 0;
}

static int click_list_box(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y,
 int new_active)
{
  list_box *src = (list_box *)e;
  int scroll_offset = src->scroll_offset;
  int choice_length = src->choice_length;
  int num_choices = src->num_choices;
  int num_choices_visible = src->num_choices_visible;
  int current_in_window = *(src->result) - scroll_offset;

  src->clicked_scrollbar = 0;

  if((num_choices > num_choices_visible) &&
   (mouse_x == choice_length))
  {
    if(mouse_y == 0)
      return IKEY_UP;

    if(mouse_y == (num_choices_visible - 1))
      return IKEY_DOWN;

    src->clicked_scrollbar = 1;

    scroll_offset =
     (mouse_y - 1) *
     (num_choices + ((num_choices_visible - 2) / 2)) /
     (num_choices_visible - 2) - (num_choices_visible / 2);

    if(scroll_offset < 0)
      scroll_offset = 0;

    if(scroll_offset + num_choices_visible >
     num_choices)
    {
      scroll_offset = num_choices - num_choices_visible;
      if(scroll_offset < 0)
        scroll_offset = 0;
    }

    src->scroll_offset = scroll_offset;
    *(src->result) = scroll_offset + (num_choices_visible / 2);
  }
  else
  {
    if(src->num_choices)
    {
      int offset = (mouse_y - current_in_window);
      if(offset)
      {
        int current_choice = *(src->result) + offset;
        if(current_choice < src->num_choices)
          *(src->result) = current_choice;
      }
      else
      {
        if(!new_active)
        {
          di->return_value = src->return_value;
          di->done = 1;
        }
      }
    }
  }

  return 0;
}

static int drag_list_box(World *mzx_world, dialog *di,
 element *e, int mouse_button, int mouse_x, int mouse_y)
{
  list_box *src = (list_box *)e;

  if(src->clicked_scrollbar)
  {
    if((mouse_y > 0) &&
     (mouse_y < src->num_choices_visible - 1))
    {
      click_list_box(mzx_world, di, e, mouse_button,
       src->choice_length, mouse_y, 0);
      src->clicked_scrollbar = 1;
    }
  }
  else

  if((mouse_y >= 0) && (mouse_y < e->height) &&
   (mouse_x >= 0))
  {
    if( ((mouse_x < e->width - 1) &&
     (mouse_y != *(src->result) - src->scroll_offset)) ||
     ((mouse_x == e->width - 1) && mouse_button)  )
    {
      return click_list_box(mzx_world, di, e, mouse_button,
       mouse_x, mouse_y, 0);
    }
  }

  return 0;
}

static int idle_input_box(World *mzx_world, dialog *di, element *e)
{
  input_box *src = (input_box *)e;
  int x = di->x + e->x;
  int y = di->y + e->y;

  return intake(mzx_world, src->result, src->max_length, x +
   strlen(src->question) + di->pad_space, y, DI_INPUT, 2,
   src->input_flags, NULL, 0, NULL);
}

void construct_dialog(dialog *src, const char *title, int x, int y,
 int width, int height, element **elements, int num_elements,
 int start_element)
{
  src->title = title;
  src->x = x;
  src->y = y;
  src->width = width;
  src->height = height;
  src->elements = elements;
  src->num_elements = num_elements;
  src->sfx_test_for_input = 0;
  src->pad_space = 0;
  src->done = 0;
  src->return_value = 0;
  src->current_element = start_element;
  src->idle_function = NULL;
}

__editor_maybe_static void construct_dialog_ext(dialog *src,
 const char *title, int x, int y, int width, int height,
 element **elements, int num_elements, int sfx_test_for_input,
 int pad_space, int start_element,
 int (* idle_function)(World *mzx_world, dialog *di, int key))
{
  src->title = title;
  src->x = x;
  src->y = y;
  src->width = width;
  src->height = height;
  src->elements = elements;
  src->num_elements = num_elements;
  src->sfx_test_for_input = sfx_test_for_input;
  src->pad_space = pad_space;
  src->done = 0;
  src->return_value = 0;
  src->current_element = start_element;
  src->idle_function = idle_function;
}

void destruct_dialog(dialog *src)
{
  int i;

  for(i = 0; i < src->num_elements; i++)
  {
    free(src->elements[i]);
  }

  restore_screen();
}

element *construct_label(int x, int y, const char *text)
{
  label *src = malloc(sizeof(label));
  src->text = text;
  construct_element(&(src->e), x, y, strlen(text), 1,
   draw_label, NULL, NULL, NULL, NULL);

  return (element *)src;
}

__editor_maybe_static element *construct_input_box(int x, int y,
 const char *question, int max_length, int input_flags, char *result)
{
  input_box *src = malloc(sizeof(input_box));
  src->question = question;
  src->input_flags = input_flags;
  src->max_length = max_length;
  src->result = result;
  construct_element(&(src->e), x, y,
   max_length + strlen(question) + 1, 1,
   draw_input_box, key_input_box, click_input_box,
   NULL, idle_input_box);

  return (element *)src;
}

element *construct_radio_button(int x, int y,
 const char **choices, int num_choices, int max_length, int *result)
{
  radio_button *src = malloc(sizeof(radio_button));
  src->choices = choices;
  src->num_choices = num_choices;
  src->result = result;
  src->max_length = max_length;
  construct_element(&(src->e), x, y, max_length + 4,
   num_choices, draw_radio_button, key_radio_button,
   click_radio_button, NULL, NULL);

  return (element *)src;
}

element *construct_button(int x, int y, const char *label,
 int return_value)
{
  button *src = malloc(sizeof(button));
  src->label = label;
  src->return_value = return_value;

  construct_element(&(src->e), x, y, strlen(src->label) + 2,
   1, draw_button, key_button, click_button, NULL, NULL);

  return (element *)src;
}

element *construct_number_box(int x, int y,
 const char *question, int lower_limit, int upper_limit,
 int mult_five, int *result)
{
  number_box *src = malloc(sizeof(number_box));
  int width;

  src->question = question;
  src->lower_limit = lower_limit;
  src->upper_limit = upper_limit;
  src->mult_five = mult_five;
  src->result = result;
  width = strlen(question) + 1;

  if((lower_limit == 1) && (upper_limit < 10))
    width += upper_limit - 1;
  else
    width += 13;

  construct_element(&(src->e), x, y, width, 1,
   draw_number_box, key_number_box, click_number_box, NULL, NULL);

  return (element *)src;
}

__editor_maybe_static element *construct_list_box(int x, int y,
 const char **choices, int num_choices, int num_choices_visible,
 int choice_length, int return_value, int *result)
{
  int scroll_offset = *result - (num_choices_visible / 2);

  list_box *src = malloc(sizeof(list_box));
  src->choices = choices;
  src->num_choices = num_choices;
  src->num_choices_visible = num_choices_visible;
  src->choice_length = choice_length;
  src->result = result;
  src->return_value = return_value;
  src->key_position = 0;
  src->last_keypress_time = 0;
  src->clicked_scrollbar = 0;

  if(scroll_offset < 0)
    scroll_offset = 0;

  if(scroll_offset + num_choices_visible > num_choices)
  {
    scroll_offset =
      num_choices - num_choices_visible;

    if(scroll_offset < 0)
      scroll_offset = 0;
  }

  src->scroll_offset = scroll_offset;

  construct_element(&(src->e), x, y, choice_length + 1,
   num_choices_visible, draw_list_box, key_list_box,
   click_list_box, drag_list_box, NULL);

  return (element *)src;
}

// Shell for run_dialog()
int confirm(World *mzx_world, const char *str)
{
  dialog di;
  element *elements[2];
  int dialog_result;

  elements[0] = construct_button(15, 2, "OK", 0);
  elements[1] = construct_button(37, 2, "Cancel", 1);
  construct_dialog(&di, str, 10, 9, 60, 5, elements,
   2, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  return dialog_result;
}

// Shell for run_dialog()
int ask_yes_no(World *mzx_world, char *str)
{
  dialog di;
  element *elements[2];
  int dialog_result;
  int dialog_width = 60;
  int str_length = strlen(str);

  int yes_button_pos;
  int no_button_pos;

  // Is this string too long for the normal ask dialog?
  if(str_length > 56)
  {
    // Is the string small enough for a resized ask dialog?
    if(str_length <= 76)
    {
      // Use a bigger ask dialog to fit the string
      dialog_width = str_length + 4;
      // If the dialog width is odd, bump it up to the next
      // even number, otherwise it will look uneven
      if((dialog_width % 2) == 1)
      {
        dialog_width++;
      }
    }
    else
    {
      // Clip the string at the maximum (76 characters)
      str[76] = 0;
      dialog_width = 80;
    }
  }

  // The 'Yes' button should be about 1/3 along, compensating for
  // button width
  yes_button_pos = (dialog_width - 4) / 3 - 2;

  // The 'No' button should be about 2/3 along
  no_button_pos = (dialog_width - 4) * 2 / 3 - 1;

  elements[0] = construct_button(yes_button_pos, 2, "Yes", 0);
  elements[1] = construct_button(no_button_pos, 2, "No", 1);

  construct_dialog(&di, str, 40 - dialog_width / 2, 9, dialog_width,
    5, elements, 2, 0);

  dialog_result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  return dialog_result;
}

static int sort_function(const void *dest_str_ptr, const void *src_str_ptr)
{
  char *dest_str = *((char **)dest_str_ptr);
  char *src_str = *((char **)src_str_ptr);

  if(src_str[0] == '.')
    return 1;

  if(dest_str[0] == '.')
    return -1;

  return strcasecmp(dest_str, src_str);
}

// Shell for list_menu() (copies file chosen to ret and returns -1 for ESC)
// dirs_okay of 1 means drive and directory changing is allowed.

#define FILESEL_MAX_ELEMENTS  64
#define FILESEL_BASE_ELEMENTS 7
#define FILESEL_FILE_LIST     0
#define FILESEL_DIR_LIST      1
#define FILESEL_FILENAME      2
#define FILESEL_OKAY_BUTTON   3
#define FILESEL_CANCEL_BUTTON 4
#define FILESEL_FILES_LABEL   5
#define FILESEL_DIRS_LABEL    6

static int file_dialog_function(World *mzx_world, dialog *di, int key)
{
  int current_element_num = di->current_element;
  element *current_element = di->elements[current_element_num];

  switch(current_element_num)
  {
    case FILESEL_DIR_LIST:
    case FILESEL_FILE_LIST:
    {
      list_box *src = (list_box *)current_element;
      input_box *dest =
       (input_box *)di->elements[FILESEL_FILENAME];
      element *e = (element *)dest;

      if(src->num_choices)
      {
        const char *file_name = src->choices[*(src->result)];

        if(get_alt_status(keycode_internal))
        {
          switch(key)
          {
            case IKEY_r:
            {
              char *new_name = malloc(MAX_PATH);
              int width = 29;

              strncpy(new_name, file_name + 56, MAX_PATH - 1);
              new_name[MAX_PATH - 1] = '\0';

              if(current_element_num == FILESEL_DIR_LIST)
                width = 14;

              intake(mzx_world, new_name, width,
               src->e.x + di->x, src->e.y + di->y + *(src->result)
               - src->scroll_offset, DI_ACTIVELIST, 2, 0, NULL, 0,
               NULL);
              rename(file_name + 56, new_name);

              di->done = 1;
              di->return_value = 5;

              free(new_name);
              return 0;
            }

            case IKEY_d:
            {
              di->done = 1;
              di->return_value = 4;
              break;
            }

            case IKEY_n:
            {
              di->done = 1;
              di->return_value = 3;
              break;
            }
          }
        }

        if(current_element_num == FILESEL_FILE_LIST)
        {
          strncpy(dest->result, file_name + 56, dest->max_length - 1);
          dest->result[dest->max_length - 1] = '\0';
          e->draw_function(mzx_world, di, e, DI_NONACTIVE, 0);
        }
      }
      else
      {
        dest->result[0] = 0;
        e->draw_function(mzx_world, di, e, DI_NONACTIVE, 0);
      }

      if(key == IKEY_DELETE)
      {
        if(current_element_num == FILESEL_DIR_LIST)
          di->return_value = 6;
        else
          di->return_value = 4;

        di->done = 1;
      }

      if(key == IKEY_BACKSPACE)
      {
        di->done = 1;
        di->return_value = -2;
        return 0;
      }

      break;
    }

    case FILESEL_FILENAME:
    {
      input_box *src = (input_box *)current_element;
      list_box *dest =
       (list_box *)di->elements[FILESEL_FILE_LIST];
      element *e = (element *)dest;
      int current_choice = *(dest->result);
      int new_choice =
       find_entry(dest->choices, src->result, dest->num_choices);

      if(new_choice != -1)
        current_choice = new_choice;

      if((current_choice < dest->scroll_offset) ||
       (current_choice >= (dest->scroll_offset +
       dest->num_choices_visible)))
      {
        dest->scroll_offset =
         current_choice - (dest->num_choices_visible / 2);

        if(dest->scroll_offset < 0)
          dest->scroll_offset = 0;

        if(dest->scroll_offset + dest->num_choices_visible >
         dest->num_choices)
        {
          dest->scroll_offset =
           dest->num_choices - dest->num_choices_visible;
          if(dest->scroll_offset < 0)
            dest->scroll_offset = 0;
        }
      }

      *(dest->result) = current_choice;
      e->draw_function(mzx_world, di, e, DI_NONACTIVE, 0);

      if(key == IKEY_RETURN)
      {
        di->done = 1;
        di->return_value = 0;
        return 0;
      }

      break;
    }
  }

  return key;
}

static void remove_files(char *directory_name, int remove_recursively)
{
  char *current_dir_name;
  struct stat file_info;
  dir_t *current_dir;
  char *file_name;

  current_dir = dir_open(directory_name);
  if(!current_dir)
    return;

  current_dir_name = malloc(MAX_PATH);
  file_name = malloc(PATH_BUF_LEN);

  getcwd(current_dir_name, MAX_PATH);
  chdir(directory_name);

  while(1)
  {
    if(dir_get_next_entry(current_dir, file_name) < 0)
      break;

    if(stat(file_name, &file_info) < 0)
      continue;

    if(!S_ISDIR(file_info.st_mode))
    {
      unlink(file_name);
      continue;
    }

    if(remove_recursively && strcmp(file_name, ".") &&
     strcmp(file_name, ".."))
    {
      remove_files(file_name, 1);
      rmdir(file_name);
    }
  }

  chdir(current_dir_name);

  free(file_name);
  free(current_dir_name);

  dir_close(current_dir);
}

__editor_maybe_static int file_manager(World *mzx_world,
 const char **wildcards, char *ret, const char *title, int dirs_okay,
 int allow_new, element **dialog_ext, int num_ext, int ext_height,
 int allow_dir_change)
{
  dir_t *current_dir;
  char *file_name;
  struct stat file_info;
  char *current_dir_name;
  char current_dir_short[56];
  int current_dir_length;
  char *previous_dir_name;
  int total_filenames_allocated;
  int total_dirnames_allocated;
  char **file_list;
  char **dir_list;
  int num_files;
  int num_dirs;
  int file_name_length;
  int ext_pos = -1;
  int chosen_file, chosen_dir;
  int dialog_result = 1;
  int return_value = 1;
  dialog di;
  element *elements[FILESEL_MAX_ELEMENTS];
  int list_length = 17 - ext_height;
  int last_element = FILESEL_FILE_LIST;
  int i;

#ifdef __WIN32__
  int drive_letter_bitmap;
#endif

  // These are stack heavy so put them on the heap
  // This function is not performance sensitive anyway.
  file_name = malloc(PATH_BUF_LEN);
  current_dir_name = malloc(MAX_PATH);
  previous_dir_name = malloc(MAX_PATH);

  if(allow_new)
    last_element = FILESEL_FILENAME;

  getcwd(previous_dir_name, MAX_PATH);

  while(return_value == 1)
  {
    total_filenames_allocated = 32;
    total_dirnames_allocated = 32;
    file_list = calloc(32, sizeof(char *));
    dir_list = calloc(32, sizeof(char *));

    num_files = 0;
    num_dirs = 0;
    chosen_file = 0;
    chosen_dir = 0;

    getcwd(current_dir_name, MAX_PATH);
    current_dir = dir_open(current_dir_name);

    while(current_dir)
    {
      if(dir_get_next_entry(current_dir, file_name) < 0)
        break;

      file_name_length = strlen(file_name);

      if((stat(file_name, &file_info) >= 0) &&
       ((file_name[0] != '.') || (file_name[1] == '.')))
      {
        if(S_ISDIR(file_info.st_mode))
        {
          if(dirs_okay)
          {
            dir_list[num_dirs] = malloc(file_name_length + 1);
            strncpy(dir_list[num_dirs], file_name, file_name_length);
            dir_list[num_dirs][file_name_length] = '\0';
            num_dirs++;
          }
        }
        else
        {
          // Must match one of the wildcards, also ignore the .
          if(file_name_length >= 4)
          {
            if(file_name[file_name_length - 4] == '.')
              ext_pos = file_name_length - 4;
            else if(file_name[file_name_length - 3] == '.')
              ext_pos = file_name_length - 3;
            else
              ext_pos = 0;

            for(i = 0; wildcards[i] != NULL; i++)
            {
              if(!strcasecmp(file_name + ext_pos, wildcards[i]))
              {
                file_list[num_files] = malloc(56 + file_name_length + 1);

                if(!strcasecmp(file_name + file_name_length - 4, ".mzx"))
                {
                  FILE *mzx_file = fopen(file_name, "rb");

                  memset(file_list[num_files], ' ', 55);
                  strncpy(file_list[num_files], file_name, file_name_length);
                  file_list[num_files][file_name_length] = ' ';
                  fread(file_list[num_files] + 30, 1, 24, mzx_file);
                  file_list[num_files][55] = 0;
                  fclose(mzx_file);
                }
                else
                {
                  strncpy(file_list[num_files], file_name, file_name_length);
                  file_list[num_files][file_name_length] = '\0';
                }
                strncpy(file_list[num_files] + 56, file_name,
                 file_name_length);
                (file_list[num_files] + 56)[file_name_length] = '\0';

                num_files++;
                break;
              }
            }
          }
        }
      }

      if(num_files == total_filenames_allocated)
      {
        file_list = realloc(file_list, sizeof(char *) *
         total_filenames_allocated * 2);
        memset(file_list + total_filenames_allocated, 0,
         sizeof(char *) * total_filenames_allocated);
        total_filenames_allocated *= 2;
      }

      if(num_dirs == total_dirnames_allocated)
      {
        dir_list = realloc(dir_list, sizeof(char *) *
         total_dirnames_allocated * 2);
        memset(dir_list + total_dirnames_allocated, 0,
         sizeof(char *) * total_dirnames_allocated);
        total_dirnames_allocated *= 2;
      }
    }

    dir_close(current_dir);

    qsort(file_list, num_files, sizeof(char *), sort_function);
    qsort(dir_list, num_dirs, sizeof(char *), sort_function);

#ifdef __WIN32__
    if(dirs_okay)
    {
      drive_letter_bitmap = GetLogicalDrives();

      for(i = 0; i < 32; i++)
      {
        if(drive_letter_bitmap & (1 << i))
        {
          dir_list[num_dirs] = malloc(3);
          sprintf(dir_list[num_dirs], "%c:", 'A' + i);

          num_dirs++;

          if(num_dirs == total_dirnames_allocated)
          {
            dir_list = realloc(dir_list, sizeof(char *) *
             total_dirnames_allocated * 2);
            memset(dir_list + total_dirnames_allocated, 0,
             sizeof(char *) * total_dirnames_allocated);
            total_dirnames_allocated *= 2;
          }
        }
      }
    }
#endif

    current_dir_length = strlen(current_dir_name);

    if(current_dir_length > 55)
    {
      memcpy(current_dir_short, "...", 3);
      memcpy(current_dir_short + 3,
       current_dir_name + current_dir_length - 52, 52);
      current_dir_short[55] = 0;
    }
    else
    {
      memcpy(current_dir_short, current_dir_name,
       current_dir_length + 1);
    }

    elements[FILESEL_FILE_LIST] =
     construct_list_box(2, 2, (const char **)file_list, num_files,
     list_length, 55, 1, &chosen_file);
    elements[FILESEL_DIR_LIST] =
     construct_list_box(59, 2, (const char **)dir_list, num_dirs,
     list_length, 15, 2, &chosen_dir);
    elements[FILESEL_FILENAME] =
     construct_input_box(2, list_length + 3, "", 55,
     0, ret);
    elements[FILESEL_OKAY_BUTTON] =
     construct_button(60, list_length + 3, "OK", 0);
    elements[FILESEL_CANCEL_BUTTON] =
     construct_button(65, list_length + 3, "Cancel", -1);
    elements[FILESEL_FILES_LABEL] =
     construct_label(2, 1, current_dir_short);
    elements[FILESEL_DIRS_LABEL] =
     construct_label(59, 1, "Directories");

    if(num_ext)
    {
      memcpy(elements + FILESEL_BASE_ELEMENTS, dialog_ext,
       sizeof(element *) * num_ext);
    }

    construct_dialog_ext(&di, title, 2, 1, 76, 23,
     elements, FILESEL_BASE_ELEMENTS + num_ext, 0, 0,
     last_element, file_dialog_function);

    dialog_result = run_dialog(mzx_world, &di);

    switch(dialog_result)
    {
      case -2:
      {
        if(dirs_okay)
          chdir("..");

        break;
      }

      case -1:
      {
        return_value = -1;
        break;
      }

      case 0:
      case 1:
      {
        int stat_result;
        char *path;

        path = malloc(MAX_PATH);
        get_path(ret, path, MAX_PATH);
        stat_result = stat(ret, &file_info);

        if((stat_result >= 0) && S_ISDIR(file_info.st_mode))
        {
          chdir(ret);
          free(path);
          break;
        }

        if(allow_new)
        {
          if(path[0])
          {
            int path_len = strlen(path);
            chdir(path);
            memmove(ret, ret + path_len + 1, strlen(ret) - path_len);
          }
          else

          if((stat_result >= 0) && (allow_new == 1))
          {
            char confirm_string[512];
            sprintf(confirm_string, "%s already exists, overwrite?", ret);
            if(!ask_yes_no(mzx_world, confirm_string))
              return_value = 0;
          }
          else
          {
            if(ret[0])
              return_value = 0;
          }
        }
        else

        if(stat_result >= 0)
        {
          if(path[0])
            chdir(path);

          return_value = 0;
        }
        else
        {
          ret[0] = 0;
        }

        free(path);
        break;
      }

      case 2:
      {
        chdir(dir_list[chosen_dir]);
        break;
      }

      case 3:
      {
        element *b_elements[3];
        int b_dialog_result;
        char *new_name;
        dialog b_di;

        new_name = malloc(MAX_PATH);

        b_elements[0] = construct_input_box(2, 2,
         "New directory name: ", 32, 0, new_name);
        b_elements[1] = construct_button(15, 4, "OK", 0);
        b_elements[2] = construct_button(37, 4, "Cancel", -1);

        construct_dialog(&b_di, "Create New Directory",
         11, 8, 57, 7, b_elements, 3, 0);

        b_dialog_result = run_dialog(mzx_world, &b_di);
        destruct_dialog(&b_di);

        if(!b_dialog_result)
        {
          if(stat(new_name, &file_info) < 0)
          {
#ifdef __WIN32__
            mkdir(new_name);
#else
            mkdir(new_name, 0777);
#endif
          }
          else
          {
            char error_str[512];
            sprintf(error_str, "%s already exists.", ret);
            error(error_str, 2, 4, 0x0000);
          }
        }

        free(new_name);
        break;
      }

      case 4:
      {
        if((stat(ret, &file_info) >= 0) && strcmp(ret, "..") &&
         strcmp(ret, "."))
        {
          char *confirm_string;

          confirm_string = malloc(MAX_PATH);
          snprintf(confirm_string, MAX_PATH,
           "Delete %s - are you sure?", ret);

          if(!confirm(mzx_world, confirm_string))
            unlink(ret);

          free(confirm_string);
        }
        break;
      }

      case 5:
      {
        break;
      }

      case 6:
      {
        char *file_name_ch = dir_list[chosen_dir];
        if(strcmp(file_name_ch, ".."))
        {
          char confirm_string[70];
          snprintf(confirm_string, 70, "Delete %s: are you sure?",
           file_name_ch);
          confirm_string[69] = 0;

          if(!confirm(mzx_world, confirm_string))
          {
            if(!ask_yes_no(mzx_world,
             (char *)"Delete subdirectories recursively?"))
            {
              remove_files(file_name_ch, 1);
              rmdir(file_name_ch);
            }
            else
            {
              remove_files(file_name_ch, 0);
            }
          }
        }
        break;
      }
    }

    // Hack - don't allow the added elements to be destroyed
    di.num_elements = FILESEL_BASE_ELEMENTS;
    last_element = di.current_element;

    for(i = 0; i < total_filenames_allocated; i++)
    {
      if(file_list[i])
        free(file_list[i]);
    }
    free(file_list);

    for(i = 0; i < total_dirnames_allocated; i++)
    {
      if(dir_list[i])
        free(dir_list[i]);
    }
    free(dir_list);

    destruct_dialog(&di);
  }

  // Now we can destroy the additional elements
  for(i = 0; i < num_ext; i++)
  {
    free(dialog_ext[i]);
  }

  if(return_value == -1)
  {
    ret[0] = 0;
    chdir(previous_dir_name);
  }

  if(!allow_dir_change)
  {
    getcwd(current_dir_name, MAX_PATH);
    if(strcmp(previous_dir_name, current_dir_name))
    {
      char *ret_buffer;

      ret_buffer = malloc(MAX_PATH);
      strncpy(ret_buffer, ret, MAX_PATH - 1);
      ret_buffer[MAX_PATH - 1] = '\0';

#ifdef __WIN32__
      snprintf(ret, MAX_PATH, "%s\\%s", current_dir_name, ret_buffer);
#else
      snprintf(ret, MAX_PATH, "%s/%s", current_dir_name, ret_buffer);
#endif

      chdir(previous_dir_name);
      free(ret_buffer);
    }
  }

  free(previous_dir_name);
  free(current_dir_name);
  free(file_name);

  return return_value;
}

int choose_file_ch(World *mzx_world, const char **wildcards, char *ret,
 const char *title, int dirs_okay)
{
  return file_manager(mzx_world, wildcards, ret, title, dirs_okay,
   0, NULL, 0, 0, 1);
}

int new_file(World *mzx_world, const char **wildcards, char *ret,
 const char *title, int dirs_okay)
{
  return file_manager(mzx_world, wildcards, ret, title, dirs_okay,
   1, NULL, 0, 0, 0);
}
