/* $Id$
 * MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick - exophase@adelphia.net
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

#ifndef __ROBO_ED_H
#define __ROBO_ED_H

#include "rasm.h"
#include "world.h"

typedef struct _robot_line robot_line;

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

extern char macros[5][64];

void robot_editor(World *mzx_world, Robot *cur_robot);
robot_line *move_line_up(robot_line *current_rline, int count,
 int *current_line);
robot_line *move_line_down(robot_line *current_rline, int count,
 int *current_line);
void strip_ccodes(char *dest, char *src);
int change_line(robot_line *current_rline, char *command_buffer,
 int include_ignores, int base, validity_types invalid_status,
 int *size);
int add_blank_line(robot_line *next_rline, robot_line *previous_rline,
 int *size);
void display_robot_line(robot_line *current_rline, int y,
 int color_code, char *color_codes);
int remove_line(robot_line *current_rline);
int validate_lines(World *mzx_world, robot_line *current_rline,
 int show_none, int *size);
void insert_string(char *dest, char *string, int *position);
int block_menu(World *mzx_world);
int add_line(robot_line *next_rline, robot_line *previous_rline,
 int *size, char *command_buffer, int include_ignores, int base,
 validity_types invalid_status);
void paste_buffer(robot_line *current_rline, int *total_lines,
 int *current_line, int *size, int include_ignores, int base,
 validity_types invalid_status);
void copy_block_to_buffer(robot_line *start_rline, int num_lines);
robot_line *clear_block(int first_line, robot_line *first_rline,
 int num_lines, int *current_line, int *total_lines, int *size,
 robot_line *cursor_rline);
void export_block(World *mzx_world, robot_line *base,
 robot_line *block_start, robot_line *block_end, int region_default);
void import_block(World *mzx_world, robot_line *current_rline,
 int *total_lines, int *current_line, int *size, int include_ignores,
 int base, validity_types invalid_status);
void edit_settings(World *mzx_world);

#endif
