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

//EDIT_DI.CPP prototypes

#ifndef __EDIT_DI_H
#define __EDIT_DI_H

#define MAX_BOARD_SIZE 16 * 1024 * 1024

#include "window.h"

void set_confirm_buttons(element **elements);
void status_counter_info(World *mzx_world);
void board_exits(World *mzx_world);
void size_pos(World *mzx_world);
void board_info(World *mzx_world);
void global_info(World *mzx_world);
void global_chars(World *mzx_world);
void global_dmg(World *mzx_world);

#endif

