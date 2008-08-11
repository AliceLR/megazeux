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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Declarations for IDARRAY.ASM */

#ifndef IDARRAY_H
#define IDARRAY_H

#include "world.h"

void id_place(World *mzx_world, int array_x, int array_y,
 mzx_thing id, char color, char param);
void offs_place_id(World *mzx_world, unsigned int offset,
 mzx_thing id, char color, char param);
void id_clear(World *mzx_world, int array_x, int array_y);
void id_remove_top(World *mzx_world, int array_x, int array_y);
//"blank" is a junk byte
void offs_remove_id(World *mzx_world, unsigned int offset);
void id_remove_under(World *mzx_world, int array_x, int array_y);

#endif
