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

/* Declarations for PARAM.CPP */

#ifndef __PARAM_H
#define __PARAM_H

#include "compat.h"

__M_BEGIN_DECLS

extern int def_params[128];

int edit_param(World *mzx_world, int id, int param);
int edit_sensor(World *mzx_world, Sensor *cur_sensor);
int edit_scroll(World *mzx_world, Scroll *cur_scroll);
int edit_robot(World *mzx_world, Robot *cur_robot);

__M_END_DECLS

#endif // __PARAM_H
