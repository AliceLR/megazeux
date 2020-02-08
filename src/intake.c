/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <string.h>
#include <ctype.h>

#include "configure.h"
#include "core.h"
#include "event.h"
#include "intake.h"
#include "graphics.h"
#include "window.h"
#include "world_struct.h"

// Global status of insert
static boolean insert_on = true;

static char last_char = 0;

// (returns the key used to exit) String points to your memory for storing
// the new string. The current "value" is used- clear the string before
// calling intake if you need a blank string. Max_len is the maximum length
// of the string. X, y, and color are self-explanitory.

// The editing keys supported are as
// follows- Keys to enter characters, Enter/ESC to exit, Home/End to move to
// the front/back of the line, Insert to toggle insert/overwrite, Left/Right
// to move within the line, Bkspace/Delete to delete as usual, and Alt-Bksp
// to clear the entire line. No screen saving is performed. After this
// function, the cursor is automatically off.

// Mouse support- Clicking inside string sends cursor there. Clicking
// outside string returns a MOUSE_EVENT to caller without acknowledging
// the event.

// Returns a backspace if attempted at start of line. (exit_type==INTK_EXIT_ANY)
// Returns a delete if attempted at end of line. (exit_type==INTK_EXIT_ANY)

int intake(struct world *mzx_world, char *string, int max_len,
 int x, int y, char color, enum intake_exit_type exit_type, int *return_x_pos)
{
  int use_mask = get_config()->mask_midchars;
  int currx, curr_len;
  int done = 0, place = 0;
  char cur_char = 0;
  boolean select_char = false;
  int mouse_press;
  int action;
  int key;

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

  move_cursor(x + currx, y);

  if(insert_on)
    cursor_underline();
  else
    cursor_solid();

  do
  {
    if(use_mask)
      write_string_mask(string, x, y, color, 0);
    else
      write_string_ext(string, x, y, color, 0, 0, 16);

    fill_line(max_len + 1 - curr_len, x + curr_len, y, 32, color);

    update_screen();
    update_event_status_intake();
    key = get_key(keycode_internal_wrt_numlock);
    place = 0;

    cur_char = get_key(keycode_unicode);

    action = get_joystick_ui_action();
    if(action)
    {
      switch(action)
      {
        case JOY_X:
          // Select character
          select_char = true;
          key = IKEY_c;
          break;

        case JOY_Y:
          key = IKEY_BACKSPACE;
          break;

        default:
          key = get_joystick_ui_key();;
          break;
      }
    }

    // Exit event mimics escape
    if(get_exit_status() && exit_type != INTK_EXIT_ENTER)
    {
      key = 0;
      done = 1;
    }

    mouse_press = get_mouse_press_ext();

    if(mouse_press)
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
        if(exit_type != INTK_EXIT_ENTER)
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
        // Home
        currx = 0;
        break;
      }

      case IKEY_END:
      {
        // End
        currx = curr_len;
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
        // Insert
        if(insert_on)
          cursor_solid();
        else
          cursor_underline();

        insert_on ^= 1;
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
          if(exit_type == INTK_EXIT_ANY)
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
          if(exit_type == INTK_EXIT_ANY)
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
        if(get_alt_status(keycode_internal) || select_char)
        {
          // If alt - C is pressed, choose character
          int new_char = char_selection(last_char);
          select_char = false;

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
        // Hack for SFX editor dialog
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
      if((cur_char != 0) && (cur_char < 32) && (exit_type == INTK_EXIT_ANY))
      {
        done = 1;
        key = cur_char;
      }
      else

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

struct intake_subcontext
{
  subcontext ctx;
  char *dest;
  int current_length;
  int max_length;
  int pos;
  int *pos_external;
  int *length_external;
  boolean select_char;

  // Display info.
  int x;
  int y;
  int width;
  int color;
};

/**
 * Set the current cursor position within the editing string.
 */

static void intake_set_pos(struct intake_subcontext *intk, int new_pos)
{
  if(new_pos > intk->current_length)
    new_pos = intk->current_length;

  intk->pos = new_pos;
  if(intk->pos_external)
    *(intk->pos_external) = new_pos;
}

/**
 * Set the editing string length.
 */

static void intake_set_length(struct intake_subcontext *intk, int new_length)
{
  if(new_length > intk->max_length)
    new_length = intk->max_length;

  intk->current_length = new_length;
  if(intk->length_external)
    *(intk->length_external) = new_length;
}

/**
 * Sync the current pos/length values with their external duplicates.
 */

void intake_sync(subcontext *sub)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;

  // TODO we can't really rely on the external length yet, so use strlen even
  // if there is an external length pointer provided.
  intake_set_length(intk, strlen(intk->dest));

  if(intk->pos_external)
    intake_set_pos(intk, *(intk->pos_external));
}

/**
 * Find the start of the word preceeding the cursor and place the cursor there.
 */

static void intake_skip_back(struct intake_subcontext *intk)
{
  char *current_position;
  int pos = intk->pos;

  if(pos)
  {
    current_position = intk->dest + pos;

    if(pos)
      current_position--;

    if(!isalnum((int)*current_position))
    {
      while(pos && !isalnum((int)*current_position))
      {
        current_position--;
        pos--;
      }
    }

    do
    {
      current_position--;
      pos--;
    } while(pos && isalnum((int)*current_position));

    if(pos < 0)
      pos = 0;

    intake_set_pos(intk, pos);
  }
}

/**
 * Find the end of the word following the cursor and place the cursor there.
 */

static void intake_skip_forward(struct intake_subcontext *intk)
{
  char *current_position;
  int pos = intk->pos;

  if(pos < intk->current_length)
  {
    current_position = intk->dest + pos;

    if(!isalnum((int)*current_position))
    {
      do
      {
        current_position++;
        pos++;
      } while(*current_position && !isalnum((int)*current_position));
    }

    while(*current_position && isalnum((int)*current_position))
    {
      current_position++;
      pos++;
    }

    intake_set_pos(intk, pos);
  }
}

/**
 * Place a new char inside of the string.
 */

static boolean intake_place_char(struct intake_subcontext *intk, char chr)
{
  if(chr && (intk->current_length != intk->max_length))
  {
    // Overwrite or insert?
    if(insert_on || (intk->pos == intk->current_length))
    {
      // Insert char
      intake_set_length(intk, intk->current_length + 1);

      memmove(intk->dest + intk->pos + 1, intk->dest + intk->pos,
       intk->current_length - intk->pos);
    }
    // Add character and move forward one
    intk->dest[intk->pos] = chr;
    intake_set_pos(intk, intk->pos + 1);
    return true;
  }
  return false;
}

/**
 * Draw the input string and cursor.
 */

static boolean intake_draw(subcontext *sub)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  int use_mask = get_config()->mask_midchars;
  char temp_char = 0;
  int temp_pos = 0;
  int start_offset = 0;
  int display_length;
  int cursor_pos;

  intake_sync(sub);
  display_length = intk->current_length;
  cursor_pos = intk->pos;

  if(intk->pos >= intk->width)
  {
    if(intk->dest[intk->pos])
    {
      temp_pos = intk->pos + 1;
      temp_char = intk->dest[temp_pos];
      intk->dest[temp_pos] = 0;
    }

    start_offset = intk->pos - intk->width + 1;
    display_length = strlen(intk->dest + start_offset);
    cursor_pos = intk->width - 1;
  }
  else

  if(display_length > intk->width)
  {
    temp_pos = intk->width;
    temp_char = intk->dest[temp_pos];
    intk->dest[temp_pos] = 0;
    display_length = intk->width;
  }

  move_cursor(intk->x + cursor_pos, intk->y);

  if(insert_on)
    cursor_underline();
  else
    cursor_solid();

  if(use_mask)
    write_string_mask(intk->dest + start_offset,
     intk->x, intk->y, intk->color, false);
  else
    write_string_ext(intk->dest + start_offset,
     intk->x, intk->y, intk->color, false, 0, 16);

  fill_line(intk->width + 1 - display_length,
   intk->x + display_length, intk->y, 32, intk->color);

  if(temp_pos)
    intk->dest[temp_pos] = temp_char;

  return true;
}

/**
 * Make sure the intake values are synchronized before doing anything.
 * Also, make sure the cursor is disabled before anything tries to open a
 * window.
 */

static boolean intake_idle(subcontext *sub)
{
  intake_sync(sub);
  cursor_off();
  return false;
}

/**
 * Move the cursor if the user clicks inside of the intake area.
 */

static boolean intake_click(subcontext *sub, int *key, int button, int x, int y)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;

  if(button)
  {
    if((y == intk->y) && (x >= intk->x) &&
     (x <= intk->x + intk->width) && (button <= MOUSE_BUTTON_RIGHT))
    {
      intake_set_pos(intk, x - intk->x);
      return true;
    }
  }
  return false;
}

/**
 * Joystick input. Can't do much here; pass most input through to the parent.
 */

static boolean intake_joystick(subcontext *sub, int *key, int action)
{
  switch(action)
  {
    case JOY_LEFT:
      *key = IKEY_LEFT;
      return true;

    case JOY_RIGHT:
      *key = IKEY_RIGHT;
      return true;

    case JOY_X:
      // Select character
      ((struct intake_subcontext *)sub)->select_char = true;
      *key = IKEY_c;
      return true;

    case JOY_Y:
      *key = IKEY_BACKSPACE;
      return true;
  }
  return false;
}

/**
 * Input/delete text, move the cursor, or pass input on to the parent context.
 */

static boolean intake_key(subcontext *sub, int *key)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  boolean alt_status = get_alt_status(keycode_internal);
  boolean ctrl_status = get_ctrl_status(keycode_internal);
  boolean shift_status = get_shift_status(keycode_internal);
  boolean any_mod = (alt_status || ctrl_status || shift_status);
  boolean place = false;

  char cur_char = get_key(keycode_unicode);

  // Exit-- let the parent context handle.
  if(get_exit_status())
    return false;

  switch(*key)
  {
    case IKEY_HOME:
    {
      if(!any_mod)
      {
        // Home
        intake_set_pos(intk, 0);
        return true;
      }
      break;
    }

    case IKEY_END:
    {
      if(!any_mod)
      {
        // End
        intake_set_pos(intk, intk->current_length);
        return true;
      }
      break;
    }

    case IKEY_LEFT:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Find nearest space to the left
        intake_skip_back(intk);
        return true;
      }
      else

      if(!any_mod)
      {
        // Left
        if(intk->pos > 0)
          intake_set_pos(intk, intk->pos - 1);

        return true;
      }
      break;
    }

    case IKEY_RIGHT:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Find nearest space to the right
        intake_skip_forward(intk);
        return true;
      }
      else

      if(!any_mod)
      {
        // Right
        if(intk->pos < intk->current_length)
          intake_set_pos(intk, intk->pos + 1);

        return true;
      }
      break;
    }

    case IKEY_INSERT:
    {
      if(!any_mod)
      {
        // Insert
        if(insert_on)
          cursor_solid();
        else
          cursor_underline();

        insert_on = !insert_on;
        return true;
      }
      break;
    }

    case IKEY_BACKSPACE:
    {
      if(get_alt_status(keycode_internal))
      {
        // Alt-backspace, erase input
        intk->dest[0] = 0;
        intake_set_pos(intk, 0);
        intake_set_length(intk, 0);
        return true;
      }
      else

      if(get_ctrl_status(keycode_internal))
      {
        // Delete word
        if(intk->pos)
        {
          int old_pos = intk->pos;
          intake_skip_back(intk);
          intake_set_length(intk, intk->current_length - (old_pos - intk->pos));

          memmove(intk->dest + intk->pos, intk->dest + old_pos,
           strlen(intk->dest + old_pos) + 1);
        }
        return true;
      }
      else

      if(intk->pos > 0)
      {
        // Backspace previous char.
        // If at position 0, let the parent handle this instead.
        intake_set_pos(intk, intk->pos - 1);
        intake_set_length(intk, intk->current_length - 1);

        memmove(intk->dest + intk->pos, intk->dest + intk->pos + 1,
         intk->current_length - intk->pos + 1);
        return true;
      }
      break;
    }

    case IKEY_DELETE:
    {
      // Delete current char.
      // If at the end of the string, let the parent handle this instead.
      if(intk->current_length && intk->pos < intk->current_length)
      {
        memmove(intk->dest + intk->pos, intk->dest + intk->pos + 1,
         intk->current_length - intk->pos);

        intake_set_length(intk, intk->current_length - 1);
        return true;
      }
      break;
    }

    case IKEY_c:
    {
      if(get_ctrl_status(keycode_internal))
      {
        break;
      }
      else

      if(get_alt_status(keycode_internal) || intk->select_char)
      {
        // If alt - C is pressed, choose character
        int new_char = char_selection(last_char);
        intk->select_char = false;

        if(new_char >= 32)
        {
          last_char = new_char;
          intake_place_char(intk, new_char);
        }
        return true;
      }
      else
      {
        place = true;
      }
      break;
    }

    default:
    {
      if(!alt_status && !ctrl_status && (*key >= 32) && (*key < 256))
        place = true;

      break;
    }
  }

  // Attempt to add the char to the string.
  if(place && cur_char && cur_char >= 32)
  {
    intake_place_char(intk, cur_char);
    return true;
  }

  return false;
}

/**
 * Disable the cursor so it doesn't persist after closing intake.
 */

static void intake_destroy(subcontext *sub)
{
  cursor_off();
}

/**
 * Create a text entry subcontext on top of the current context. Will pass all
 * inputs it can't/won't handle through to its parent. An optional external
 * cursor position pointer can be provided to allow intake to return the cursor
 * position and receive outside updates to the cursor position.
 */

subcontext *intake2(context *parent, char *dest, int max_length,
 int x, int y, int width, int color, int *pos_external, int *length_external)
{
  struct intake_subcontext *intk = ccalloc(1, sizeof(struct intake_subcontext));
  struct context_spec spec;

  intk->dest = dest;
  intk->max_length = max_length;
  intk->pos_external = pos_external;
  intk->length_external = length_external;
  intk->x = x;
  intk->y = y;
  intk->width = width;
  intk->color = color;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw     = intake_draw;
  spec.idle     = intake_idle;
  spec.click    = intake_click;
  spec.key      = intake_key;
  spec.joystick = intake_joystick;
  spec.destroy  = intake_destroy;

  intake_set_length(intk, strlen(dest));
  intake_set_pos(intk, (pos_external ? *pos_external : intk->current_length));

  create_subcontext((subcontext *)intk, parent, &spec);
  return (subcontext *)intk;
}

/**
 * Change the draw color of intake.
 */

void intake_set_color(subcontext *sub, int color)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  intk->color = color;
}

/**
 * Change the draw position of intake.
 */

void intake_set_screen_pos(subcontext *sub, int x, int y)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  intk->x = x;
  intk->y = y;
}

/**
 * Insert a string of text into the intake buffer. A linebreak char can be
 * provided which will cause this loop to terminate and return the next char
 * position. Otherwise, return NULL when the input string has been exhausted.
 */

const char *intake_input_string(subcontext *sub, const char *src,
 char linebreak_char)
{
  struct intake_subcontext *intk = (struct intake_subcontext *)sub;
  const char *pos = src;
  char cur_char;

  intake_sync(sub);

  while(*pos)
  {
    cur_char = *pos;

    // Linebreak char? Skip and request a line break.
    if(cur_char == linebreak_char)
      return (pos + 1);

    // Otherwise, attempt to place until no more space is left.
    if(!intake_place_char(intk, cur_char))
      return NULL;

    pos++;
  }
  return NULL;
}
