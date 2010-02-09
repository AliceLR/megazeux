/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __SPRITE_H
#define __SPRITE_H

#include "compat.h"

__M_BEGIN_DECLS

#include "sprite_struct.h"
#include "world_struct.h"

// sprite.h, by Exophase

// structures

  // flags - abcdefgh
  // a - nothing
  // b - nothing
  // c - nothing
  // d - static
  // e - use "natural" colors
  // f - draw over overlay
  // g - char based collision
  // h - initialized

enum
{
  SPRITE_INITIALIZED  = (1 << 0),
  SPRITE_CHAR_CHECK   = (1 << 1),
  SPRITE_OVER_OVERLAY = (1 << 2),
  SPRITE_SRC_COLORS   = (1 << 3),
  SPRITE_STATIC       = (1 << 4),
  SPRITE_CHAR_CHECK2  = (1 << 5),
  SPRITE_VLAYER       = (1 << 6),
};

void plot_sprite(struct world *mzx_world, struct sprite *cur_sprite, int color,
 int x, int y);
void draw_sprites(struct world *mzx_world);
int sprite_at_xy(struct sprite *cur_sprite, int x, int y);
int sprite_colliding_xy(struct world *mzx_world, struct sprite *check_sprite,
 int x, int y);

__M_END_DECLS

#endif // __SPRITE_H
