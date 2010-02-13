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

#ifndef __ROBOT_H
#define __ROBOT_H

#include "compat.h"

__M_BEGIN_DECLS

#include "robot_struct.h"
#include "board_struct.h"
#include "world_struct.h"

// Let's not let a robot's stack get larger than 64k right now.
// The value is a bit arbitrary, but it's mainly there to prevent MZX from
// crashing when under infinite recursion.

#define ROBOT_START_STACK 4
#define ROBOT_MAX_STACK   65536

#ifdef CONFIG_DEBYTECODE

// This is the version where programs became source code instead of
// bytecode.
#define VERSION_PROGRAM_SOURCE   0x025A

// And the last one where bytecode was used.
#define VERSION_PROGRAM_BYTECODE 0x0253

CORE_LIBSPEC void change_robot_name(struct board *src_board,
 struct robot *cur_robot, char *new_name);
CORE_LIBSPEC void add_robot_name_entry(struct board *src_board,
 struct robot *cur_robot, char *name);
CORE_LIBSPEC int find_free_robot(struct board *src_board);

void prepare_robot_bytecode(struct robot *cur_robot);

#else /* !CONFIG_DEBYTECODE */

CORE_LIBSPEC void reallocate_robot(struct robot *robot, int size);
CORE_LIBSPEC struct label **cache_robot_labels(struct robot *robot,
 int *num_labels);

void clear_label_cache(struct label **label_list, int num_labels);
void change_robot_name(struct board *src_board, struct robot *cur_robot,
 char *new_name);
void add_robot_name_entry(struct board *src_board, struct robot *cur_robot,
 char *name);
int find_free_robot(struct board *src_board);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC void duplicate_robot_direct(struct robot *cur_robot,
 struct robot *copy_robot, int x, int y);
#endif

#endif /* !CONFIG_DEBYTECODE */

CORE_LIBSPEC void clear_robot_contents(struct robot *cur_robot);
CORE_LIBSPEC void clear_robot_id(struct board *src_board, int id);
CORE_LIBSPEC void clear_scroll_id(struct board *src_board, int id);
CORE_LIBSPEC void clear_sensor_id(struct board *src_board, int id);
CORE_LIBSPEC void replace_robot(struct board *src_board,
 struct robot *src_robot, int dest_id);
CORE_LIBSPEC int duplicate_robot(struct board *src_board,
 struct robot *cur_robot, int x, int y);
CORE_LIBSPEC int duplicate_scroll(struct board *src_board,
 struct scroll *cur_scroll);
CORE_LIBSPEC int duplicate_sensor(struct board *src_board,
 struct sensor *cur_sensor);
CORE_LIBSPEC void send_robot_def(struct world *mzx_world, int robot_id,
 int mesg_id);
CORE_LIBSPEC void optimize_null_objects(struct board *src_board);

CORE_LIBSPEC int place_at_xy(struct world *mzx_world, enum thing id,
 int color, int param, int x, int y);
CORE_LIBSPEC int place_player_xy(struct world *mzx_world, int x, int y);
CORE_LIBSPEC void setup_overlay(struct board *src_board, int mode);
CORE_LIBSPEC void replace_player(struct world *mzx_world);

struct robot *load_robot_allocate(FILE *fp, int savegame, int version);
void load_robot(struct robot *cur_robot, FILE *fp, int savegame, int version);
struct scroll *load_scroll_allocate(FILE *fp);
struct sensor *load_sensor_allocate(FILE *fp);
void save_robot(struct robot *cur_robot, FILE *fp, int savegame);
void save_scroll(struct scroll *cur_scroll, FILE *fp, int savegame);
void save_sensor(struct sensor *cur_sensor, FILE *fp, int savegame);
void clear_robot(struct robot *cur_robot);
void clear_scroll(struct scroll *cur_scroll);
void clear_sensor(struct sensor *cur_sensor);
void reallocate_scroll(struct scroll *scroll, size_t size);
int find_robot(struct board *src_board, const char *name,
 int *first, int *last);
void send_robot(struct world *mzx_world, char *name, const char *mesg,
 int ignore_lock);
int send_robot_id(struct world *mzx_world, int id, const char *mesg,
 int ignore_lock);
void send_robot_all(struct world *mzx_world, const char *mesg);
int send_robot_self(struct world *mzx_world, struct robot *src_robot,
 const char *mesg);
int move_dir(struct board *src_board, int *x, int *y, enum dir dir);
void prefix_first_last_xy(struct world *mzx_world, int *fx, int *fy,
 int *lx, int *ly, int robotx, int roboty);
void prefix_mid_xy(struct world *mzx_world, int *mx, int *my, int x, int y);
void prefix_last_xy_var(struct world *mzx_world, int *lx, int *ly,
 int robotx, int roboty, int width, int height);
void prefix_mid_xy_var(struct world *mzx_world, int *mx, int *my,
 int robotx, int roboty, int width, int height);
void prefix_first_xy_var(struct world *mzx_world, int *fx, int *fy,
 int robotx, int roboty, int width, int height);
int fix_color(int color, int def);
int restore_label(struct robot *cur_robot, char *label);
int zap_label(struct robot *cur_robot, char *label);
int next_param(char *ptr, int pos);
char *next_param_pos(char *ptr);
int parse_param(struct world *mzx_world, char *robot, int id);
enum thing parse_param_thing(struct world *mzx_world, char *program);
enum dir parse_param_dir(struct world *mzx_world, char *program);
enum equality parse_param_eq(struct world *mzx_world, char *program);
enum condition parse_param_cond(struct world *mzx_world, char *program,
 enum dir *direction);
void robot_box_display(struct world *mzx_world, char *program,
 char *label_storage, int id);
void push_sensor(struct world *mzx_world, int id);
void step_sensor(struct world *mzx_world, int id);
char *tr_msg_ext(struct world *mzx_world, char *mesg, int id, char *buffer,
 char terminating_char);
int get_robot_id(struct board *src_board, const char *name);

static inline char *tr_msg(struct world *mzx_world, char *mesg, int id,
 char *buffer)
{
  return tr_msg_ext(mzx_world, mesg, id, buffer, 0);
}

void run_robot(struct world *mzx_world, int id, int x, int y);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC void duplicate_scroll_direct(struct scroll *cur_scroll,
 struct scroll *copy_scroll);
CORE_LIBSPEC void duplicate_sensor_direct(struct sensor *cur_sensor,
 struct sensor *copy_sensor);

CORE_LIBSPEC void copy_buffer_to_layer(int x, int y, int width, int height,
 char *src_char, char *src_color, char *dest_char,
 char *dest_color, int layer_width);
CORE_LIBSPEC void copy_layer_to_board(struct board *src_board, int x, int y,
 int width, int height, char *src_char, char *src_color,
 int src_width, enum thing convert_id);
CORE_LIBSPEC void copy_layer_to_buffer(int x,  int y, int width, int height,
 char *src_char, char *src_color, char *dest_char,
 char *dest_color, int layer_width);
CORE_LIBSPEC void copy_board_to_board_buffer(struct board *src_board,
 int x, int y, int width, int height, char *dest_id, char *dest_param,
 char *dest_color, char *dest_under_id, char *dest_under_param,
 char *dest_under_color, struct board *dest_board);
CORE_LIBSPEC void copy_board_buffer_to_board(struct board *src_board,
 int x, int y, int width, int height, char *src_id, char *src_param,
 char *src_color, char *src_under_id, char *src_under_param,
 char *src_under_color);
CORE_LIBSPEC void copy_board_to_layer(struct board *src_board,
 int x, int y, int width, int height, char *dest_char, char *dest_color,
 int dest_width);

CORE_LIBSPEC extern const int def_params[128];
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __ROBOT_H
