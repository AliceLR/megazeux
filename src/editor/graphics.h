/* MegaZeux
 *
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

#ifndef __EDITOR_GRAPHICS_H
#define __EDITOR_GRAPHICS_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../platform.h"

void save_editor_palette(void);
void load_editor_palette(void);
void save_palette(char *fname);
void draw_char_linear(Uint8 color, Uint8 chr, Uint32 offset);
void clear_screen_no_update(void);
void ec_save_set_var(char *name, Uint8 offset, Uint32 size);
void load_editor_charsets(void);
void ec_load_smzx(void);
void ec_load_blank(void);
void ec_load_ascii(void);
void ec_load_char_ascii(Uint32 char_number);
void ec_load_char_mzx(Uint32 char_number);

__M_END_DECLS

#endif // __EDITOR_GRAPHICS_H
