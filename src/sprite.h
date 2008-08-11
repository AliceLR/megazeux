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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef SPRITE_H
#define SPRITE_H

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

#define SPRITE_INITIALIZED  0x01
#define SPRITE_CHAR_CHECK   0x02
#define SPRITE_OVER_OVERLAY 0x04
#define SPRITE_SRC_COLORS   0x08
#define SPRITE_STATIC       0x10
#define SPRITE_CHAR_CHECK2  0x20
#define SPRITE_VLAYER       0x40

#define MAX_SPRITES         256

struct Sprite
{
  int x;
  int y;
  int ref_x;
  int ref_y;
  char color;
  char flags;
  char width;
  char height;
  signed char col_x;
  signed char col_y;
  char col_width;
  char col_height;
};

typedef struct Sprite Sprite;

struct Collision_list
{
  int num;
  int collisions[MAX_SPRITES];
};

typedef struct Collision_list Collision_list;

// functions

void plot_sprite(World *mzx_world, Sprite *cur_sprite, int color,
 int x, int y);
void draw_sprites(World *mzx_world);
int sprite_at_xy(Sprite *cur_sprite, int x, int y);
int sprite_colliding_xy(World *mzx_world, Sprite *check_sprite,
 int x, int y);
int is_blank(char c);

// global data types

#endif
