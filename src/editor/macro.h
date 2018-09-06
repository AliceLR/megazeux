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

#ifndef __EDITOR_MACRO_H
#define __EDITOR_MACRO_H

#include "../compat.h"

__M_BEGIN_DECLS

struct ext_macro;
struct macro_type;
struct editor_config_info;

void free_macro(struct ext_macro *macro_src);
void add_ext_macro(struct editor_config_info *conf, char *name,
 char *line_data, char *label);
struct ext_macro *find_macro(struct editor_config_info *conf, char *name,
 int *next);

#ifndef CONFIG_DEBYTECODE
union variable_storage *find_macro_variable(char *name, struct macro_type *m);
char *skip_whitespace(char *src);
char *skip_to_next(char *src, char t, char a, char b);
#endif

__M_END_DECLS

#endif // __EDITOR_MACRO_H
