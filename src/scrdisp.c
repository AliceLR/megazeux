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

/* Scroll display code */

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "configure.h"
#include "event.h"
#include "data.h"
#include "scrdisp.h"
#include "window.h"
#include "graphics.h"
#include "error.h"
#include "intake.h"
#include "world.h"
#include "world_struct.h"
#include "robot.h"

static const char scr_nm_strs[5][12] =
 { "  Scroll   ", "   Sign    ", "Edit Scroll", "   Help    ", "" };

static void scroll_frame(struct world *mzx_world, struct scroll *scroll,
 int pos, boolean mask_chars, boolean mask_colors)
{
  // Displays one frame of a scroll. The scroll edging, arrows, and title
  // must already be shown. Simply prints each line. POS is the position
  // of the scroll's line in the text. This is of the center line.
  int t1;
  unsigned int old_pos = pos;
  char *where;
  int scroll_base_color = mzx_world->scroll_base_color;
  int c_offset = 16;

  where = scroll->mesg;

  select_layer(UI_LAYER);

  if(!mask_colors)
  {
    // Clear the UI over the frame area in case the frame is masked (editor)
    // but the contents aren't.
    erase_area(8, 6, 71, 18);
    select_layer(GAME_UI_LAYER);
    c_offset = 0;
  }

  // Display center line
  fill_line_ext(64, 8, 12, 32, scroll_base_color, 0, c_offset);

  if(mask_chars)
    write_line_mask(where + pos, 8, 12, scroll_base_color, 1);
  else
    write_line_ext(where + pos, 8, 12, scroll_base_color, 1, 0, c_offset);

  // Display lines above center line
  for(t1 = 11; t1 >= 6; t1--)
  {
    fill_line_ext(64, 8, t1, 32, scroll_base_color, 0, c_offset);
    // Go backward to previous line
    if(where[pos] != 1)
    {
      pos--;
      if(where[pos] == '\n')
      {
        do
        {
          pos--;
        } while((where[pos] != '\n') && (where[pos] != 1));
        // At start of prev. line -1. Display.
        pos++;
        if(mask_chars)
          write_line_mask(where + pos, 8, t1, scroll_base_color, 1);
        else
          write_line_ext(where + pos, 8, t1, scroll_base_color, 1, 0, c_offset);
      }
    }
    // Next line...
  }
  // Display lines below center line
  pos = old_pos;
  for(t1 = 13; t1 <= 18; t1++)
  {
    fill_line_ext(64, 8, t1, 32, scroll_base_color, 0, c_offset);
    if(where[pos] == 0) continue;
    // Go forward to next line
    while(where[pos] != '\n') pos++;
    // At end of current. If next is a 0, don't show nuthin'.
    pos++;
    if(where[pos])
    {
      if(mask_chars)
        write_line_mask(where + pos, 8, t1, scroll_base_color, 1);
      else
        write_line_ext(where + pos, 8, t1, scroll_base_color, 1, 0, c_offset);
    }
    // Next line...
  }

  select_layer(UI_LAYER);
}

// Also used to display scrolls. Use a type of 0 for Show Scroll, 1 for Show
// Sign, 2 for Edit Scroll.

void scroll_edit(struct world *mzx_world, struct scroll *scroll, int type)
{
  // Important status vars (insert kept in intake.cpp)
  unsigned int pos = 1, old_pos; // Where IN scroll?
  int currx = 0; // X position in line
  int key; // Key returned by intake()
  int t1, t2 = -1, t3;
  char *where; // Where scroll is
  char line[80]; // For editing
  int scroll_base_color = mzx_world->scroll_base_color;
  boolean editing = (type == 2);
  // If the user wants to mask chars and we're in the editor..
  boolean mask_chars = get_config()->mask_midchars && editing;

  m_show();
  save_screen();
  dialog_fadein();

  // Mask edging chars/colors in the scroll editor, but display natural
  // chars/colors during gameplay.
  scroll_edging_ext(mzx_world, type, editing);

  // Loop
  where = scroll->mesg;

  do
  {
    // Display scroll. If editing, the colors of the frame need to be masked.
    // Otherwise, they should display using game colors.
    scroll_frame(mzx_world, scroll, pos, mask_chars, editing);
    update_screen();

    if(editing)
    {
      // Figure length
      for(t1 = 0; t1 < 80; t1++)
      {
        if(where[pos + t1] == '\n')
          break;
      }
      // t1 == length
      // Edit line
      where[pos + t1] = 0;
      strcpy(line, where + pos);
      where[pos + t1] = '\n';
      key = intake(mzx_world, line, 64, 8, 12, scroll_base_color,
       INTK_EXIT_ANY, &currx);
      // Modify scroll to hold new line (give errors here)
      t2 = (int)strlen(line); // Get length of NEW line
      // Resize and move
      t3 = scroll->mesg_size;

      if(t2 - t1 > 0)
      {
        reallocate_scroll(scroll, t3 + t2 - t1);
        where = scroll->mesg;

        memmove(where + pos + t2, where + pos + t1, t3 - pos - t1);
      }
      else if(t2 - t1 < 0)
      {
        memmove(where + pos + t2, where + pos + t1, t3 - pos - t1);

        reallocate_scroll(scroll, t3 + t2 - t1);
        where = scroll->mesg;
      }

      // Copy in new line
      strcpy(where + pos, line);
      where[pos + t2] = '\n';
    }
    else
    {
      int joystick_key;

      update_event_status_delay();
      key = get_key(keycode_internal_wrt_numlock);

      joystick_key = get_joystick_ui_key();
      if(joystick_key)
        key = joystick_key;
    }

    // Exit event -- mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    old_pos = pos;

    if(get_mouse_press() || (key == -1))
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_y >= 6) && (mouse_y <= 18) && (mouse_x >= 8) &&
       (mouse_x <= 71))
      {
        t1 = mouse_y - 12;
        if(t1)
        {
          if(t1 < 0)
            goto pgdn;
          else
            goto pgup;
        }
      }
      key = IKEY_ESCAPE;
    }

    switch(key)
    {
      case IKEY_UP:
      {
        // Go back a line (if possible)
        if(where[pos - 1] == 1) break; // Can't.
        pos--;
        // Go to start of this line.
        do
        {
          pos--;
        } while((where[pos] != '\n') && (where[pos] != 1));
        pos++;
        // Done.
        break;
      }

      case IKEY_DOWN:
      {
        // Go forward a line (if possible)
        while(where[pos] != '\n')
          pos++;
        // At end of current. Is there a next line?
        pos++;
        if(where[pos] == 0)
        {
          // Nope.
          pos = old_pos;
          break;
        }
        // Yep. Done.
        break;
      }

      case IKEY_RETURN:
      {
        if(type < 2)
        {
          key = IKEY_ESCAPE;
        }
        else
        {
          // Add in new line below. Need only add one byte.
          t3 = scroll->mesg_size;
          reallocate_scroll(scroll, t3 + 1);
          where = scroll->mesg;
          // Move all at pos + currx up a space
          memmove(where + pos + currx + 1, where + pos + currx,
           t3 - pos - currx);
          // Insert a \n
          where[pos + currx] = '\n';
          // Change pos and currx
          pos = pos + currx + 1;
          currx = 0;
          scroll->num_lines++;
        }
        break;
      }

      case IKEY_BACKSPACE:
      {
        if(type < 2)
          break;

        // We are at the start of the current line and we are trying to
        // append it to the end of the previous line. First, remember
        // the size of the current line is in t2. Now we go back a line,
        // if there is one.
        if(where[pos - 1] == 1)
          break; // Nope.

        pos--;
        // Go to start of this line, COUNTING CHARACTERS.
        t1 = 0;
        do
        {
          pos--;
          t1++;
        } while((where[pos] != '\n') && (where[pos] != 1));

        // pos is at one before the start of this line. WHO CARES!? :)
        // Now we have to see if the size, t2, is over 64 characters.
        // t2 + t1 is currently one too many so check for >65 for error.

        if((t2 + t1) > 65)
        {
          pos = old_pos;
          break; // Too long. Reset position.
        }

        // OKAY! Just copy backwards over the \n in the middle to
        // append...
        t3 = scroll->mesg_size;
        memmove(where + (old_pos - 1), where + old_pos, t3 - (old_pos + 1));
        // ...and reallocate to one space less!
        reallocate_scroll(scroll, t3 - 1);
        where = scroll->mesg;

        // pos is one before this start. Fix to the start of this new
        // line. Set currx to the length of the old line this was.
        pos++;
        currx = (int)t1 - 1;
        scroll->num_lines--;

        // FIXME - total hack!
        scroll->mesg[scroll->mesg_size - 1] = 0;

        // Done.
        break;
      }

      case IKEY_PAGEDOWN:
      {
        for(t1 = 6; t1 > 0; t1--)
        {
          pgdn:
          // Go forward a line (if possible)
          old_pos = pos;
          while(where[pos] != '\n') pos++;
          // At end of current. Is there a next line?
          pos++;
          if(where[pos] == 0)
          {
            // Nope.
            pos = old_pos;
            break;
          }
          // Yep. Done.
        }
        break;
      }

      case IKEY_PAGEUP:
      {
        for(t1 = -6; t1 < 0; t1++)
        {
          pgup:
          // Go back a line (if possible)
          if(where[pos - 1] == 1) break; // Can't.
          pos--;
          // Go to start of this line.
          do
          {
            pos--;
          } while((where[pos] != '\n') && (where[pos] != 1));
          pos++;
          // Done.
        }
        break;
      }

      case IKEY_HOME:
      {
        // FIXME - This is so dirty. Please replace it.
        t1 = -30000;
        goto pgup;
      }

      case IKEY_END:
      {
        // FIXME - See above.
        t1 = 30000;
        goto pgdn;
      }

      default:
      case IKEY_ESCAPE:
      case 0:
        break;
    }
    // Continue?
  } while(key != IKEY_ESCAPE);
  // Restore screen and exit
  restore_screen();
  // Due to a faulty check, 2.83 through 2.91f always stay faded in here.
  // If something is found that relies on that, make this conditional.
  dialog_fadeout();
}

void scroll_edging_ext(struct world *mzx_world, int type, boolean mask)
{
  int scroll_base_color = mzx_world->scroll_base_color;
  int scroll_corner_color = mzx_world->scroll_corner_color;
  int scroll_pointer_color = mzx_world->scroll_pointer_color;
  int scroll_title_color = mzx_world->scroll_title_color;
  int bottom_y = (type == 3) ? 22 : 21;
  int offset = PROTECTED_CHARSET_POSITION;
  int c_offset = 16;
  int i;

  select_layer(UI_LAYER);

  if(!mask)
  {
    // Clear the place on the UI layer where this will display. This is
    // necessary because the UI may have been filled to simulate the faded
    // out state. Then, draw the shadow on the UI layer.
    erase_area(5, 3, 74, bottom_y);

    // Right shadow
    for(i = 4; i < bottom_y; i++)
      draw_char(32, 0, 75, i);

    // Bottom shadow
    color_line(70, 6, bottom_y + 1, 0);

    select_layer(GAME_UI_LAYER);
    offset = 0;
    c_offset = 0;
  }

  // Displays the edging box of scrolls. Type=0 for Scroll, 1 for Sign, 2
  // for Edit Scroll, 3 for Help, 4 for Robot (w/o title)
  // Doesn't save the screen.
  // Box for the title
  draw_window_box_ext(5, 3, 74, 5, scroll_base_color & 0xF0, scroll_base_color,
   scroll_corner_color, 1, 1, offset, c_offset);
  // Main box
  draw_window_box_ext(5, 5, 74, 19, scroll_base_color, scroll_base_color & 0xF0,
   scroll_corner_color, 1, 1, offset, c_offset); /* Text on 6 - 18 */
  // Shows keys in a box at the bottom
  if(type == 3)
  {
    draw_window_box_ext(5, 19, 74, 22, scroll_base_color & 0xF0,
     scroll_base_color, scroll_corner_color, 1, 1, offset, c_offset);
  }
  else
  {
    draw_window_box_ext(5, 19, 74, 21, scroll_base_color & 0xF0,
     scroll_base_color, scroll_corner_color, 1, 1, offset, c_offset);
  }

  // Fix chars on edging
  draw_char_ext(217, scroll_base_color, 74, 5, offset, c_offset);
  draw_char_ext(217, scroll_base_color & 0xF0, 74, 19, offset, c_offset);
  // Add arrows
  draw_char_ext(16, scroll_pointer_color, 6, 12, offset, c_offset);
  draw_char_ext(17, scroll_pointer_color, 73, 12, offset, c_offset);
  // Write title
  write_string_ext(scr_nm_strs[type], 34, 4, scroll_title_color, 0,
   offset, c_offset);
  // Write key reminders
  if(type == 2)
  {
    write_string_ext("\x12\x1d: Move cursor   Alt+C: Character "
     "  Esc: Done editing", 13, 20, scroll_corner_color, 0,
     offset, c_offset);
  }
  else

  if(type < 2)
  {
    write_string_ext("\x12: Scroll text   Esc/Enter: End reading",
     21, 20, scroll_corner_color, 0, offset, c_offset);
  }
  else

  if(type == 3)
  {
    write_string_ext("\x12:Scroll text  Esc:Exit help"
     "  Enter:Select  Alt+P:Print", 13, 20,
     scroll_corner_color, 0, offset, c_offset);
    write_string_ext("F1:Help on Help "
     " Alt+F1:Table of Contents", 20, 21,
     scroll_corner_color, 0, offset, c_offset);
  }
  else
  {
    write_string_ext("\x12:Scroll text  Esc:Exit "
     "  Enter:Select", 21, 20, scroll_corner_color, 0, offset, c_offset);
  }
  select_layer(UI_LAYER);
  update_screen();
}

#ifdef CONFIG_HELPSYS

static void help_frame(struct world *mzx_world, char *help, int pos)
{
  // Displays one frame of the help. Simply prints each line. POS is the
  // position of the center line.
  int t1, t2;
  int first = 12;
  int scroll_base_color = mzx_world->scroll_base_color;
  int scroll_arrow_color = mzx_world->scroll_arrow_color;
  unsigned int next_pos;

  // Work backwards to line
  do
  {
    if(help[pos - 1] == 1) break; // Can't.
    pos--;
    // Go to start of this line.
    do
    {
      pos--;
    } while((help[pos] != '\n') && (help[pos] != 1));
    pos++;
    //Back a line!
    first--;
  } while(first > 6);
  //First holds first line pos (6-12) to draw
  if(first > 6)
  {
    for(t1 = 6; t1 < first; t1++)
      fill_line(64, 8, t1, 32, scroll_base_color);
  }
  // Display from First to either 18 or end of help
  for(t1 = first; t1 < 19; t1++)
  {
    // Fill...
    fill_line(64, 8, t1, 32, scroll_base_color);
    // Find NEXT line NOW - Actually get end of this one.
    next_pos = pos;
    while(help[next_pos] != '\n')
      next_pos++;

    // Temp. make a 0
    help[next_pos] = 0;
    // Write- What TYPE is it?
    if(help[pos] != 255) //Normal
      color_string(help + pos, 8, t1, scroll_base_color);
    else
    {
      pos++;
      switch(help[pos])
      {
        case '$':
          // Centered. :)
          pos++;
          t2 = strlencolor(help + pos);
          color_string(help + pos, 40 - (t2 >> 1), t1, scroll_base_color);
          break;
        case '>':
        case '<':
          // Option- Jump to AFTER dest. label/fill
          pos += help[pos + 1] + 3;
          // Now show, two spaces over
          color_string(help + pos, 10, t1, scroll_base_color);
          // Add arrow
          draw_char('\x10', scroll_arrow_color, 8, t1);
          break;
        case ':':
          // Label- Jump to mesg and show
          pos += help[pos + 1] + 3;
          color_string(help + pos, 8, t1, scroll_base_color);
          break;
      }
    }
    // Now fix EOS to be a \n
    help[next_pos] = '\n';
    // Next line...
    next_pos++;
    pos = next_pos;
    if(help[pos] == 0)  break;
  }
  if(t1 < 19)
  {
    for(t1 += 1; t1 < 19; t1++)
      fill_line(64, 8, t1, 32, scroll_base_color);
  }

  update_screen();
}

void help_display(struct world *mzx_world, char *help, int offs, char *file,
 char *label)
{
  // Display a help file
  int pos = offs, old_pos; // Where
  int joystick_key;
  int key = 0;
  int t1;
  char mclick;
  // allow_help = 0;

  m_show();
  save_screen();
  dialog_fadein();

  scroll_edging_ext(mzx_world, 3, true);

  // Loop
  file[0] = label[0] = 0;
  do
  {
    // Display scroll
    help_frame(mzx_world, help, pos);
    mclick = 0;

    update_screen();

    update_event_status_delay();

    if(get_mouse_press())
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      // Move to line clicked on if mouse is in scroll, else exit
      if((mouse_y >= 6) && (mouse_y <= 18) &&
       (mouse_x >= 8) && (mouse_x <= 71))
      {
        mclick = 1;
        t1 = mouse_y - 12;
        if(t1 == 0)
          goto option;

        //t1<0 = PGUP t1 lines
        //t1>0 = PGDN t1 lines
        if(t1 < 0)
          goto pgup;

        goto pgdn;
      }
      key = IKEY_ESCAPE;
    }

    key = get_key(keycode_internal_wrt_numlock);

    joystick_key = get_joystick_ui_key();
    if(joystick_key)
      key = joystick_key;

    // Exit event -- mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    old_pos = pos;
    switch(key)
    {
      case IKEY_F1:
      {
        if(get_alt_status(keycode_internal))
        {
          // Jump to label 072 in MAIN.HLP
          strcpy(file,"MAIN.HLP");
          strcpy(label,"072");
        }
        else
        {
          // Jump to label 000 in HELPONHE.HLP
          strcpy(file, "HELPONHE.HLP");
          strcpy(label, "000");
        }
        goto ex;
      }

      case IKEY_UP:
      {
        // Go back a line (if possible)
        if(help[pos - 1] == 1) break; // Can't.
        pos--;
        // Go to start of this line.
        do
        {
          pos--;
        } while((help[pos] != '\n') && (help[pos] != 1));
        pos++;
        // Done.
        break;
      }

      case IKEY_DOWN:
      {
        // Go forward a line (if possible)
        while(help[pos] != '\n') pos++;
        // At end of current. Is there a next line?
        pos++;
        if(help[pos] == 0)
        {
          // Nope.
          pos = old_pos;
          break;
        }
        // Yep. Done.
        break;
      }

      case IKEY_RETURN:
      {
        option:
        // Option?
        if((help[pos] == 255) && ((help[pos + 1] == '>') ||
         (help[pos + 1] == '<')))
        {
          // Yep!
          pos++;
          if(help[pos] == '<')
          {
            // Get file and label and exit
            t1 = 0;
            pos++;
            do
            {
              pos++;
              file[t1] = help[pos];
              t1++;
            } while(help[pos] != ':');
            file[t1 - 1] = 0;
            strcpy(label, help + pos + 1);
            goto ex;
          }
          // Get label and jump
          strcpy(label, help + pos + 2);

          // Search backwards for a 1
          do
          {
            pos--;
          } while(help[pos] != 1);
          // Search for label OR a \n followed by a \0
          do
          {
            pos++;
            if(help[pos] == 255)
              if(help[pos + 1] == ':')
                if(!strcmp(help + pos + 3, label))
                  // pos is correct!
                  goto labdone;
            if(help[pos] == '\n')
              if(help[pos + 1] == 0) break;
          } while(1);
        }
        // If there WAS an option, any existing label was found.
        labdone:
        break;
      }

      case IKEY_PAGEDOWN:
      {
        for(t1 = 6; t1 > 0; t1--)
        {
          pgdn:
          // Go forward a line (if possible)
          old_pos = pos;
          while(help[pos] != '\n') pos++;
          // At end of current. Is there a next line?
          pos++;
          if(help[pos] == 0)
          {
            // Nope.
            pos = old_pos;
            break;
          }
          // Yep. Done.
        }
        if(mclick)
          goto option;

        break;
      }

      case IKEY_PAGEUP:
      {
        for(t1 = -6; t1 < 0; t1++)
        {
          pgup:
          // Go back a line (if possible)
          if(help[pos - 1] == 1) break; // Can't.
          pos--;
          // Go to start of this line.
          do
          {
            pos--;
          } while((help[pos] != '\n') && (help[pos] != 1));
          pos++;
          // Done.
        }
        if(mclick)
          goto option;
        break;
      }

      case IKEY_HOME:
      {
        // FIXME - :(
        t1 = -30000;
        goto pgup;
      }

      case IKEY_END:
      {
        t1 = 30000;
        goto pgdn;
      }

      default:
      {
        break;
      }
    }
  } while(key != IKEY_ESCAPE);

  // Restore screen and exit
ex:
  dialog_fadeout();

  restore_screen();
}

#endif // CONFIG_HELPSYS

int strlencolor(char *str)
{
  int len = 0;
  char cur_char = *str;\

  while(cur_char != 0)
  {
    switch(cur_char)
    {
      // Color character
      case '@':
      case '~':
      {
        str++;
        cur_char = *str;

        // If 0, stop right there
        if(!cur_char)
          return len;

        // If the next isn't hex, count as one
        if(!isxdigit((int)cur_char))
        {
          len++;
        }
        break;
      }

      default:
      {
        len++;
      }
    }

    str++;
    cur_char = *str;
  }

  return len;
}
