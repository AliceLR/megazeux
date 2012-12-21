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

/* Declarations for EDIT.CPP */

#ifndef __EDITOR_EDIT_H
#define __EDITOR_EDIT_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../world_struct.h"

EDITOR_LIBSPEC void free_editor_config(struct world *mzx_world);
EDITOR_LIBSPEC void load_editor_config(struct world *mzx_world, int *argc,
 char *argv[]);
EDITOR_LIBSPEC void editor_init(void);
EDITOR_LIBSPEC bool is_editor(void);

/* SAVED POSITIONS
EDITOR_LIBSPEC void refactor_saved_positions(struct world *mzx_world,
 int *board_id_translation_list);
*/

int place_current_at_xy(struct world *mzx_world, enum thing id, int color,
 int param, int x, int y, struct robot *copy_robot,
 struct scroll *copy_scroll, struct sensor *copy_sensor, int overlay_edit);

#define EC_MAIN_BOX           25
#define EC_MAIN_BOX_DARK      16
#define EC_MAIN_BOX_CORNER    24
#define EC_MENU_NAME          27
#define EC_CURR_MENU_NAME     159
#define EC_MODE_STR           30
#define EC_MODE_HELP          31
#define EC_CURR_THING         31
#define EC_CURR_PARAM         23
#define EC_OPTION             26
#define EC_HIGHLIGHTED_OPTION 31
#define EC_NA_FILL            1

__M_END_DECLS

#endif // __EDITOR_EDIT_H
