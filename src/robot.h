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
#define ROBOT_MAX_TR      512

Robot *load_robot_allocate(FILE *fp, int savegame);
void load_robot(Robot *cur_robot, FILE *fp, int savegame);
Scroll *load_scroll_allocate(FILE *fp, int savegame);
Sensor *load_sensor_allocate(FILE *fp, int savegame);
void save_robot(Robot *cur_robot, FILE *fp, int savegame);
void save_scroll(Scroll *cur_scroll, FILE *fp, int savegame);
void save_sensor(Sensor *cur_sensor, FILE *fp, int savegame);
void clear_robot(Robot *cur_robot);
void clear_scroll(Scroll *cur_scroll);
void clear_sensor(Sensor *cur_sensor);
void clear_robot_id(Board *src_board, int id);
void clear_scroll_id(Board *src_board, int id);
void clear_sensor_id(Board *src_board, int id);
void reallocate_scroll(Scroll *scroll, int size);
void reallocate_robot(Robot *robot, int size);
Label **cache_robot_labels(Robot *robot, int *num_labels);
void clear_label_cache(Label **label_list, int num_labels);
int find_robot(Board *src_board, char *name, int *first, int *last);
void send_robot(World *mzx_world, char *name, char *mesg,
 int ignore_lock);
int send_robot_id(World *mzx_world, int id, char *mesg, int ignore_lock);
void send_robot_def(World *mzx_world, int robot_id, int mesg_id);
void send_robot_all(World *mzx_world, char *mesg);
int send_robot_self(World *mzx_world, Robot *src_robot, char *mesg);
int move_dir(Board *src_board, int *x, int *y, int dir);
void prefix_first_last_xy(World *mzx_world, int *fx, int *fy,
 int *lx, int *ly, int robotx, int roboty);
void prefix_mid_xy(World *mzx_world, int *mx, int *my, int x, int y);
void prefix_last_xy_var(World *mzx_world, int *lx, int *ly,
 int robotx, int roboty, int width, int height);
void prefix_mid_xy_var(World *mzx_world, int *mx, int *my,
 int robotx, int roboty, int width, int height);
void prefix_first_xy_var(World *mzx_world, int *fx, int *fy,
 int robotx, int roboty, int width, int height);
int fix_color(int color, int def);
int restore_label(Robot *cur_robot, char *label);
int zap_label(Robot *cur_robot, char *label);
int next_param(char *ptr, int pos);
char *next_param_pos(char *ptr);
int parse_param(World *mzx_world, char *robot, int id);
mzx_thing parse_param_thing(World *mzx_world, char *program);
mzx_dir parse_param_dir(World *mzx_world, char *program);
mzx_equality parse_param_eq(World *mzx_world, char *program);
mzx_condition parse_param_cond(World *mzx_world, char *program,
 mzx_dir *direction);
void robot_box_display(World *mzx_world, char *program,
 char *label_storage, int id);
void push_sensor(World *mzx_world, int id);
void step_sensor(World *mzx_world, int id);
char *tr_msg(World *mzx_world, char *mesg, int id, char *buffer);
void add_robot_name_entry(Board *src_board, Robot *cur_robot, char *name);
void change_robot_name(Board *src_board, Robot *cur_robot, char *new_name);
int find_free_robot(Board *src_board);
int duplicate_robot(Board *src_board, Robot *cur_robot, int x, int y);
void replace_robot(Board *src_board, Robot *src_robot, int dest_id);
int duplicate_scroll(Board *src_board, Scroll *cur_scroll);
int duplicate_sensor(Board *src_board, Sensor *cur_sensor);
void optimize_null_objects(Board *src_board);
int get_robot_id(Board *src_board, char *name);

// These are part of runrobo2.cpp
int place_player_xy(World *mzx_world, int x, int y);
void run_robot(World *mzx_world, int id, int x, int y);
void clear_layer_block(int src_x, int src_y, int width,
 int height, char *dest_char, char *dest_color, int dest_width);
void clear_board_block(Board *src_board, int x, int y,
 int width, int height);
void setup_overlay(Board *src_board, int mode);
void replace_player(World *mzx_world);

#ifdef CONFIG_EDITOR
void create_blank_robot_direct(Robot *cur_robot, int x, int y);
void create_blank_scroll_direct(Scroll *cur_croll);
void create_blank_sensor_direct(Sensor *cur_sensor);
void clear_robot_contents(Robot *cur_robot);
void clear_scroll_contents(Scroll *cur_scroll);
void duplicate_robot_direct(Robot *cur_robot, Robot *copy_robot,
 int x, int y);
void duplicate_scroll_direct(Scroll *cur_scroll, Scroll *copy_scroll);
void duplicate_sensor_direct(Sensor *cur_sensor, Sensor *copy_sensor);
int place_at_xy(World *mzx_world, mzx_thing id, int color,
 int param, int x, int y);
void replace_scroll(Board *src_board, Scroll *src_scroll, int dest_id);
void replace_sensor(Board *src_board, Sensor *src_sensor, int dest_id);

void copy_buffer_to_layer(int x, int y, int width, int height,
 char *src_char, char *src_color, char *dest_char,
 char *dest_color, int layer_width);
void copy_layer_to_board(Board *src_board, int x, int y,
 int width, int height, char *src_char, char *src_color,
 int src_width, mzx_thing convert_id);
void copy_layer_to_buffer(int x,  int y, int width, int height,
 char *src_char, char *src_color, char *dest_char,
 char *dest_color, int layer_width);
void copy_board_to_board_buffer(Board *src_board, int x, int y,
 int width, int height, char *dest_id, char *dest_param,
 char *dest_color, char *dest_under_id, char *dest_under_param,
 char *dest_under_color, Board *dest_board);
void copy_board_buffer_to_board(Board *src_board, int x, int y,
 int width, int height, char *src_id, char *src_param,
 char *src_color, char *src_under_id, char *src_under_param,
 char *src_under_color);
void copy_board_to_layer(Board *src_board, int x, int y,
 int width, int height, char *dest_char, char *dest_color,
 int dest_width);
#endif // CONFIG_EDITOR

__M_END_DECLS

#endif // __ROBOT_H
