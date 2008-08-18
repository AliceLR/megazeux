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

/* Declaration for FILL.CPP */

#ifndef __FILL_H
#define __FILL_H

#include "compat.h"

__M_BEGIN_DECLS

void fill_area(World *mzx_world, mzx_thing id, int color, int param,
 int x, int y, Robot *copy_robot, Scroll *copy_scroll, Sensor *copy_sensor,
 int overlay_edit);

__M_END_DECLS

#endif // __FILL_H
