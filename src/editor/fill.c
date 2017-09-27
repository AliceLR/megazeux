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

/* Fill function. */

#include "fill.h"

#include "../util.h"
#include "edit.h"

struct queue_elem
{
  short int x;
  short int y;
};

#define QUEUE_SIZE 8192

#define MATCHING(off) (               \
 (level_id[off] == fill_id) &&        \
 (level_color[off] == fill_color) &&  \
 (level_param[off] == fill_param)     \
)

#define QUEUE_NEXT() {                              \
  x = queue[queue_first].x;                         \
  y = queue[queue_first].y;                         \
  queue_first = (queue_first + 1) % QUEUE_SIZE;     \
  offset = x + (y * board_width);                   \
}

#define QUEUE_ADD(addx,addy) {                      \
  queue[queue_next].x = addx;                       \
  queue[queue_next].y = addy;                       \
  queue_next = (queue_next + 1) % QUEUE_SIZE;       \
}

void fill_area(struct world *mzx_world, enum thing id, int color, int param,
 int x, int y, struct robot *copy_robot, struct scroll *copy_scroll,
 struct sensor *copy_sensor, int overlay_edit)
{
  struct board *cur_board = mzx_world->current_board;

  struct queue_elem *queue = cmalloc(QUEUE_SIZE * sizeof(struct queue_elem));
  int queue_first = 0;
  int queue_next = 1;
  int matched_down;
  int matched_up;

  char *level_id;
  char *level_color;
  char *level_param;
  int board_width;
  int board_height;
  int offset;

  enum thing fill_id = 0;
  int fill_color = 0;
  int fill_param = 0;

  // Do nothing if the player is in the buffer
  if(id == PLAYER)
    return;

  switch(overlay_edit)
  {
    default:
    case EDIT_BOARD:
      level_id = cur_board->level_id;
      level_color = cur_board->level_color;
      level_param = cur_board->level_param;
      board_width = cur_board->board_width;
      board_height = cur_board->board_height;
      break;

    case EDIT_OVERLAY:
      // Use chars for id so we don't have to check for NULL
      level_id = cur_board->overlay;
      level_color = cur_board->overlay_color;
      level_param = cur_board->overlay;
      board_width = cur_board->board_width;
      board_height = cur_board->board_height;
      break;

    case EDIT_VLAYER:
      // Use chars for id so we don't have to check for NULL
      level_id = mzx_world->vlayer_chars;
      level_color = mzx_world->vlayer_colors;
      level_param = mzx_world->vlayer_chars;
      board_width = mzx_world->vlayer_width;
      board_height = mzx_world->vlayer_height;
      break;
  }

  offset = x + (y * board_width);
  fill_id = level_id[offset];
  fill_color = level_color[offset];
  fill_param = level_param[offset];

  // Fill same as buffer? Do nothing
  if((fill_id == id) &&
   (fill_color == color) &&
   (fill_param == param))
    goto err_free;

  queue[0].x = x;
  queue[0].y = y;

  // Perform the fill
  do
  {
    // Get next queue element
    QUEUE_NEXT();

    if(!MATCHING(offset))
      continue;

    // Seek left
    do
    {
      offset--;
      x--;
    }
    while((x >= 0) && MATCHING(offset));

    matched_down = 0;
    matched_up = 0;
    offset++;
    x++;

    // Scan right
    do
    {
      if(place_current_at_xy(mzx_world, id, color, param, x, y, copy_robot,
       copy_scroll, copy_sensor, overlay_edit, 0) == -1)
        goto err_free;

      if(y > 0 && MATCHING(offset - board_width))
      {
        if(!matched_up)
        {
          QUEUE_ADD(x, y-1);
          matched_up = 1;
        }
      }
      else
      {
        matched_up = 0;
      }

      if(y+1 < board_height && MATCHING(offset + board_width))
      {
        if(!matched_down)
        {
          QUEUE_ADD(x, y+1);
          matched_down = 1;
        }
      }
      else
      {
        matched_down = 0;
      }

      offset++;
      x++;
    }
    while((x < board_width) && MATCHING(offset));
  }
  while(queue_first != queue_next);

err_free:
  free(queue);
}
