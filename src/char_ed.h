/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

// Declaration

#ifndef __CHAR_ED_H
#define __CHAR_ED_H

#include "compat.h"

__M_BEGIN_DECLS

int char_editor(World *mzx_world);
int char_editor_ext(World *mzx_world);
int smzx_char_editor(World *mzx_world);
void fill_region(char *matrix, int x, int y, int check, int draw);
void fill_region_smzx(char *matrix, int x, int y, int check, int draw);

__M_END_DECLS

#endif // __CHAR_ED_H
