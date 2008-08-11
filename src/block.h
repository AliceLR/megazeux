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

/* Declarations */

#ifndef __BLOCK_H
#define __BLOCK_H

int block_cmd(World *mzx_world);
int rtoo_obj_type(World *mzx_world);
int choose_char_set(World *mzx_world);
int save_file_dialog(World *mzx_world, char *title, char *prompt, char *dest);
int save_char_dialog(World *mzx_world, char *dest, int *char_offset,
 int *char_size);
int export_type(World *mzx_world);
int import_type(World *mzx_world);
int import_mzm_obj_type(World *mzx_world);

#endif
