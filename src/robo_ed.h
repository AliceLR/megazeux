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

#define MAX_MACRO_RECURSION 16
#define MAX_MACRO_REPEAT 128

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
  char command_buffer_space[512];
  int macro_recurse_level;
  int macro_repeat_level;

  World *mzx_world;
};

extern char macros[5][64];

void robot_editor(World *mzx_world, Robot *cur_robot);
void move_and_update(robot_state *rstate, int count);
void move_line_up(robot_state *rstate, int count);
void move_line_down(robot_state *rstate, int count);
void strip_ccodes(char *dest, char *src);
int update_current_line(robot_state *rstate);
void add_blank_line(robot_state *rstate, int relation);
void display_robot_line(robot_state *rstate,
 robot_line *current_rline, int y);
void delete_current_line(robot_state *rstate, int move);
int validate_lines(robot_state *rstate, int show_none);
void insert_string(char *dest, char *string, int *position);
int block_menu(World *mzx_world);
void add_line(robot_state *rstate);
void paste_buffer(robot_state *rstate);
void copy_block_to_buffer(robot_state *rstate);
void clear_block(robot_state *rstate);
void export_block(robot_state *rstate, int region_default);
void import_block(robot_state *rstate);
void edit_settings(World *mzx_world);
void goto_line(robot_state *rstate, int line);
void block_action(robot_state *rstate);
int find_string(robot_state *rstate, char *str, int wrap,
 int *position, int case_sensitive);
void find_replace_action(robot_state *rstate);
void str_lower_case(char *str, char *dest);
void replace_current_line(robot_state *rstate, int r_pos, char *str,
 char *replace);
void execute_macro(robot_state *rstate,
 ext_macro *macro_src);
void execute_numbered_macro(robot_state *rstate, int num);
void output_macro(robot_state *rstate, ext_macro *macro_src);
void execute_named_macro(robot_state *rstate, char *macro_name);
void macro_default_values(robot_state *rstate, ext_macro *macro_src);

#endif
