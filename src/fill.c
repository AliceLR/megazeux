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

/* Fill function. */

#include "fill.h"
#include "world_struct.h"
#include "edit.h"

/* Pseudo-code algorithm-

  (MATCHES means that the id/color at that id is what we are
   filling over. PLACING includes deleting the old, deleting programs,
   and copying current program, if any. The STACK is an array for
   keeping track of areas we need to continue filling at, and the
   direction to fill in at that point. A PUSH onto the STACK moves the
   stack pointer up one and inserts info, unless the STACK is full, in
   which case the operation is cancelled. A STACK size of about 500
   elements is usally enough for a fairly complex 100 by 100 section.)

  (Note that the usual Robot#0 code is NOT performed- If current is a
   robot, it is already #0 and must stay that way for proper filling.)

  1) Initialize fill stack and record what we are filling over.
  2) Push current position going left.
  3) Loop:

    1) Take top element off of stack.
    1.5) Verify it still matches (in case another branch got here)
    2) Start at noted position. Set ABOVE_MATCH and BELOW_MATCH to 0.
    3a) If direction is left, push onto stack-

      1) Id above, going left, if matches. Set ABOVE_MATCH to 1.
      2) Id below, going left, if matches. Set BELOW_MATCH to 1.
      3) Id to the right, going right, if matches.

    3b) If direction is right, push nothing.
    4) Place at position.
    6) Note what is above- If it DOESN'T match, set ABOVE_MATCH to 0.
    7) Note what is below- If it DOESN'T match, set BELOW_MATCH to 0.
    8) If above DOES match and ABOVE_MATCH is 0-

      1) Push onto stack Id above, going left.
      2) Set ABOVE_MATCH to 1.

    9) If below DOES match and BELOW_MATCH is 0-

      1) Push onto stack Id below, going left.
      2) Set BELOW_MATCH to 1.

    10) Go one step in direction. (left/right)
    11) If position matches, jump to #4.
    12) If any elements remain on stack, loop.

  4) Done.
*/

#define match_position(off, new_x, new_y, d, check)     \
  new_offset = off;                                     \
  if((id_check[new_offset] == fill_over_id) &&          \
   (param_check[new_offset] == fill_over_param) &&      \
   (color_check[new_offset] == fill_over_color))        \
  {                                                     \
    if(stack_pos < STACK_SIZE)                          \
    {                                                   \
      stack_pos++;                                      \
      stack[stack_pos].x = new_x;                       \
      stack[stack_pos].y = new_y;                       \
      stack[stack_pos].dir = d;                         \
      check = 1;                                        \
    }                                                   \
  }                                                     \

#define match_no_check(off, new_x, new_y, d)            \
  new_offset = off;                                     \
  if((id_check[new_offset] == fill_over_id) &&          \
   (param_check[new_offset] == fill_over_param) &&      \
   (color_check[new_offset] == fill_over_color))        \
  {                                                     \
    if(stack_pos < STACK_SIZE)                          \
    {                                                   \
      stack_pos++;                                      \
      stack[stack_pos].x = new_x;                       \
      stack[stack_pos].y = new_y;                       \
      stack[stack_pos].dir = d;                         \
    }                                                   \
  }                                                     \

#define no_match(off, check)                            \
  new_offset = off;                                     \
  if((id_check[new_offset] != fill_over_id) ||          \
   (param_check[new_offset] != fill_over_param) ||      \
   (color_check[new_offset] != fill_over_color))        \
    check = 0;                                          \

#define match_position_ov(off, new_x, new_y, d, check)  \
  new_offset = off;                                     \
  if((param_check[new_offset] == fill_over_param) &&    \
   (color_check[new_offset] == fill_over_color))        \
  {                                                     \
    if(stack_pos < STACK_SIZE)                          \
    {                                                   \
      stack_pos++;                                      \
      stack[stack_pos].x = new_x;                       \
      stack[stack_pos].y = new_y;                       \
      stack[stack_pos].dir = d;                         \
      check = 1;                                        \
    }                                                   \
  }                                                     \

#define match_no_check_ov(off, new_x, new_y, d)         \
  new_offset = off;                                     \
  if((param_check[new_offset] == fill_over_param) &&    \
   (color_check[new_offset] == fill_over_color))        \
  {                                                     \
    if(stack_pos < STACK_SIZE)                          \
    {                                                   \
      stack_pos++;                                      \
      stack[stack_pos].x = new_x;                       \
      stack[stack_pos].y = new_y;                       \
      stack[stack_pos].dir = d;                         \
    }                                                   \
  }                                                     \


#define no_match_ov(off, check)                         \
  new_offset = off;                                     \
  if((param_check[new_offset] != fill_over_param) ||    \
   (color_check[new_offset] != fill_over_color))        \
    check = 0;                                          \

// Structure for one element of the stack
struct _StackElem
{
  short int x;
  short int y;
  // -1 = Left, 1 = Right.
  signed char dir;
};

typedef struct _StackElem StackElem;

// The stack
#define STACK_SIZE 4096

void fill_area(World *mzx_world, mzx_thing id, int color, int param,
 int x, int y, Robot *copy_robot, Scroll *copy_scroll, Sensor *copy_sensor,
 int overlay_edit)
{
  Board *src_board = mzx_world->current_board;
  mzx_thing fill_over_id;
  int fill_over_param, fill_over_color;
  int dir, above_match, below_match;
  int new_offset;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  char *id_check;
  char *param_check;
  char *color_check;
  int offset = x + (y * board_width);

  int stack_pos; // Current element. -1 = empty.
  StackElem stack[STACK_SIZE + 1];

  if(overlay_edit)
  {
    param_check = src_board->overlay;
    color_check = src_board->overlay_color;

    // 1) Initialize fill stack and record what we are filling over.
    fill_over_color = color_check[offset];
    fill_over_param = param_check[offset];

    if((fill_over_param == param) && (fill_over_color == color))
      return;

    stack_pos = 0;
    // 2) Push current position going left.
    stack[0].x = x;
    stack[0].y = y;
    stack[0].dir = -1;

    // 3) Loop:
    do
    {
      // 1) Take top element off of stack.
      x = stack[stack_pos].x;
      y = stack[stack_pos].y;
      dir = stack[stack_pos].dir;
      offset = x + (y * board_width);
      stack_pos--;

      // 2) Start at noted position. Set ABOVE_MATCH and BELOW_MATCH to 0.
      above_match = 0;
      below_match = 0;
      // 3a) If direction is left, push onto stack-
      if(dir == -1)
      {
        // 1) Id above, going left, if matches. Set ABOVE_MATCH to 1.
        if(y > 0)
        {
          match_position_ov(offset - board_width, x, y - 1, -1, above_match);
        }
        // 2) Id below, going left, if matches. Set BELOW_MATCH to 1.
        if(y < (board_height - 1))
        {
          match_position_ov(offset + board_width, x, y + 1, -1, below_match);
        }
        // 3) Id to the right, going right, if matches.
        if(x < (board_width - 1))
        {
          match_no_check_ov(offset + 1, x + 1, y, 1);
        }
      }
      //  3b) If direction is right, push nothing.
      //  4) Place at position.

      do
      {
        if(place_current_at_xy(mzx_world, id, color, param, x, y, copy_robot,
         copy_scroll, copy_sensor, overlay_edit) == -1)
          return;

        // 6) Note what is above- If it DOESN'T match, set ABOVE_MATCH to 0.
        if(y > 0)
        {
          no_match_ov(offset - board_width, above_match);
        }

        // 7) Note what is below- If it DOESN'T match, set BELOW_MATCH to 0.
        if(y < (board_height -1))
        {
          no_match_ov(offset + board_width, below_match);
        }

        // 8) If above DOES match and ABOVE_MATCH is 0-
        if((y > 0) && (above_match == 0))
        {
          match_position_ov(offset - board_width, x, y - 1, -1, above_match);
        }
        // 9) If below DOES match and BELOW_MATCH is 0-
        if((y < (board_height - 1)) && (below_match == 0))
        {
          match_position_ov(offset + board_width, x, y + 1, -1, below_match);
        }

        // 10) Go one step in direction. (left/right)
        x += dir;
        offset += dir;
        // 11) If position matches, jump to #4.

        if((x >= 0) && (x < board_width))
        {
          if((param_check[offset] != fill_over_param) ||
           (color_check[offset] != fill_over_color))
          {
            break;
          }
        }
        else
        {
          break;
        }
      } while(1);

      //  12) If any elements remain on stack, loop.
    } while(stack_pos >= 0);
  }
  else
  {
    if(id == PLAYER)
      return;

    id_check = src_board->level_id;
    param_check = src_board->level_param;
    color_check = src_board->level_color;

    // 1) Initialize fill stack and record what we are filling over.
    fill_over_id = (mzx_thing)id_check[offset];
    fill_over_color = color_check[offset];
    fill_over_param = param_check[offset];

    if((fill_over_id == id) && (fill_over_param == param) &&
     (fill_over_color == color))
      return;

    stack_pos = 0;
    // 2) Push current position going left.
    stack[0].x = x;
    stack[0].y = y;
    stack[0].dir = -1;

    // 3) Loop:
    do
    {
      // 1) Take top element off of stack.
      x = stack[stack_pos].x;
      y = stack[stack_pos].y;
      dir = stack[stack_pos].dir;
      offset = x + (y * board_width);
      stack_pos--;

      // 2) Start at noted position. Set ABOVE_MATCH and BELOW_MATCH to 0.
      above_match = 0;
      below_match = 0;
      // 3a) If direction is left, push onto stack-
      if(dir == -1)
      {
        // 1) Id above, going left, if matches. Set ABOVE_MATCH to 1.
        if(y > 0)
        {
          match_position(offset - board_width, x, y - 1, -1, above_match);
        }
        // 2) Id below, going left, if matches. Set BELOW_MATCH to 1.
        if(y < (board_height - 1))
        {
          match_position(offset + board_width, x, y + 1, -1, below_match);
        }
        // 3) Id to the right, going right, if matches.
        if(x < (board_width - 1))
        {
          match_no_check(offset + 1, x + 1, y, 1);
        }
      }
      //  3b) If direction is right, push nothing.
      //  4) Place at position.

      do
      {
        if(place_current_at_xy(mzx_world, id, color, param, x, y, copy_robot,
         copy_scroll, copy_sensor, overlay_edit) == -1)
        {
          return;
        }

        // 6) Note what is above- If it DOESN'T match, set ABOVE_MATCH to 0.
        if(y > 0)
        {
          no_match(offset - board_width, above_match);
        }

        // 7) Note what is below- If it DOESN'T match, set BELOW_MATCH to 0.
        if(y < (board_height -1))
        {
          no_match(offset + board_width, below_match);
        }

        // 8) If above DOES match and ABOVE_MATCH is 0-
        if((y > 0) && (above_match == 0))
        {
          match_position(offset - board_width, x, y - 1, -1, above_match);
        }
        // 9) If below DOES match and BELOW_MATCH is 0-
        if((y < (board_height - 1)) && (below_match == 0))
        {
          match_position(offset + board_width, x, y + 1, -1, below_match);
        }

        // 10) Go one step in direction. (left/right)
        x += dir;
        offset += dir;
        // 11) If position matches, jump to #4.

        if((x >= 0) && (x < board_width))
        {
          if((id_check[offset] != fill_over_id) ||
           (param_check[offset] != fill_over_param) ||
           (color_check[offset] != fill_over_color))
          {
            break;
          }
        }
        else
        {
          break;
        }
      } while(1);

      //  12) If any elements remain on stack, loop.
    } while(stack_pos >= 0);
  }
}
