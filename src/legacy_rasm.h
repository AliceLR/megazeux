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

#ifndef __LEGACY_RASM_H
#define __LEGACY_RASM_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stdio.h>

#define MAX_OBJ_SIZE       65536

#define S_IMM_U16          0
#define S_IMM_S16          0
#define S_CHARACTER        2
#define S_COLOR            3
#define S_DIR              4
#define S_THING            5
#define S_PARAM            6
#define S_STRING           7
#define S_EQUALITY         8
#define S_CONDITION        9
#define S_ITEM             10
#define S_EXTRA            11
#define S_CMD              12
#define S_UNDEFINED        13

struct search_entry
{
  const char *const name;
  int count;
  const int offsets[19];
};

struct search_entry_short
{
  const char *const name;
  int offset;
  int type;
};

CORE_LIBSPEC int get_color(char *cmd_line);

extern const char special_first_char[256];

#ifdef CONFIG_DEBYTECODE

int legacy_assemble_line(char *cpos, char *output_buffer, char *error_buffer,
 char *param_listing, int *arg_count_ext);

#else // !CONFIG_DEBYTECODE

char *assemble_file(char *name, int *size);
void disassemble_file(char *name, char *program, int program_length,
 int allow_ignores, int base);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC int legacy_assemble_line(char *cpos, char *output_buffer,
 char *error_buffer, char *param_listing, int *arg_count_ext);
CORE_LIBSPEC int disassemble_line(char *cpos, char **next, char *output_buffer,
 char *error_buffer, int *total_bytes, int print_ignores, char *arg_types,
 int *arg_count, int base);
CORE_LIBSPEC const struct search_entry_short *find_argument(char *name);
CORE_LIBSPEC void print_color(int color, char *color_buffer);
CORE_LIBSPEC int unescape_char(char *dest, char c);
#endif // CONFIG_EDITOR

#endif // !CONFIG_DEBYTECODE

__M_END_DECLS

#endif // __LEGACY_RASM_H
