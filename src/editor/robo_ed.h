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

#define MAX_MACRO_RECURSION 16
#define MAX_MACRO_REPEAT 128
#define COMMAND_BUFFER_LEN 512

typedef struct _robot_line robot_line;
typedef struct _robot_state robot_state;

typedef enum
{
  valid,
  invalid_uncertain,
  invalid_discard,
  invalid_comment
} validity_types;

struct _robot_line
{
  int line_text_length;
  int line_bytecode_length;
  char *line_text;
  char *line_bytecode;
  char arg_types[20];
  int num_args;
  validity_types validity_status;

  struct _robot_line *next;
  struct _robot_line *previous;
};

struct _robot_state
{
  int current_line;
  robot_line *current_rline;
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
  robot_line *base;
  robot_line *mark_start_rline;
  robot_line *mark_end_rline;
  char *ccodes;
  validity_types default_invalid;
  char *active_macro;
  char *command_buffer;
  char command_buffer_space[COMMAND_BUFFER_LEN];
  int macro_recurse_level;
  int macro_repeat_level;

  World *mzx_world;
};

void robot_editor(World *mzx_world, Robot *cur_robot);

void init_macros(World *mzx_world);
void free_extended_macros(World *mzx_world);

__M_END_DECLS

#endif // __EDITOR_ROBO_ED_H
