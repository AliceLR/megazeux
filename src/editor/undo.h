/* MegaZeux
 *
 * Copyright (C) 2017 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __EDITOR_UNDO_H
#define __EDITOR_UNDO_H

#include "compat.h"

__M_BEGIN_DECLS

#include "data.h"
#include "world_struct.h"

struct undo_frame
{
  int type;
};

struct undo_history
{
  struct undo_frame **frames;
  struct undo_frame *current_frame;
  int current;
  int first;
  int last;
  int size;
  void (*undo_function)(struct undo_frame *);
  void (*redo_function)(struct undo_frame *);
  void (*update_function)(struct undo_frame *);
  void (*clear_function)(struct undo_frame *);
};

int apply_undo(struct undo_history *h);
int apply_redo(struct undo_history *h);
void update_undo_frame(struct undo_history *h);
void destruct_undo_history(struct undo_history *h);

struct undo_history *construct_charset_undo_history(int max_size);
struct undo_history *construct_board_undo_history(int max_size);
struct undo_history *construct_layer_undo_history(int max_size);

void add_charset_undo_frame(struct undo_history *h, int offset,
 int width, int height);

void add_board_undo_frame(struct world *mzx_world, struct undo_history *h,
 enum thing id, int color, int param, int x, int y, struct robot *copy_robot,
 struct scroll *copy_scroll, struct sensor *copy_sensor);

void add_board_undo_position(struct undo_history *h, int x, int y);

void add_block_undo_frame(struct world *mzx_world, struct undo_history *h,
 struct board *src_board, int src_offset, int width, int height);

void add_layer_undo_frame(struct undo_history *h, char *layer_chars,
 char *layer_colors, int layer_width, int layer_offset, int width, int height);

__M_END_DECLS

#endif // __EDITOR_UNDO_H
