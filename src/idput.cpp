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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* edited for mzx s3 by JZig */

#include <stdlib.h>

#include "const.h"
#include "graphics.h"
#include "data.h"
#include "idput.h"
#include "world.h"
#include "board.h"

#define OMIT_IDCHAR_DECLARATIONS

/* Unfortunately, this needs to be C++, since C doesn't consider the below
   to be true constants */
unsigned char id_chars[455];
unsigned char const *thin_line = id_chars + 128;
unsigned char const *thick_line = thin_line + 16;
unsigned char const *ice_anim = thick_line + 16;
unsigned char const *lava_anim = ice_anim + 4;
unsigned char const *low_ammo = lava_anim + 3;
unsigned char const *hi_ammo = low_ammo + 1;
unsigned char const *lit_bomb = hi_ammo + 1;
unsigned char const *energizer_glow = lit_bomb + 7;
unsigned char const *explosion_colors = energizer_glow + 8;
unsigned char const *horiz_door = explosion_colors + 4;
unsigned char const *vert_door = horiz_door + 1;
unsigned char const *cw_anim = vert_door + 1;
unsigned char const *ccw_anim = cw_anim + 4;
unsigned char const *open_door = ccw_anim + 4;
unsigned char const *transport_anims = open_door + 32;
unsigned char const *trans_north = transport_anims;
unsigned char const *trans_south = trans_north + 4;
unsigned char const *trans_east = trans_south + 4;
unsigned char const *trans_west = trans_east + 4;
unsigned char const *trans_all = trans_west + 4;
unsigned char const *thick_arrow = transport_anims + 20;
unsigned char const *thin_arrow = thick_arrow + 4;
unsigned char const *horiz_lazer = thin_arrow + 4;
unsigned char const *vert_lazer = horiz_lazer + 4;
unsigned char const *fire_anim = vert_lazer + 4;
unsigned char const *fire_colors = fire_anim + 6;
unsigned char const *life_anim = fire_colors + 6;
unsigned char const *life_colors = life_anim + 4;
unsigned char const *ricochet_panels = life_colors + 4;
unsigned char const *mine_anim = ricochet_panels + 2;
unsigned char const *shooting_fire_anim = mine_anim + 2;
unsigned char const *shooting_fire_colors = shooting_fire_anim + 2;
unsigned char const *seeker_anim = shooting_fire_colors + 2;
unsigned char const *seeker_colors = seeker_anim + 4;
unsigned char const *whirlpool_glow = seeker_colors + 4;
unsigned char *bullet_char = (unsigned char *)whirlpool_glow + 4;
unsigned char *player_char = bullet_char + 12;
unsigned char *player_color = player_char + 4;
unsigned char missile_color = 8;

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
unsigned char id_dmg[128];

unsigned char get_special_id_char(Board *src_board, unsigned char cell_id,
 int offset)
{
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;

  switch(cell_id)
  {
    // line
    case 4:
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

      return thick_line[bits];
    }

    // web (thick/thin)
    case 18:
    case 19:
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
        return thin_line[bits];
      return thick_line[bits];
    }

    // ice
    case 25:
    {
      return ice_anim[level_param[offset]];
    }

    // lava
    case 26:
    {
      return lava_anim[level_param[offset]];
    }

    // ammo (hi/lo)
    case 35:
    {
      if(level_param[offset] < 10)
        return *low_ammo;
      else
        return *hi_ammo;
    }

    // litbomb
    case 37:
    {
      return lit_bomb[level_param[offset] & 0x0F];
    }

    // door
    case 41:
    {
      if(level_param[offset] & 1)
        return *vert_door;
      else
        return *horiz_door;
    }

    // opendoor
    case 42:
    {
      return open_door[level_param[offset] & 0x1F];
    }

    // cw
    case 45:
    {
      return cw_anim[level_param[offset]];
    }

    // ccw
    case 46:
    {
      return ccw_anim[level_param[offset]];
    }

    // transport
    case 49:
    {
      switch(level_param[offset] & 0x07)
      {
        case 0:
          return trans_north[(level_param[offset] >> 3) & 0x03];
        case 1:
          return trans_south[(level_param[offset] >> 3) & 0x03];
        case 2:
          return trans_east[(level_param[offset] >> 3) & 0x03];
        case 3:
          return trans_west[(level_param[offset] >> 3) & 0x03];
        default:
          return trans_all[(level_param[offset] >> 3) & 0x03];
      }
    }

    // pusher/missile/spike
    case 56:
    case 62:
    case 75:
    {
      return thick_arrow[level_param[offset]];
    }

    // lazer
    case 59:
    {
      if(level_param[offset] & 1)
        return vert_lazer[(level_param[offset] >> 1) & 0x03];
      else
        return horiz_lazer[(level_param[offset] >> 1)&0x03];
    }

    // bullet
    case 61:
    {
      return bullet_char[level_param[offset]];
    }

    // fire
    case 63:
    {
      return fire_anim[level_param[offset]];
    }

    // life
    case 66:
    {
      return life_anim[level_param[offset]];
    }

    // ricochet panel
    case 72:
    {
      return ricochet_panels[level_param[offset]];
    }

    // mine
    case 74:
    {
      return mine_anim[level_param[offset] & 1];
    }

    // shooting fire
    case 78:
    {
      return shooting_fire_anim[level_param[offset] & 0x01];
    }

    // seeker
    case 79:
    {
      return seeker_anim[level_param[offset] & 0x03];
    }

    // bullet gun/spinning gun
    case 92:
    case 93:
    {
      return thin_arrow[(level_param[offset]>>3) & 0x03];
    }

    // missile gun
    case 97:
    {
      return thick_arrow[(level_param[offset] >> 3) & 0x03];
    }

    // sensor
    case 122:
    {
      return (src_board->sensor_list[level_param[offset]])->sensor_char;
    }

    // robot
    case 124:
    case 123:
    {
      return (src_board->robot_list[level_param[offset]])->robot_char;
    }

    // player
    case 127:
    {
      return player_char[(src_board->player_last_dir >> 4)];
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
      get_special_id_char(src_board, cell_id, id_offset);
    default: return cell_char;
  }
}

unsigned char get_id_color(Board *src_board, int id_offset)
{
  unsigned char cell_id;
  unsigned char normal_color;
  unsigned char spec_color;

  cell_id = src_board->level_id[id_offset];
  normal_color = src_board->level_color[id_offset];
  spec_color = 0;
  switch(cell_id)
  {
    case 33: /* energizer */
      spec_color = energizer_glow[src_board->level_param[id_offset]];
      break;
    case 38: /* explosion */
      normal_color =
        explosion_colors[src_board->level_param[id_offset]&0x0F];
      goto no_spec;
    case 63: /* fire */
      spec_color = fire_colors[src_board->level_param[id_offset]];
      break;
    case 66: /* life */
      spec_color = life_colors[src_board->level_param[id_offset]];
      break;
    case 67:
    case 68:
    case 69:
    case 70: /* whirlpool */
      spec_color = whirlpool_glow[cell_id - 67];
      break;
    case 78: /* shooting fire */
      spec_color =
        shooting_fire_colors[src_board->level_param[id_offset]&1];
      break;
    case 79: /* seeker */
      spec_color =
        seeker_colors[src_board->level_param[id_offset] & 0x03];
      break;
    case 126: /* scroll */
      spec_color = scroll_color;
      break;
    case 127: /* player */
      normal_color = *player_color;
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
