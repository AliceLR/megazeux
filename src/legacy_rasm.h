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

#ifndef __RASM_H
#define __RASM_H

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
  const char *name;
  int count;
  int offsets[19];
};

struct search_entry_short
{
  const char *name;
  int offset;
  int type;
};

int is_dir(char *cmd_line, char **next);
int is_condition(char *cmd_line, char **next);
int is_equality(char *cmd_line, char **next);
int is_item(char *cmd_line, char **next);
int is_command_fragment(char *cmd_line, char **next);
int is_extra(char *cmd_line, char **next);

int assemble_text(char *input_name, char *output_name);
char *assemble_file(char *name, int *size);
void disassemble_file(char *name, char *program, int program_length,
 int allow_ignores, int base);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC int assemble_line(char *cpos, char *output_buffer, char *error_buffer,
 char *param_listing, int *arg_count_ext);
CORE_LIBSPEC int disassemble_line(char *cpos, char **next, char *output_buffer,
 char *error_buffer, int *total_bytes, int print_ignores, char *arg_types,
 int *arg_count, int base);
CORE_LIBSPEC const struct search_entry_short *find_argument(char *name);
CORE_LIBSPEC void print_color(int color, char *color_buffer);
CORE_LIBSPEC int unescape_char(char *dest, char c);
CORE_LIBSPEC int get_color(char *cmd_line);
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __RASM_H
