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

#ifndef __EDITOR_ROBO_ED_H
#define __EDITOR_ROBO_ED_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../rasm.h"
#include "../world_struct.h"

#define COMMAND_BUFFER_LEN 512

enum find_option
{
  FIND_OPTION_NONE,
  FIND_OPTION_FIND = 0,
  FIND_OPTION_REPLACE = 1,
  FIND_OPTION_REPLACE_ALL = 2
};

enum validity_types
{
  valid,
  invalid_uncertain,
  invalid_discard,
  invalid_comment
};

struct robot_line
{
  int line_text_length;
  int line_bytecode_length;
  char *line_text;
  char *line_bytecode;
  char arg_types[20];
  int num_args;
  enum validity_types validity_status;

  struct robot_line *next;
  struct robot_line *previous;
};

struct robot_state
{
  int current_line;
  struct robot_line *current_rline;
  int total_lines;
  int size;
  int max_size;
  int include_ignores;
  int disassemble_base;
  int current_x;
  int color_code;
  int mark_mode;
  int mark_start;
  int mark_end;
  int show_line_numbers;
  int scr_line_start;
  int scr_line_middle;
  int scr_line_end;
  int scr_hide_mode;
  struct robot_line *base;
  struct robot_line *mark_start_rline;
  struct robot_line *mark_end_rline;
  char *ccodes;
  enum validity_types default_invalid;
  char *active_macro;
  char *command_buffer;
  char command_buffer_space[COMMAND_BUFFER_LEN];
  int macro_recurse_level;
  int macro_repeat_level;

  struct world *mzx_world;
};

void robot_editor(struct world *mzx_world, struct robot *cur_robot);

EDITOR_LIBSPEC void init_macros(struct world *mzx_world);
EDITOR_LIBSPEC void free_extended_macros(struct world *mzx_world);

__M_END_DECLS

#endif // __EDITOR_ROBO_ED_H
