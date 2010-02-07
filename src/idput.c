/* MegaZeux
 *
 * Copyright (C) 1998 MenTaLguY - mentalg@geocities.com
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

#include <stdlib.h>

#include "const.h"
#include "graphics.h"
#include "data.h"
#include "idput.h"
#include "world.h"
#include "board.h"

unsigned char id_chars[455];

unsigned char def_id_chars[455] =
{
  /* id_chars */
  32,178,219,6,0,255,177,255,233,254,255,254,255,178,177,176, /* 0-15 */
  254,255,0,0,176,24,25,26,27,0,0,160,4,4,3,9,                /* 16-31 */
  150,7,176,0,11,0,177,12,10,0,0,162,161,0,0,22,              /* 32-47 */
  196,0,7,255,255,255,255,159,0,18,29,0,206,0,0,0,            /* 48-63 */
  '?',178,0,151,152,153,154,32,0,42,0,0,255,255,0,0,          /* 64-79 */
  235,236,5,42,2,234,21,224,127,149,5,227,0,0,172,173,        /* 80-95 */
  '?',0, /* 96-97 */
  '?','?','?','?','?','?','?','?', /* 98-121 */
  '?','?','?','?','?','?','?','?',
  '?','?','?','?','?','?','?','?',
  0,0,0,226,232,0, /* 122-127 */
  /* thin_line */
  249,179,179,179, /* None, N, S, NS */
  196,192,218,195, /* E, NE, SE, NSE */
  196,217,191,180, /* W, NW, SW, NSW */
  196,193,194,197, /* EW, NEW, SEW, NSEW */
  /* thick_line */
  249,186,186,186, /* None, N, S, NS */
  205,200,201,204, /* E, NE, SE, NSE */
  205,188,187,185, /* W, NW, SW, NSW */
  205,202,203,206, /* EW, NEW, SEW, NSEW */
  /* ice_anim */
  32,252,253,255, /* Ice animation table */
  /* lava_anim */
  176,177,178, /* Lava animation table */
  /* low_ammo, hi_ammo */
  163, /* < 10 ammunition pic */
  164, /* > 9 ammunition pic */
  /* lit_bomb */
  171,170,169,168,167,166,165, /* Lit bomb animation */
  /* energizer_glow */
  1,9,3,11,15,11,3,9, /* Energizer Glow */
  /* explosion_colors */
  239,206,76,4, /* Explosion color table */
  /* horiz_door, vert_door */
  196, /* Horizontal door pic */
  179, /* Vertical door pic */
  /* cw_anim, ccw_anim */
  47,196,92,179, /* CW animation table */
  47,179,92,196, /* CCW animation table */
  /* open_door */
  47,47, /* Open 1/2 of type 0 */
  92,92, /* Open 1/2 of type 1 */
  92,92, /* Open 1/2 of type 2 */
  47,47, /* Open 1/2 of type 3 */
  179,196,179,196,179,196,179,196, /* Open full of all types */
  179,196,179,196,179,196,179,196, /* Open full of all types */
  47,47, /* Close 1/2 of type 0 */
  92,92, /* Close 1/2 of type 1 */
  92,92, /* Close 1/2 of type 2 */
  47,47, /* Close 1/2 of type 3 */
  /* transport_anims */
  45,94,126,94, /* North */
  86,118,95,118, /* South */
  93,41,62,41, /* East */
  91,40,60,40, /* West */
  94,62,118,60, /* All directions */
  /* thick_arrow, thin_arrow */
  30,31,16,17, /* Thick arrows (pusher-style) NSEW */
  24,25,26,27, /* Thin arrows (gun-style) NSEW */
  /* horiz_lazer, vert_lazer */
  130,196,131,196, /* Horizontal Lazer Anim Table */
  128,179,129,179, /* Vertical Lazer Anim Table */
  /* fire_anim, fire_colors */
  177,178,178,178,177,176, /* Fire animation */
  4,6,12,14,12,6, /* Fire colors */
  /* life_anim, life_colors */
  155,156,157,158, /* Free life animation */
  15,11,3,11, /* Free life colors */
  /* ricochet_panels */
  28,23, /* Ricochet pics */
  /* mine_anim */
  143,144, /* Mine animation */
  /* shooting_fire_anim, shooting_fire_colors */
  15,42, /* Shooting Fire Anim */
  12,14, /* Shooting Fire Colors */
  /* seeker_anim, seeker_colors */
  '/','-','\\','|', /* Seeker animation */
  11,14,10,12, /* Seeker colors */
  /* whirlpool_glow */
  11,3,9,15, /* Whirlpool colors (this ends at 306 bytes, where ver */
  /* 1.02 used to end) */
  /* bullet_char */
  145,146,147,148, /* Player */
  145,146,147,148, /* Neutral */
  145,146,147,148, /* Enemy */
  /* player_char */
  2, 2, 2, 2, /*N S E W */
  /* player_color, missile_color, bullet_color */
  27,
  8,
  15, /* Player */
  15, /* Neutral */
  15, /* Enemy */
  /* id_dmg */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0-15 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100,  0, 0, 0, 0, 0, /* 16-31 */
  0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 32-47 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 10, 5, 5, /* 48-63 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 10, 10, /* 64-79 */
  10, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 10, 10, /* 80-95 */
  0, 0, /* 96-97 */
  0, 0, 0, 0, 0, 0, 0, 0, /* 98-121 */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0  /* 122-127 */
};

unsigned char bullet_color[3] = { 15, 15, 15 };
unsigned char missile_color = 8;
unsigned char id_dmg[128];

unsigned char get_special_id_char(Board *src_board, mzx_thing cell_id,
 int offset)
{
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;

  switch(cell_id)
  {
    case LINE:
    {
      int array_x = offset % board_width;
      int array_y = offset / board_width;

      int bits = 0;
      char *ptr = level_id + offset;

      if(array_y > 0)
      {
        if(*(ptr - board_width) == 4)
          bits = 1;
      }
      else
      {
        bits = 1;
      }

      if(array_y < (board_height - 1))
      {
        if(*(ptr + board_width) == 4)
          bits |= 2;
      }
      else
      {
        bits |= 2;
      }

      if(array_x > 0)
      {
        if(*(ptr - 1) == 4)
          bits |= 8;
      }
      else
      {
        bits |= 8;
      }

      if(array_x < (board_width - 1))
      {
        if(*(ptr + 1) == 4 )
          bits |= 4;
      }
      else
      {
        bits |= 4;
      }

      return id_chars[thick_line + bits];
    }

    case WEB:
    case THICK_WEB:
    {
      int array_x = offset % board_width;
      int array_y = offset / board_width;
      int bits = 0;

      char *ptr;
      ptr = level_id + offset;

      if(array_y > 0 )
      {
        if(*(ptr - board_width) != 0)
          bits = 1;
      }
      else
      {
        bits = 1;
      }

      if(array_y < (board_height - 1))
      {
        if(*(ptr + board_width) != 0)
          bits |= 2;
      }
      else
      {
        bits |= 2;
      }

      if(array_x > 0)
      {
        if(*(ptr - 1) != 0)
          bits |= 8;
      }
      else
      {
        bits |= 8;
      }

      if(array_x < (board_width - 1))
      {
        if(*(ptr + 1) != 0)
          bits |= 4;
      }
      else
      {
        bits |= 4;
      }

      if(cell_id == 18)
        return id_chars[thin_line + bits];
      return id_chars[thick_line + bits];
    }

    case ICE:
    {
      return id_chars[ice_anim + level_param[offset]];
    }

    case LAVA:
    {
      return id_chars[lava_anim + level_param[offset]];
    }

    case AMMO:
    {
      if(level_param[offset] < 10)
        return id_chars[low_ammo];
      else
        return id_chars[hi_ammo];
    }

    case LIT_BOMB:
    {
      return id_chars[lit_bomb + (level_param[offset] & 0x0F)];
    }

    case DOOR:
    {
      if(level_param[offset] & 1)
        return id_chars[vert_door];
      else
        return id_chars[horiz_door];
    }

    case OPEN_DOOR:
    {
      return id_chars[open_door + (level_param[offset] & 0x1F)];
    }

    case CW_ROTATE:
    {
      return id_chars[cw_anim + level_param[offset]];
    }

    case CCW_ROTATE:
    {
      return id_chars[ccw_anim + level_param[offset]];
    }

    case TRANSPORT:
    {
      switch(level_param[offset] & 0x07)
      {
        case 0:
          return id_chars[trans_north + ((level_param[offset] >> 3) & 0x03)];
        case 1:
          return id_chars[trans_south + ((level_param[offset] >> 3) & 0x03)];
        case 2:
          return id_chars[trans_east  + ((level_param[offset] >> 3) & 0x03)];
        case 3:
          return id_chars[trans_west  + ((level_param[offset] >> 3) & 0x03)];
        default:
          return id_chars[trans_all   + ((level_param[offset] >> 3) & 0x03)];
      }
    }

    case PUSHER:
    case MISSILE:
    case SPIKE:
    {
      return id_chars[thick_arrow + level_param[offset]];
    }

    case LAZER:
    {
      if(level_param[offset] & 1)
        return id_chars[vert_lazer + ((level_param[offset] >> 1) & 0x03)];
      else
        return id_chars[horiz_lazer + ((level_param[offset] >> 1) & 0x03)];
    }

    case BULLET:
    {
      return id_chars[bullet_char + level_param[offset]];
    }

    case FIRE:
    {
      return id_chars[fire_anim + level_param[offset]];
    }

    case LIFE:
    {
      return id_chars[life_anim + level_param[offset]];
    }

    case RICOCHET_PANEL:
    {
      return id_chars[ricochet_panels + level_param[offset]];
    }

    case MINE:
    {
      return id_chars[mine_anim + (level_param[offset] & 0x01)];
    }

    case SHOOTING_FIRE:
    {
      return id_chars[shooting_fire_anim + (level_param[offset] & 0x01)];
    }

    case SEEKER:
    {
      return id_chars[seeker_anim + (level_param[offset] & 0x03)];
    }

    case BULLET_GUN:
    case SPINNING_GUN:
    {
      return id_chars[thin_arrow + ((level_param[offset] >> 3) & 0x03)];
    }

    case MISSILE_GUN:
    {
      return id_chars[thick_arrow + ((level_param[offset] >> 3) & 0x03)];
    }

    case SENSOR:
    {
      int idx = level_param[offset];
      return (src_board->sensor_list[idx])->sensor_char;
    }

    case ROBOT:
    case ROBOT_PUSHABLE:
    {
      int idx = level_param[offset];
      return (src_board->robot_list[idx])->robot_char;
    }

    case PLAYER:
    {
      return id_chars[player_char + (src_board->player_last_dir >> 4)];
    }

    // everything else
    default:
    {
      return id_chars[cell_id];
    }
  }
}

unsigned char get_id_char(Board *src_board, int id_offset)
{
  unsigned char cell_id, cell_char;
  cell_id = src_board->level_id[id_offset];
  cell_char = id_chars[cell_id];

  switch(cell_char)
  {
    case 255: return src_board->level_param[id_offset];
    case 0: return
      get_special_id_char(src_board, (mzx_thing)cell_id, id_offset);
    default: return cell_char;
  }
}

unsigned char get_id_color(Board *src_board, int id_offset)
{
  mzx_thing cell_id;
  unsigned char normal_color;
  unsigned char spec_color;

  cell_id = (mzx_thing)src_board->level_id[id_offset];
  normal_color = src_board->level_color[id_offset];
  spec_color = 0;
  switch(cell_id)
  {
    case ENERGIZER:
      spec_color = id_chars[energizer_glow + src_board->level_param[id_offset]];
      break;

    case EXPLOSION:
      normal_color =
        id_chars[explosion_colors + (src_board->level_param[id_offset] & 0x0F)];
      goto no_spec;

    case FIRE:
      spec_color = id_chars[fire_colors + src_board->level_param[id_offset]];
      break;

    case LIFE:
      spec_color = id_chars[life_colors + src_board->level_param[id_offset]];
      break;

    case WHIRLPOOL_1:
    case WHIRLPOOL_2:
    case WHIRLPOOL_3:
    case WHIRLPOOL_4:
      spec_color = id_chars[whirlpool_glow + (cell_id - 67)];
      break;

    case SHOOTING_FIRE:
      spec_color =
        id_chars[shooting_fire_colors + (src_board->level_param[id_offset]&1)];
      break;

    case SEEKER:
      spec_color =
        id_chars[seeker_colors + (src_board->level_param[id_offset] & 0x03)];
      break;

    case SCROLL:
      spec_color = scroll_color;
      break;

    case PLAYER: /* player */
      normal_color = id_chars[player_color];

    default:
      goto no_spec;
  }

  if(!(spec_color & 0xF0))
  {
    normal_color &= 0xF0;
    normal_color |= (spec_color & 0x0F);
  }
  else
  {
    normal_color = spec_color;
  }

  no_spec:

  if(!(normal_color & 0xF0))
  {
    normal_color |=
      (src_board->level_under_color[id_offset] & 0xF0);
  }
  return normal_color;
}

void id_put(Board *src_board, unsigned char x_pos, unsigned char y_pos,
 int array_x, int array_y, int ovr_x, int ovr_y)
{
  int array_offset, overlay_offset;
  int overlay_mode = src_board->overlay_mode;
  int board_width = src_board->board_width;

  unsigned char c, color;

  array_offset = (array_y * board_width) + array_x;

  if(!(overlay_mode & 128) && (overlay_mode & 3) &&
   ((overlay_mode & 3) != 3))
  {
    if(overlay_mode & 2)
    {
      overlay_offset = (ovr_y * board_width) + ovr_x;
    }
    else
    {
      overlay_offset = array_offset;
    }

    c = src_board->overlay[overlay_offset];
    color = src_board->overlay_color[overlay_offset];
    if(overlay_mode & 64)
    {
      draw_char_ext(c, color, x_pos, y_pos, 0, 0);
      return;
    }

    if(overlay_mode == 4)
    {
      if(c == 32)
      {
        c = get_id_char(src_board, array_offset);
        color = get_id_color(src_board, array_offset);
      }
      else
      {
        if(!(color & 0xF0) && !(overlay_mode & 64))
        {
          c = get_id_char(src_board, array_offset);
          color = (color & 0x0F) |
           (get_id_color(src_board, array_offset) & 0xF0);
        }
      }
    }
    else
    {
      if(c == 32)
      {
        c = get_id_char(src_board, array_offset);
        color = get_id_color(src_board, array_offset);
      }
      else
      {
        if(!(color & 0xF0) && !(overlay_mode & 64))
        {
          color = (color & 0x0F) |
           (get_id_color(src_board, array_offset) & 0xF0);
        }
      }
    }
  }
  else
  {
    c = get_id_char(src_board, array_offset);
    color = get_id_color(src_board, array_offset);
  }

  draw_char_ext(c, color, x_pos, y_pos, 0, 0);
}

void draw_edit_window(Board *src_board, int array_x, int array_y,
 int window_height)
{
  int viewport_width = 80, viewport_height = window_height;
  int x, y;
  int a_x, a_y;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;

  if(board_width < viewport_width)
    viewport_width = board_width;
  if(board_height < viewport_height)
    viewport_height = board_height;

  for(y = 0, a_y = array_y; y < viewport_height; y++, a_y++)
  {
    for(x = 0, a_x = array_x; x < viewport_width; x++, a_x++)
    {
      id_put(src_board, x, y, a_x, a_y, a_x, a_y);
    }
  }
}

void draw_game_window(Board *src_board, int array_x, int array_y)
{
  int x_limit, y_limit;
  int x, y;
  int a_x, a_y;
  int o_x, o_y;

  x_limit = src_board->viewport_width;
  y_limit = src_board->viewport_height;

  for(y = src_board->viewport_y, a_y = array_y, o_y = 0; o_y < y_limit;
   y++, a_y++, o_y++)
  {
    for(x = src_board->viewport_x, a_x = array_x, o_x = 0;
     o_x < x_limit; x++, a_x++, o_x++)
    {
      id_put(src_board, x, y, a_x, a_y, o_x, o_y);
    }
  }

}
