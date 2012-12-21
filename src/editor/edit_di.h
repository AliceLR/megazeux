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

//EDIT_DI.CPP prototypes

#ifndef __EDITOR_EDIT_DI_H
#define __EDITOR_EDIT_DI_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../world_struct.h"
#include "../window.h"

int board_goto(struct world *mzx_world, int *cursor_board_x, int *cursor_board_y);
void board_info(struct world *mzx_world);
void board_exits(struct world *mzx_world);
void global_info(struct world *mzx_world);
void global_robot(struct world *mzx_world);
void size_pos(struct world *mzx_world);
void set_confirm_buttons(struct element **elements);
void status_counter_info(struct world *mzx_world);

__M_END_DECLS

#endif // __EDITOR_EDIT_DI_H
