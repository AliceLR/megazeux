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

#define thin_line            128
#define thick_line           144
#define ice_anim             160
#define lava_anim            164
#define low_ammo             167
#define hi_ammo              168
#define lit_bomb             169
#define energizer_glow       176
#define explosion_colors     184
#define horiz_door           188
#define vert_door            189
#define cw_anim              190
#define ccw_anim             194
#define open_door            198
#define trans_north          230
#define trans_south          234
#define trans_east           238
#define trans_west           242
#define trans_all            246
#define thick_arrow          250
#define thin_arrow           254
#define horiz_lazer          258
#define vert_lazer           262
#define fire_anim            266
#define fire_colors          272
#define life_anim            278
#define life_colors          282
#define ricochet_panels      286
#define mine_anim            288
#define shooting_fire_anim   290
#define shooting_fire_colors 292
#define seeker_anim          294
#define seeker_colors        298
#define whirlpool_glow       302

unsigned char id_chars[455];

unsigned char bullet_color[3] = { 15, 15, 15 };
unsigned char missile_color = 8;
unsigned char id_dmg[128];

static unsigned char get_special_id_char(struct board *src_board,
 enum thing cell_id, char param, int offset)
{
  char *level_id = src_board->level_id;
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
      return id_chars[ice_anim + param];
    }

    case LAVA:
    {
      return id_chars[lava_anim + param];
    }

    case AMMO:
    {
      if(param < 10)
        return id_chars[low_ammo];
      else
        return id_chars[hi_ammo];
    }

    case LIT_BOMB:
    {
      return id_chars[lit_bomb + (param & 0x0F)];
    }

    case DOOR:
    {
      if(param & 1)
        return id_chars[vert_door];
      else
        return id_chars[horiz_door];
    }

    case OPEN_DOOR:
    {
      return id_chars[open_door + (param & 0x1F)];
    }

    case CW_ROTATE:
    {
      return id_chars[cw_anim + param];
    }

    case CCW_ROTATE:
    {
      return id_chars[ccw_anim + param];
    }

    case TRANSPORT:
    {
      switch(param & 0x07)
      {
        case 0:
          return id_chars[trans_north + ((param >> 3) & 0x03)];
        case 1:
          return id_chars[trans_south + ((param >> 3) & 0x03)];
        case 2:
          return id_chars[trans_east  + ((param >> 3) & 0x03)];
        case 3:
          return id_chars[trans_west  + ((param >> 3) & 0x03)];
        default:
          return id_chars[trans_all   + ((param >> 3) & 0x03)];
      }
    }

    case PUSHER:
    case MISSILE:
    case SPIKE:
    {
      return id_chars[thick_arrow + param];
    }

    case LAZER:
    {
      if(param & 1)
        return id_chars[vert_lazer + ((param >> 1) & 0x03)];
      else
        return id_chars[horiz_lazer + ((param >> 1) & 0x03)];
    }

    case BULLET:
    {
      return id_chars[bullet_char + param];
    }

    case FIRE:
    {
      return id_chars[fire_anim + param];
    }

    case LIFE:
    {
      return id_chars[life_anim + param];
    }

    case RICOCHET_PANEL:
    {
      return id_chars[ricochet_panels + param];
    }

    case MINE:
    {
      return id_chars[mine_anim + (param & 0x01)];
    }

    case SHOOTING_FIRE:
    {
      return id_chars[shooting_fire_anim + (param & 0x01)];
    }

    case SEEKER:
    {
      return id_chars[seeker_anim + (param & 0x03)];
    }

    case BULLET_GUN:
    case SPINNING_GUN:
    {
      return id_chars[thin_arrow + ((param >> 3) & 0x03)];
    }

    case MISSILE_GUN:
    {
      return id_chars[thick_arrow + ((param >> 3) & 0x03)];
    }

    case SENSOR:
    {
      int idx = param;
      return (src_board->sensor_list[idx])->sensor_char;
    }

    case ROBOT:
    case ROBOT_PUSHABLE:
    {
      int idx = param;
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

static unsigned char get_special_id_color(struct board *src_board,
 enum thing cell_id, char color, char param)
{
  char normal_color = color, spec_color = 0;
  switch(cell_id)
  {
    case ENERGIZER:
      spec_color = id_chars[energizer_glow + param];
      break;

    case EXPLOSION:
      spec_color = id_chars[explosion_colors + (param & 0x0F)];
      break;

    case FIRE:
      spec_color = id_chars[fire_colors + param];
      break;

    case LIFE:
      spec_color = id_chars[life_colors + param];
      break;

    case WHIRLPOOL_1:
    case WHIRLPOOL_2:
    case WHIRLPOOL_3:
    case WHIRLPOOL_4:
      spec_color = id_chars[whirlpool_glow + (cell_id - 67)];
      break;

    case SHOOTING_FIRE:
      spec_color = id_chars[shooting_fire_colors + (param&1)];
      break;

    case SEEKER:
      spec_color = id_chars[seeker_colors + (param & 0x03)];
      break;

    case SCROLL:
      spec_color = scroll_color;
      break;

    case PLAYER: /* player */
      return id_chars[player_color];

    default:
      return color;
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
  return normal_color;
}

unsigned char get_id_char(struct board *src_board, int id_offset)
{
  unsigned char cell_id, cell_char;
  cell_id = src_board->level_id[id_offset];
  cell_char = id_chars[cell_id];

  switch(cell_char)
  {
    case 255: return src_board->level_param[id_offset];
    case 0: return
      get_special_id_char(src_board, (enum thing)cell_id,
       src_board->level_param[id_offset], id_offset);
    default: return cell_char;
  }
}

unsigned char get_id_board_color(struct board *src_board, int id_offset, int ignore_under)
{
  unsigned char color = get_special_id_color(src_board,
   (enum thing)src_board->level_id[id_offset],
   src_board->level_color[id_offset],
   src_board->level_param[id_offset]
  );

  if(!(color & 0xF0) && !(ignore_under))
  {
    color |=
      (src_board->level_under_color[id_offset] & 0xF0);
  }
  return color;
}

unsigned char get_id_color(struct board *src_board, int id_offset)
{
  return get_id_board_color(src_board, id_offset, 0);
}

unsigned char get_id_under_char(struct board *src_board, int id_offset)
{
  unsigned char cell_id, cell_char;
  cell_id = src_board->level_under_id[id_offset];
  cell_char = id_chars[cell_id];

  switch(cell_char)
  {
    case 255: return src_board->level_under_param[id_offset];
    case 0: return
      get_special_id_char(src_board, (enum thing)cell_id,
       src_board->level_under_param[id_offset], id_offset);
    default: return cell_char;
  }
}

unsigned char get_id_under_color(struct board *src_board, int id_offset)
{
  return get_special_id_color(src_board,
   (enum thing)src_board->level_under_id[id_offset],
   src_board->level_under_color[id_offset],
   src_board->level_under_param[id_offset]
  );
}

void id_put(struct board *src_board, unsigned char x_pos, unsigned char y_pos,
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

void draw_game_window(struct board *src_board, int array_x, int array_y)
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
