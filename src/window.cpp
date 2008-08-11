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

// Windowing code- Save/restore screen and draw windows and other elements,
//                 also displays and runs dialog boxes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "event.h"
#include "helpsys.h"
#include "hexchar.h"
#include "sfx.h"
#include "error.h"
#include "intake.h"
#include "window.h"
#include "graphics.h"
#include "const.h"
#include "data.h"
#include "helpsys.h"

#define NUM_SAVSCR 6

// Big fat hack. This will be initialized to a bunch of 0's, or NULLs.
// Use this to replace the null strings before they get dereferenced.

void *null_storage[256];

Uint8 screen_storage[NUM_SAVSCR][80 * 25 * 2];
int cur_screen = 0; // Current space for save_screen and restore_screen
signed char vid_usage[80 * 25]; // Mouse video usage array (for dialogs)

// Free up memory.

// The following functions do NOT check to see if memory is reserved, in
// the interest of time, space, and simplicity. Make sure you have already
// called window_cpp_init.

// Saves current screen to buffer and increases buffer count. Returns
// non-0 if the buffer for screens is already full. (IE 6 count)

int save_screen()
{
  if(cur_screen >= NUM_SAVSCR)
  {
    cur_screen = 0;
    error("Windowing code bug", 2, 20, 0x1F01);
  }

  get_screen(screen_storage[cur_screen]);
  cur_screen++;
  return 0;
}

// Restores top screen from buffer to screen and decreases buffer count.
// Returns non-0 if there are no screens in the buffer.

int restore_screen()
{
  if(cur_screen == 0)
    error("Windowing code bug", 2, 20, 0x1F02);
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
        draw_char(' ', color, t1, t2);
      }
    }
  }

  // Draw top and bottom edges
  for(t1 = x1 + 1; t1 < x2; t1++)
  {
    draw_char('Ä', color, t1, y1);
    draw_char('Ä', dark_color, t1, y2);
  }

  // Draw left and right edges
  for(t2 = y1 + 1; t2 < y2; t2++)
  {
    draw_char('³', color, x1, t2);
    draw_char('³', dark_color, x2, t2);
  }

  // Draw corners
  draw_char('Ú', color, x1, y1);
  draw_char('Ù', dark_color, x2, y2);
  if(corner_color)
  {
    draw_char('À', corner_color, x1, y2);
    draw_char('¿', corner_color, x2, y1);
  }
  else
  {
    draw_char('À', color, x1, y2);
    draw_char('¿', dark_color, x2, y1);
  }

  // Draw shadow if applicable
  if(shadow)
  {
    // Right edge
    if(x2 < 79)
    {
      for(t2 = y1 + 1; t2 <= y2; t2++)
      {
        draw_char(' ', 0, x2 + 1, t2);
      }
    }

    // Lower edge
    if(y2 < 24)
    {
      for(t1 = x1 + 1; t1 <= x2; t1++)
      {
        draw_char(' ', 0, t1, y2 + 1);
      }
    }

    // Lower right corner
    if((y2 < 24) && (x2 < 79))
    {
      draw_char(' ', 0, x2 + 1, y2 + 1);
    }
  }
  return 0;
}

// Strings for drawing different dialog box elements.
// All parts are assumed 3 wide.

#define check_on "[û]"
#define check_off "[ ]"
#define radio_on "()"
#define radio_off "( )"
#define color_blank ' '
#define color_wild '?'
#define color_dot 'þ'
#define list_button "  "
#define num_buttons "    "

//Foreground colors that look nice for each background color
char fg_per_bk[16] =
 { 15, 15, 15, 15, 15, 15, 15, 0, 15, 15, 0, 0, 15, 15, 0, 0 };

// For list/choice menus-

#define arrow_char ''

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

#define pc_top_arrow ''
#define pc_bottom_arrow ''
#define pc_filler '±'
#define pc_dot 'þ'

// Mouse support- Click on a choice to move there auto, click on progress
// column arrows to move one line, click on progress meter itself to move
// there percentage-wise. If you click on the current choice, it exits.
// If you click on a choice to move there, the mouse cursor moves with it.

int list_menu(char *choices, int choice_size, char *title,
 int current, int num_choices, int xpos)
{
  char key_buffer[64];
  int key_position = 0;
  int last_keypress_time = 0;
  int ticks;
  int width = choice_size + 6, t1;
  int key;

  cursor_off();

  if(strlen(title) > (unsigned int)choice_size)
    width = strlen(title) + 6;

  // Save screen
  if(save_screen()) return OUT_OF_WIN_MEM;

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
    draw_char(pc_bottom_arrow,DI_PCARROW, xpos + 4 + choice_size, 21);
  }
  do
  {
    // Fill over old menu
    draw_window_box(xpos + 3, 3, xpos + 3 + choice_size, 21,
     DI_DARK,DI_MAIN, DI_CORNER, 0, 1);
    // Draw current
    color_string(choices + (choice_size * current), xpos + 4, 12, DI_ACTIVE);
    // Draw above current
    for(t1 = 1; t1 < 9; t1++)
    {
      if((current - t1) < 0) break;
      color_string(choices + (choice_size * (current - t1)), xpos + 4,
       12 - t1, DI_NONACTIVE);
    }
    // Draw below current
    for(t1 = 1; t1 < 9; t1++)
    {
      if((current + t1) >= num_choices) break;
      color_string(choices + (choice_size * (current + t1)), xpos + 4,
       12 + t1, DI_NONACTIVE);
    }
    // Draw meter (xpos 9+choice_size, ypos 4 thru 20)
    if(num_choices > 1)
    {
      for(t1 = 4; t1 < 21; t1++)
        draw_char(pc_filler, DI_PCFILLER, xpos + 4 + choice_size, t1);
      // Add progress dot
      t1 = (current * 16) / (num_choices - 1);
      draw_char(pc_dot, DI_PCDOT, xpos + 4 + choice_size, t1 + 4);
    }

    update_screen();

    // Get keypress
    update_event_status_delay();

    // Act upon it
    key = get_key(keycode_SDL);

    if(get_mouse_press())
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
          key = SDLK_RETURN;

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

    switch(key)
    {
      case SDLK_ESCAPE:
        // ESC
        restore_screen();
        if(current == 0)
         return -32767;
        else
         return -current;

      case SDLK_BACKSPACE:
      {
        int i;
        // Goto .. if it's there
        for(i = 0; i < num_choices; i++)
        {
          if(!strncmp(choices + (i * choice_size), " .. ", 4))
            break;
        }

        if(i == num_choices)
          break;

        current = i;
      }

      case SDLK_KP_ENTER:
      case SDLK_RETURN:
        // Selected
        restore_screen();
        return current;

      case SDLK_UP:
        //Up
        if(current > 0) current--;
        break;

      case SDLK_DOWN:
        //Down
        if(current < (num_choices - 1)) current++;
        break;

      case SDLK_PAGEUP:
        //Page Up
        current -= 8;
        if(current < 0) current=0;
        break;

      case SDLK_PAGEDOWN:
        //Page Down
        current += 8;
        if(current >= num_choices) current = num_choices - 1;
        break;

      case SDLK_HOME:
        //Home
        current = 0;
        break;

      case SDLK_END:
        //End
        current = num_choices - 1;
        break;

      default:
        // Not necessarily invalid. Might be an alphanumeric; seek the
        // sucker.

        if(((key >= SDLK_a) && (key <= SDLK_z)) ||
         ((key >= SDLK_0) && (key <= SDLK_9)))
        {
          ticks = SDL_GetTicks();
          if(((ticks - last_keypress_time) >= TIME_SUSPEND) ||
           (key_position == 63))
          {
            // Go back to zero
            key_position = 0;
          }
          last_keypress_time = ticks;

          key_buffer[key_position] = key;
          key_position++;

          int i;

          if(get_shift_status(keycode_SDL))
          {
            for(i = 0; i < num_choices; i++)
            {
              if((choices[i * choice_size] == ' ') &&
               !strncasecmp(choices + (i * choice_size) + 1, key_buffer,
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
              if(!strncasecmp(choices + (i * choice_size), key_buffer,
               key_position))
              {
                // It's good!
                current = i;
                break;
              }
            }
          }
        }

      case 0:
        break;
    }
    // Loop
  } while(1);
}

// For char selection screen

#define char_sel_arrows_0 ''
#define char_sel_arrows_1 ''
#define char_sel_arrows_2 ''
#define char_sel_arrows_3 ''

// Char selection screen colors- Window colors same as dialog, non-current
// characters same as nonactive, current character same as active, arrows
// along edges same as text, title same as title.

// Put up a character selection box. Returns selected, or negative selected
// for ESC, -256 for -0. Returns OUT_OF_WIN_MEM if out of window mem.
// Display is 32 across, 8 down. All work done on given page.

// Mouse support- Click on a character to select it. If it is the current
// character, it exits.
int char_selection(int current)
{
  int x, y;
  int key;
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
  write_string(" Select a character ", 22, 5, DI_TITLE, 0);

  // Draw character set
  for(x = 0; x < 32; x++)
  {
    for(y = 0; y < 8; y++)
    {
      draw_char(x + (y * 32), DI_NONACTIVE, x + 23, y + 7);
    }
  }

  do
  {
    // Calculate x/y
    x = (current & 31) + 23;
    y = (current >> 5) + 7;
    // Highlight active character
    draw_char(current, DI_ACTIVE, x, y);
    // Draw arrows
    draw_window_box(22, 6, 55, 15, DI_DARK, DI_MAIN, DI_CORNER, 0, 0);
    draw_char(char_sel_arrows_0, DI_TEXT, x, 15);
    draw_char(char_sel_arrows_1, DI_TEXT, x, 6);
    draw_char(char_sel_arrows_2, DI_TEXT, 22, y);
    draw_char(char_sel_arrows_3, DI_TEXT, 55, y);
    // Write number of character
    write_number(current, DI_MAIN, 53, 16, 3);

    update_screen();

    // Get key
    update_event_status_delay();
    key = get_key(keycode_SDL);

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

    // Erase marks
    draw_char(current, DI_NONACTIVE, x, y);
    // Process key

    switch(key)
    {
      case SDLK_ESCAPE:
      {
        // ESC
        pop_context();
        restore_screen();

        if(current == 0)
          return -256;
        else
          return -current;
      }

      case SDLK_SPACE:
      case SDLK_KP_ENTER:
      case SDLK_RETURN:
      {
        // Selected
        pop_context();
        restore_screen();
        return current;
      }

      case SDLK_UP:
      {
        if(current > 31)
          current -= 32;

        break;
      }

      case SDLK_DOWN:
      {
        if(current < 224)
          current += 32;

        break;
      }

      case SDLK_LEFT:
      {
        if(x > 23)
          current--;

        break;
      }

      case SDLK_RIGHT:
      {
        if(x < 54)
          current++;

        break;
      }

      case SDLK_HOME:
      {
        current = 0;
        break;
      }

      case SDLK_END:
      {
        current = 255;
        break;
      }

      default:
      {
        // If this is from 32 to 255, jump there.
        if(key > 31)
          current = get_key(keycode_unicode);
      }

      case SDLK_LSHIFT:
      case SDLK_RSHIFT:
      case 0:
      {
        break;
      }
    }
  } while(1);
}

// For color selection screen

#define color_sel_char 'þ'
#define color_sel_wild '?'

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
            draw_char(color_sel_wild, 135, 31, 20);
          else
            draw_char(color_sel_wild, fg_per_bk[y] + y * 16,
             31, y + 4);
        }
        else

        if(y == 16)
        {
          if(x == 0)
            draw_char(color_sel_wild, 128, 15, 20);
          else
            draw_char(color_sel_wild, x, x + 15, 20);
        }
        else
        {
          draw_char(color_sel_char, x + (y * 16), x + 15, y + 4);
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

    write_number(selected, DI_MAIN, 28 + allow_wild, 21 + allow_wild, 3);
    update_screen();

    // Get key

    update_event_status_delay();
    key = get_key(keycode_SDL);

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
          key = SDLK_RETURN;

        currx = new_x;
        curry = new_y;
      }
    }

    // Process
    switch(key)
    {
      // ESC
      case SDLK_ESCAPE:
      case SDLK_SPACE:
      case SDLK_KP_ENTER:
      case SDLK_RETURN:
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

        if(key == SDLK_ESCAPE)
        {
          if(current == 0)
            current = -512;
          else
            current =- current;
        }
        return current;
      }

      case SDLK_UP:
      {
        // Up
        if(curry > 0)
          curry--;

        break;
      }

      case SDLK_DOWN:
      {
        // Down
        if(curry < (15 + allow_wild))
          curry++;

        break;
      }

      case SDLK_LEFT:
      {
        // Left
        if(currx > 0)
          currx--;

        break;
      }

      case SDLK_RIGHT:
      {
        // Right
        if(currx < (15 + allow_wild))
          currx++;

        break;
      }

      case SDLK_HOME:
      {
        // Home
        currx = 0;
        curry = 0;
        break;
      }

      case SDLK_END:
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

      draw_char(color_wild, color, x, y);
      draw_char(color_dot, color, x + 1, y);
      draw_char(color_wild, color, x + 2, y);
    }
    else

    if(color < 32)
    {
      // Unkown foreground
      // Use foreground from array
      color -= 16;
      color = (color << 4) + fg_per_bk[color];
      draw_char(color_wild, color, x, y);
      draw_char(color_wild, color, x + 1, y);
      draw_char(color_wild, color, x + 2, y);
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
    draw_char(color_blank, color, x, y);
    draw_char(color_dot, color, x + 1, y);
    draw_char(color_blank, color, x + 2, y);
  }
}

// Prototype- Internal function that displays a dialog box element.
void display_element(World *mzx_world, int type, int x, int y,
 char *str, int p1, int p2, void *value, int active, int curr_check,
 int set_vid_usage);

// The code that runs the dialog boxes
// Mouse support- Click to move/select/choose.
int run_dialog(World *mzx_world, dialog *di, char reset_curr,
 char sfx_alt_t)
{
  int t1, t2 = -1, key;
  int tlng;
  // Current radio button or check box
  int curr_check = 0;
  // Use these to save time and coding hassle
  int num_e = di->num_elements;
  int curr_e = di->curr_element;
  int x1 = di->x1;
  int y1 = di->y1;
  // X pos of title
  int titlex = x1 + ((di->x2 - x1 + 1) >> 1) -
   (strlen(di->title) >> 1);

  // Fix broken elements that have null storage
  if(di->element_storage == NULL)
    di->element_storage = null_storage;
  if(di->element_param1s == NULL)
    di->element_param1s = (int *)null_storage;
  if(di->element_param2s == NULL)
    di->element_param2s = (int *)null_storage;

  // Save screen
  save_screen();

  if(context == 72)
    set_context(98);
  else
    set_context(context);

  // Clear mouse usage array
  memset(vid_usage, -1, 2000);

  // Turn cursor off
  cursor_off();

  // Draw main box and title
  draw_window_box(x1, y1, di->x2, di->y2, DI_MAIN,DI_DARK, DI_CORNER,
   1, 1);

  write_string(di->title, titlex, y1, DI_TITLE, 0);
  draw_char(' ', DI_TITLE, titlex - 1, y1);
  draw_char(' ', DI_TITLE, titlex + strlen(di->title), y1);

  // Reset current
  if(reset_curr) di->curr_element = curr_e = 0;
  // Initial draw of elements, as well as init of vid_usage
  for(t1 = 0; t1 < num_e; t1++)
  {
    if(t1 == curr_e)
    {
      display_element(mzx_world, di->element_types[t1], x1 + di->element_xs[t1],
       y1 + di->element_ys[t1], di->element_strs[t1], di->element_param1s[t1],
       di->element_param2s[t1], di->element_storage[t1], 1, 0, t1 + 1);
    }
    else
    {
      display_element(mzx_world, di->element_types[t1], x1 + di->element_xs[t1],
       y1 + di->element_ys[t1], di->element_strs[t1], di->element_param1s[t1],
       di->element_param2s[t1], di->element_storage[t1], 0, 0, t1 + 1);
    }
  }

  // Loop of selection
  do
  {
    // Check current...
    do
    {
      if(curr_e >= num_e) curr_e = 0;
      t1 = di->element_types[curr_e];
      if((t1 == DE_BOX) || (t1 == DE_TEXT) || (t1 == DE_LINE))
        curr_e++;
      else
        break;
    } while(1);

    if(t1 == DE_CHECK)
    {
      if(curr_check >= di->element_param1s[curr_e])
        curr_check = 0;
    }

    // Highlight current...
    display_element(mzx_world, t1, x1 + di->element_xs[curr_e],
     y1 + di->element_ys[curr_e], di->element_strs[curr_e],
     di->element_param1s[curr_e], di->element_param2s[curr_e],
     di->element_storage[curr_e], 1, curr_check, 0);

    update_event_status_delay();

    // Get key (through intake for DE_INPUT)
    if(t1 == DE_INPUT)
    {
      key = intake((char *)di->element_storage[curr_e],
       di->element_param1s[curr_e], x1 + di->element_xs[curr_e] +
       strlen(di->element_strs[curr_e]), y1 + di->element_ys[curr_e],
       DI_INPUT, 2, di->element_param2s[curr_e]);
    }
    else
    {
      update_screen();
      key = get_key(keycode_SDL);
    }
      // Process key (t1 still holds element type)

    if(get_mouse_press() || (key == -1))
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);
      int id_over = vid_usage[mouse_x + (mouse_y * 80)];

      // Is it over anything important?
      if(id_over != -1)
      {
        // Yes.
        // Get id number
        if(id_over != curr_e)
        {
          // Unhighlight old element
          display_element(mzx_world, t1, x1 + di->element_xs[curr_e],
           y1 + di->element_ys[curr_e], di->element_strs[curr_e],
           di->element_param1s[curr_e], di->element_param2s[curr_e],
           di->element_storage[curr_e], 0, curr_check, 0);

          curr_e = id_over;
          id_over = -1;
        }
        // Get type of new one
        t1 = di->element_types[curr_e];
        // Code for clicking on something... (t1!=-1 means it was
        // already current)

        switch(t1)
        {
          case DE_INPUT:
            // Nothing special
            break;

          case DE_CHECK:
            // Move curr_check and toggle
            curr_check = (mouse_y - y1) - di->element_ys[curr_e];
            ((char *)di->element_storage[curr_e])[curr_check] ^= 1;
            break;

          case DE_RADIO:
            //Move radio button
            ((char *)di->element_storage[curr_e])[0]=
            (mouse_y - y1) - di->element_ys[curr_e];
            break;

          case DE_COLOR:
          case DE_CHAR:
          case DE_LIST:
          case DE_BOARD:
            key = SDLK_RETURN;
            break;

          case DE_BUTTON:
            // If it was already current, select. (enter)
            // Otherwise nothing happens.
            if(id_over != -1)
              key = SDLK_RETURN;
            break;

          case DE_NUMBER:
            // Two types of numeral apply here.
            if((di->element_param1s[curr_e] == 1) &&
             (di->element_param2s[curr_e] < 10))
            {
              // Number line
              // Select number IF on the number line itself
              t2 = mouse_x - x1;
              t2 -= di->element_xs[curr_e];
              t2 -= strlen(di->element_strs[curr_e]);
              if((t2 < di->element_param2s[curr_e]) && (t2 >= 0))
               ((int *)di->element_storage[curr_e])[0] = t2 + 1;
              break;
            }

          case DE_FIVE:
            // Numeric selector
            // Go to change_up w/-72 as key for up button
            // Go to change_down w/-80 as key for down button
            // Anything else is ignored
            // Set mouse_held to cycle.
            t2 = mouse_x - 7 - x1;
            t2 -= di->element_xs[curr_e];
            t2 -= strlen(di->element_strs[curr_e]);

            if((t2 >= 0) && (t2 <= 2))
            {
              key = SDLK_UP;
            }
            else

            if((t2 >= 3) && (t2 <= 5))
            {
              key = SDLK_DOWN;
            }
            break;
        }
      }
    }

    switch(key)
    {
      case SDLK_TAB: // Tab
      {
        if(get_shift_status(keycode_SDL))
        {
          prev_elem:
          //Unhighlight old element
          display_element(mzx_world, t1, x1 + di->element_xs[curr_e],
           y1 + di->element_ys[curr_e], di->element_strs[curr_e],
           di->element_param1s[curr_e], di->element_param2s[curr_e],
           di->element_storage[curr_e], 0, curr_check, 0);

          // Move to previous element, going past text/box/line
          do
          {
            curr_e--;
            if(curr_e < 0) curr_e = num_e - 1;
            t1 = di->element_types[curr_e];
            if((t1 == DE_BOX) || (t1 == DE_TEXT) || (t1 == DE_LINE))
              continue;
          } while(0);
        }
        else
        {
          next_elem:
          // Unhighlight old element
          display_element(mzx_world, t1, x1 + di->element_xs[curr_e],
           y1 + di->element_ys[curr_e], di->element_strs[curr_e],
           di->element_param1s[curr_e], di->element_param2s[curr_e],
           di->element_storage[curr_e], 0, curr_check, 0);
          // Move to next element
          curr_e++;
        }
        break;
      }

      case SDLK_SPACE: // Space
      case SDLK_KP_ENTER:
      case SDLK_RETURN: // Enter
      {
        // Activate button, choose from list/menu, or toggle check.
        switch(t1)
        {
          case DE_CHECK:
            ((char *)di->element_storage[curr_e])[curr_check] ^= 1;
            break;
          case DE_COLOR:
            t2 =
             color_selection(((int *)di->element_storage[curr_e])[0],
             di->element_param1s[curr_e]);
            if(t2 >= 0)
              ((int *)di->element_storage[curr_e])[0] = t2;
            break;
          case DE_CHAR:
            t2 =
             char_selection(((unsigned char *)di->element_storage[curr_e])[0]);
            if(t2 == 0) t2 = 1;
            if((t2 == 255) && (di->element_param1s[curr_e] == 0))
              t2 = 1;
            if(t2 >= 0)
              ((unsigned char *)di->element_storage[curr_e])[0] = t2;
            break;
          case DE_BUTTON:
            // Restore screen, set current, and return answer
            pop_context();
            restore_screen();
            di->curr_element = curr_e;
            return di->element_param1s[curr_e];
          case DE_BOARD:
            t2 = choose_board(mzx_world, ((int *)di->element_storage[curr_e])[0],
             di->element_strs[curr_e], di->element_param1s[curr_e]);
            if(t2 >= 0)
              ((int *)di->element_storage[curr_e])[0] = t2;
            break;
          case DE_LIST:
            t2 = di->element_param2s[curr_e];
            t2 = list_menu(di->element_strs[curr_e] + t2, t2,
             di->element_strs[curr_e],
             ((int *)di->element_storage[curr_e])[0],
             di->element_param1s[curr_e], 31);
            if(t2 >= 0)
              ((int *)di->element_storage[curr_e])[0] = t2;
            break;
          default:
            // Same as a tab
            goto next_elem;
        }
        break;
      }

      case SDLK_RIGHT:
      {
        if((t1 != DE_NUMBER) && (t1 != DE_FIVE))
          goto change_down;
      }

      case SDLK_UP:
      case SDLK_PAGEUP:
      {
        change_up:
        // Change numeric, radio, or current check. Otherwise, move
        // to previous element.

        switch(t1)
        {
          case DE_RADIO:
          {
            if(key == SDLK_PAGEUP)
              ((char *)di->element_storage[curr_e])[0] = 0;
            else
            {
              if(((char *)di->element_storage[curr_e])[0] > 0)
                ((char *)di->element_storage[curr_e])[0]--;
            }
            break;
          }

          case DE_CHECK:
          {
            if(key == SDLK_PAGEUP)
              curr_check = 0;
            else
              if(curr_check > 0)  curr_check--;
            break;
          }

          case DE_NUMBER:
          case DE_FIVE:
          {
            switch(key)
            {
              case SDLK_RIGHT: // Right
              case SDLK_UP: // Up
              {
                if(get_alt_status(keycode_SDL) ||
                 get_ctrl_status(keycode_SDL))
                  t2 = 10;
                else
                  t2 = 1;
                break;
              }

              case SDLK_PAGEUP:
              {
                t2 = 100;
                break;
              }
            }
            change_num:
            tlng = ((int *)di->element_storage[curr_e])[0];
            if((tlng + t2) < di->element_param1s[curr_e])
            {
              ((int *)di->element_storage[curr_e])[0] =
               di->element_param1s[curr_e];
              break;
            }
            if((tlng + t2) > di->element_param2s[curr_e])
            {
              ((int *)di->element_storage[curr_e])[0] =
               di->element_param2s[curr_e];
              break;
            }
            ((int *)di->element_storage[curr_e])[0] += t2;
            break;
          }

          default:
          {
            goto prev_elem;
          }
        }
        break;
      }

      case SDLK_HOME:
      {
        // Change numeric, radio, or current check. Otherwise, move
        // to first element.
        switch(t1)
        {
          case DE_NUMBER:
          case DE_FIVE:
          {
            ((int *)di->element_storage[curr_e])[0] =
             di->element_param2s[curr_e];
            break;
          }

          default:
          {
            // Unhighlight old element
            display_element(mzx_world, t1, x1 + di->element_xs[curr_e],
             y1 + di->element_ys[curr_e], di->element_strs[curr_e],
             di->element_param1s[curr_e], di->element_param2s[curr_e],
             di->element_storage[curr_e], 0, curr_check, 0);
            //Jump to first
            curr_e = 0;
            if(sfx_alt_t > 1)
              curr_e = sfx_alt_t;
            else
              if(sfx_alt_t == 1)  curr_e = 4;
            break;
          }
        }

        break;
      }

      case SDLK_LEFT:
      {
        if((t1 != DE_NUMBER) && (t1 != DE_FIVE))
          goto change_up;
      }

      case SDLK_DOWN:
      case SDLK_PAGEDOWN:
      {
        change_down:
        // Change numeric, radio, or current check. Otherwise, move
        // to next element.
        switch(t1)
        {
          case DE_RADIO:
          {
            if(key == SDLK_PAGEDOWN)
            {
              ((char *)di->element_storage[curr_e])[0] =
               di->element_param1s[curr_e] - 1;
            }
            else
            {
              if(((char *)di->element_storage[curr_e])[0] <
                 di->element_param1s[curr_e] - 1)
                ((char *)di->element_storage[curr_e])[0]++;
            }
            break;
          }

          case DE_CHECK:
          {
            if(key == SDLK_PAGEDOWN)
             curr_check = di->element_param1s[curr_e] - 1;
            else

            if(curr_check<di->element_param1s[curr_e] - 1)
              curr_check++;
            break;
          }

          case DE_NUMBER:
          case DE_FIVE:
          {
            switch(key)
            {
              case SDLK_LEFT://Left
              case SDLK_DOWN://Down
                if(get_alt_status(keycode_SDL) ||
                 get_ctrl_status(keycode_SDL))
                  t2 = -10;
                else
                  t2 = -1;
                break;
              case SDLK_PAGEDOWN://PageDown
                t2 = -100;
                break;
            }
            goto change_num;
          }

          default:
          {
            goto next_elem;
          }
        }
        break;
      }

      case SDLK_END:
      {
        // Change numeric, radio, or current check. Otherwise, move
        // to last element.
        switch(t1)
        {
          case DE_NUMBER:
          case DE_FIVE:
          {
            ((int *)di->element_storage[curr_e])[0] =
             di->element_param1s[curr_e];
            break;
          }

          default:
          {
            // Unhighlight old element
            display_element(mzx_world, t1, x1 + di->element_xs[curr_e],
             y1 + di->element_ys[curr_e], di->element_strs[curr_e],
             di->element_param1s[curr_e], di->element_param2s[curr_e],
             di->element_storage[curr_e], 0, curr_check, 0);
            // Jump to end
            if(sfx_alt_t > 0) curr_e = 0;
            else
            {
              curr_e = num_e - 1;
              // Work backwards past found text, box, or line
              do
              {
                curr_e--;
                t1 = di->element_types[curr_e];
                if((t1 == DE_BOX) || (t1 == DE_TEXT) || (t1 == DE_LINE))
                  continue;
              } while(0);
            }
            break;
          }
        }
        break;
      }

      case SDLK_ESCAPE: // ESC
      {
        // Restore screen, set current, and return -1
        restore_screen();
        di->curr_element = curr_e;
        pop_context();
        return -1;
      }

      case SDLK_F1: // F1
      {
        m_show();
        help_system(mzx_world);
        break;
      }

      case SDLK_t:
      {
        if(get_alt_status(keycode_SDL))
        {
          // Test sfx
          if((sfx_alt_t == 1) && (di->element_types[curr_e] == DE_INPUT))
          {
            // Play a sfx
            clear_sfx_queue();
            play_str((char *)di->element_storage[curr_e], 0);
          }
        }
      }

      default:
      {
        int key_char = get_key(keycode_unicode);

        // Change character to key.
        switch(t1)
        {
          case DE_CHAR:
          {
            *((char *)di->element_storage[curr_e]) =
             key_char;
            break;
          }

          case DE_NUMBER:
          case DE_FIVE:
          {
            int current_value =
             *((int *)di->element_storage[curr_e]);

            if(key == SDLK_BACKSPACE)
            {
              current_value /= 10;              
            }

            if((key_char >= '0') && (key_char <= '9'))
            {
              if(current_value == di->element_param2s[curr_e])
              {
                current_value = (key_char - '0');
              }
              else
              {
                current_value = (current_value * 10) +
                 (key_char - '0');
              }

              if(current_value < di->element_param1s[curr_e])
                current_value = di->element_param1s[curr_e]; 

              if(current_value > di->element_param2s[curr_e])
                current_value = di->element_param2s[curr_e]; 
            }


            *((int *)di->element_storage[curr_e]) =
             current_value;

            break;
          }
        }
        break;
      }

      case 0:
      {
        break;
      }
    }
  } while(1);
}

// Internal function to display an element, whether active or not.

void display_element(World *mzx_world, int type, int x, int y,
 char *str, int p1, int p2, void *value, int active, int curr_check,
 int set_vid_usage)
{
  // If set_vid_usage is non-0, set the vid_usage array appropriately.
  // set_vid_usage is equal to the element number plus 1.
  int t1, t2;
  char temp[7];
  // Color for elements (active vs. inactive)
  int color = DI_NONACTIVE;

  if(active)
    color = DI_ACTIVE;

  // Act according to type, and fill vid_usage array as well if flag set
  switch(type)
  {
    case DE_TEXT:
    {
      color_string(str, x, y, DI_TEXT);
      break;
    }

    case DE_BOX:
    {
      draw_window_box(x, y, x + p1 - 1, y + p2 - 1, DI_DARK, DI_MAIN,
       DI_CORNER, 0, 0);
      break;
    }

    case DE_LINE:
    {
      if(p2)
      {
        // Vertical
        for(t1 = 0; t1 < p1; t1++)
          draw_char('³', DI_LINE, x, y + t1);
      }
      else
      {
        // Horizontal
        fill_line(p1, x, y, 196, DI_LINE);
      }
      break;
    }

    case DE_INPUT:
    {
      write_string(str, x, y, color, 0);
      write_string((char *)value, x + strlen(str), y, DI_INPUT, 0);
      fill_line(p1 - strlen((char *)value) + 1,
       x + strlen(str) + strlen((char *)value), y, 32, DI_INPUT);
      // Fill vid_usage
      if(set_vid_usage)
      {
        for(t1 = 0; (unsigned int)t1 <= p1 + strlen(str); t1++)
        {
          vid_usage[t1 + x + y * 80] = set_vid_usage - 1;
        }
      }
      break;
    }

    case DE_CHECK:
    {
      write_string(str, x + 4, y, DI_NONACTIVE, 0);
      // Draw boxes
      for(t1 = 0; t1 < p1; t1++)
      {
        if(((char *)value)[t1]) write_string(check_on, x, y + t1,
         DI_NONACTIVE, 0);
        else write_string(check_off, x, y + t1, DI_NONACTIVE, 0);
      }
      // Highlight current
      if(active)
      {
        if(curr_check >= p1) curr_check = p1 - 1;
        color_line(p2 + 4, x, y + curr_check, DI_ACTIVE);
      }
      // Fill vid_usage
      if(set_vid_usage)
      {
        for(t1 = 0; t1 < p1; t1++)
          for(t2 = 0; t2 < p2 + 4; t2++)
            vid_usage[t2 + x + (t1 + y) * 80] = set_vid_usage - 1;
      }
      break;
    }

    case DE_RADIO:
    {
      write_string(str, x + 4, y, DI_NONACTIVE, 0);
      // Draw boxes
      for(t1 = 0; t1 < p1; t1++)
      {
        if(t1 == ((char *)value)[0])
         write_string(radio_on, x, y + t1, DI_NONACTIVE, 0);
        else
          write_string(radio_off, x, y + t1,DI_NONACTIVE, 0);
      }
      // Highlight current
      if(active)
      {
        color_line(p2 + 4, x, y + ((char *)value)[0], DI_ACTIVE);
      }
      // Fill vid_usage
      if(set_vid_usage)
      {
        for(t1 = 0; t1 < p1; t1++)
          for(t2 = 0; t2 < p2 + 4; t2++)
            vid_usage[t2 + x + (t1 + y) * 80] = set_vid_usage - 1;
      }
      break;
    }

    case DE_COLOR:
    {
      write_string(str, x, y, color, 0);
      t1 = ((int *)value)[0];
      if((t1 & 256) == 256)
      {
        draw_color_box(t1 & 255, 1, x + strlen(str), y);
      }
      else
      {
        draw_color_box(t1 & 255, 0, x + strlen(str), y);
      }

      // Fill vid_usage
      if(set_vid_usage)
      {
        t2 = strlen(str) + 3;
        for(t1 = 0; t1 < t2; t1++)
          vid_usage[t1 + x + y * 80] = set_vid_usage - 1;
      }
      break;
    }

    case DE_CHAR:
    {
      write_string(str, x, y, color, 0);
      draw_char(((unsigned char *)value)[0], DI_CHAR, x +
       strlen(str) + 1, y);
      draw_char(' ', DI_CHAR, x + strlen(str), y);
      draw_char(' ', DI_CHAR, x + strlen(str) + 2, y);
      // Fill vid_usage
      if(set_vid_usage)
      {
        t2 = strlen(str) + 3;
        for(t1 = 0; t1 < t2; t1++)
          vid_usage[t1 + x + y * 80] = set_vid_usage - 1;
      }
      break;
    }

    case DE_BUTTON:
    {
      if(active)
        color = DI_ACTIVEBUTTON;
      else
        color = DI_BUTTON;

      write_string(str, x + 1, y, color, 0);
      draw_char(' ', color, x, y);
      draw_char(' ', color, x + strlen(str) + 1, y);
      // Fill vid_usage
      if(set_vid_usage)
      {
        t2 = strlen(str) + 2;
        for(t1 = 0; t1 < t2; t1++)
          vid_usage[t1 + x + y * 80] = set_vid_usage - 1;
      }
      break;
    }

    case DE_NUMBER:
    case DE_FIVE:
    {
      t1 = 1;
      if(type == DE_FIVE) t1 = 5;
      write_string(str, x, y, color, 0);
      x += strlen(str);
      if((p1 == 1) && (p2 < 10) && (t1 == 1))
      {
        // Number line
        for(t1 = 1; t1 <= p2; t1++)
        {
          if(t1 == ((int *)value)[0])
            draw_char(t1 + '0', DI_ACTIVE, x + t1 - 1, y);
          else
            draw_char(t1 + '0', DI_NONACTIVE, x + t1 - 1, y);
        }
        // Fill vid_usage
        if(set_vid_usage)
        {
          t2 = strlen(str);
          x -= t2;
          t2 += p2;
          for(t1 = 0; t1 < t2; t1++)
            vid_usage[t1 + x + y * 80] = set_vid_usage - 1;
        }
      }
      else
      {
        // Number
        sprintf(temp, "%d", *((int *)value) * t1);
        fill_line(7, x, y, 32, DI_NUMERIC);
        write_string(temp, x + 6 - strlen(temp), y, DI_NUMERIC, 0);
        // Buttons
        write_string(num_buttons, x + 7, y, DI_ARROWBUTTON, 0);
        // Fill vid_usage
        if(set_vid_usage)
        {
          t2 = strlen(str);
          x -= t2;
          t2 += 13;
          for(t1 = 0; t1 < t2; t1++)
            vid_usage[t1 + x + y * 80] = set_vid_usage - 1;
        }
      }
      break;
    }

    case DE_LIST:
    {
      write_string(str, x , y, color, 0);
      // Draw in current choice
      t1 = p2 * (((int *)value)[0] + 1);
      fill_line(p2 + 1, x, y + 1, 32, DI_LIST);
      color_string(str + t1, x + 1, y + 1, DI_LIST);
      // Draw button
      write_string(list_button, x + p2 + 1, y + 1, DI_ARROWBUTTON, 0);
      // Fill vid_usage
      if(set_vid_usage)
      {
        for(t1 = 0; t1 <= p2 + 3; t1++)
        {
          vid_usage[t1 + x + y * 80] = set_vid_usage - 1;
          vid_usage[t1 + x + (y + 1) * 80] = set_vid_usage - 1;
        }
      }
      break;
    }

    case DE_BOARD:
    {
      int board_number = *((int *)value);
      Board *src_board;
      char board_name[BOARD_NAME_SIZE] = "(none)";

      if(board_number)
      {
        if(board_number <= mzx_world->num_boards)
        {
          src_board = mzx_world->board_list[board_number];
          strcpy(board_name, src_board->board_name);
        }
        else
        {
          strcpy(board_name, "(no board");
        }
      }

      write_string(str, x, y, color, 0);
      fill_line(BOARD_NAME_SIZE + 1, x, y + 1, 32, DI_LIST);

      color_string(board_name, x + 1, y + 1, DI_LIST);

      // Draw button
      write_string(list_button, x + BOARD_NAME_SIZE + 1, y + 1,
       DI_ARROWBUTTON, 0);
      // Fill vid_usage
      if(set_vid_usage)
      {
        for(t1 = 0; t1 <= BOARD_NAME_SIZE + 3; t1++)
        {
          vid_usage[t1 + x + y * 80] = set_vid_usage - 1;
          vid_usage[t1 + x + (y + 1) * 80] = set_vid_usage - 1;
        }
      }
      break;
    }
  }
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
  if(intake(temp_board_str, BOARD_NAME_SIZE - 1,
   38, 13, 15, 1, 0) == SDLK_ESCAPE)
  {
    restore_screen();
    return -1;
  }
  if(mzx_world->num_boards == mzx_world->num_boards_allocated)
  {
    mzx_world->num_boards_allocated *= 2;
    mzx_world->board_list = (Board **)realloc(mzx_world->board_list,
     mzx_world->num_boards_allocated * sizeof(Board *));
  }

  mzx_world->num_boards++;
  new_board = create_blank_board();
  mzx_world->board_list[current] = new_board;
  strcpy(new_board->board_name, temp_board_str);
  // Link global robot!
  new_board->robot_list[0] = &mzx_world->global_robot;
  restore_screen();

  return current;
}

// Shell for list_menu()
int choose_board(World *mzx_world, int current, char *title, int board0_none)
{
  int i;
  char *board_names =
   (char *)malloc((mzx_world->num_boards + 1) * BOARD_NAME_SIZE);
  char *cur_offset = board_names;

  // Go through - blank boards get a (no board) marker. t2 keeps track
  // of the number of boards.
  for(i = 0; i < mzx_world->num_boards; i++, cur_offset += BOARD_NAME_SIZE)
  {
    if(mzx_world->board_list[i] == NULL)
      strcpy(cur_offset, "(no board)");
    else
      strcpy(cur_offset, (mzx_world->board_list[i])->board_name);
  }

  if((current < 0) || (current >= mzx_world->num_boards))
    current = 0;

  // Add (new board) to bottom
  if(i < MAX_BOARDS)
  {
    strcpy(board_names + (i * BOARD_NAME_SIZE), "(add board)");
    i++;
  }

  // Change top to (none) if needed
  if(board0_none)
  {
    strcpy(board_names, "(none)");
  }

  // Run the list_menu()
  current = list_menu(board_names, BOARD_NAME_SIZE, title, current, i, 27);

  // New board? (if select no board or add board)
  if((current == mzx_world->num_boards) ||
   (current >= 0) && (mzx_world->board_list[current] == NULL))
  {
    return add_board(mzx_world, current);
  }

  free(board_names);

  return current;
}

char cd_types[2] = { DE_BUTTON, DE_BUTTON };
char cd_xs[2] = { 15, 37 };
char cd_ys[2] = { 2, 2 };
char *cd_strs[2] = { "OK", "Cancel" };
int cd_p1s[2] = { 0, 1 };
dialog c_di =
{ 10, 9, 69, 13, NULL, 2, cd_types, cd_xs, cd_ys, cd_strs, cd_p1s, NULL, NULL, 0 };

// Shell for run_dialog()
char confirm(World *mzx_world, char *str)
{
  c_di.title = str;
  return run_dialog(mzx_world, &c_di);
}

char yn_types[2] = { DE_BUTTON, DE_BUTTON };
char yn_xs[2] = { 15, 37 };
char yn_ys[2] = { 2, 2 };
char *yn_strs[2] = { "Yes", "No" };
int yn_p1s[2] = { 0, 1 };
dialog yn_di =
{ 10, 9, 69, 13, NULL, 2, yn_types, yn_xs, yn_ys, yn_strs, yn_p1s, NULL,
 NULL, 0 };

// Shell for run_dialog()
char ask_yes_no(World *mzx_world, char *str)
{
  yn_di.title = str;
  return run_dialog(mzx_world, &yn_di);
}

// Sort function for below use in qsort
int sort_function(const void *a, const void *b)
{
  char *dest_str = (char *)a;
  char *src_str = (char *)b;

  // Space is greater if the other is not a space
  // Space then dot dot is lower if the other is not a dot

  if((*src_str == 32) && (*dest_str != 32))
    return -1;

  if((*dest_str == 32) && (*src_str != 32))
    return 1;


  if(!strncmp(src_str, " .. ", 4) && (*dest_str == 32))
    return 1;

  if(!strncmp(dest_str, " .. ", 4) && (*src_str == 32))
    return -1;

  if((*src_str == 32) && (*dest_str != 32))
    return -1;

  if((*dest_str == 32) && (*src_str != 32))
    return 1;

  return strcasecmp(dest_str, src_str);
}

// Shell for list_menu() (copies file chosen to ret and returns -1 for ESC)
// dirs_okay of 1 means drive and directory changing is allowed.
// Use NULL for wildcards to mean "all module files"
#define NUM_MOD_TYPES 5
char *mod_types[] =
{ ".MOD", ".NST", ".WOW", ".OCT", ".S3M", ".XM", NULL };

// FIXME - make memory dynamically allocated inside this

int choose_file(char **wildcards, char *ret, char *title,
 int dirs_okay)
{
  DIR *current_dir;
  struct stat file_info;
  struct dirent *current_file;
  char current_dir_name[PATHNAME_SIZE];
  char previous_dir_name[PATHNAME_SIZE];
  char list[4096 * 60];
  int list_pos;
  int num_files;
  char *file_name;
  int file_name_length;
  int ext_pos = -1;
  int chosen;
  int i;

  getcwd(previous_dir_name, PATHNAME_SIZE);

  while(1)
  {
    list_pos = 0;
    num_files = 0;

    getcwd(current_dir_name, PATHNAME_SIZE);
    current_dir = opendir(current_dir_name);

    while(1)
    {
      memset(list + list_pos, ' ', 58);
      list[list_pos + 58] = 0;
      current_file = readdir(current_dir);
      if(current_file)
      {
        file_name = current_file->d_name;
        file_name_length = strlen(current_file->d_name);

        if(file_name_length > 32)
          file_name_length = 32;

        if(stat(current_file->d_name, &file_info) >= 0)
        {
          if(S_ISDIR(file_info.st_mode))
          {
            if(dirs_okay && strcmp(file_name, "."))
            {
              memcpy(list + list_pos + 1, file_name, file_name_length);
              strcpy(list + list_pos + 34, "[subdirectory]");
              list[list_pos + 59] = file_name_length;
  
              list_pos += 60;
              num_files++;
            }
          }
          else
          {
            // Must match one of the wildcards, also ignore the .
            if(file_name_length >= 4)
            {
              if(file_name[file_name_length - 4] == '.')
                ext_pos = file_name_length - 4;
              else
  
              if(file_name[file_name_length - 3] == '.')
                ext_pos = file_name_length - 3;
  
              for(i = 0; wildcards[i] != NULL; i++)
              {
                if(!strcasecmp((file_name + ext_pos),
                 wildcards[i]))
                {
                  memcpy(list + list_pos, file_name, file_name_length);
                  // Is it a .mzx? If so try to display title
                  if(!strcasecmp(file_name + file_name_length - 4, ".mzx"))
                  {
                    FILE *mzx_file = fopen(file_name, "rb");
                    list[list_pos + file_name_length] = ' ';
                    fread(list + list_pos + 34, 1, 24, mzx_file);
                    list[list_pos + 58] = 0;
                    fclose(mzx_file);
                  }
  
                  list[list_pos + 59] = file_name_length;
                  list_pos += 60;
                  num_files++;
                  break;
                }
              }
            }
          }
        }
      }
      else
      {
        break;
      }
    }

    qsort((void *)list, num_files, 60, sort_function);
    chosen = list_menu(list, 60, title, 0, num_files, 7);

    if(chosen < 0)
    {
      closedir(current_dir);
      chdir(previous_dir_name);
      return -1;
    }

    // Directory or drive
    if(list[60 * chosen] == ' ')
    {
      list[(60 * chosen + 1) + list[60 * chosen + 59]] = 0;
      chdir(list + (60 * chosen) + 1);
      closedir(current_dir);
    }
    else
    {
      memcpy(ret, list + (60 * chosen), list[(60 * chosen) + 59]);
      ret[list[(60 * chosen) + 59]] = 0;
      return 0;
    }
  }

  return 0;
}
