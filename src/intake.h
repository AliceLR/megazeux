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

/* INTAKE.H- Declarations for INTAKE.CPP */

#ifndef __INTAKE_H
#define __INTAKE_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stdlib.h>

//Global insert status
extern char insert_on;

#include "world.h"

// See code for full docs, preserves mouse cursor, be prepared for a
// MOUSE_EVENT! (must acknowledge_event() it)
int intake(World *mzx_world, char *string, int max_len,
 int x, int y, char color, int exit_type, int filter_type,
 int *return_x_pos, char robo_intk, char *macro);

__M_END_DECLS

#endif // __INTAKE_H

