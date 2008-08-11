/* $Id$
 * MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 1999 Charles Goetzman
 * Copyright (C) 2002 B.D.A - Koji_takeo@worldmailer.com
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

struct Sprite
{
  unsigned int x;
  unsigned int y;
  unsigned int ref_x;
  unsigned int ref_y;
  char color;
  char flags;
  char width;
  char height;
  char col_x;
  char col_y;
  char col_width;
  char col_height;
};
typedef struct Sprite Sprite;

struct Collision_list
{
  int num;
  char collisions[64];
};
typedef struct Collision_list Collision_list;

// functions

void plot_sprite(int color, int sprite, int x, int y);
void draw_sprites();
int sprite_at_xy(unsigned int sprite, int x, int y);
int sprite_colliding_xy(unsigned int sprite, int x, int y);

// global data types

extern char sprite_num;
extern char sprite_y_order;
extern char total_sprites;
extern Sprite sprites[64];
extern Collision_list collision_list;

#endif
