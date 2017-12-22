/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef __EDITOR_BLOCK_H
#define __EDITOR_BLOCK_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../board_struct.h"

void clear_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height);
void clear_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height);

void flip_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height);
void flip_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height);

void mirror_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height);
void mirror_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height);

void paint_layer_block(char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height, int paint_color);

void copy_layer_buffer_to_buffer(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height);

void move_layer_block(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height,
 int clear_width, int clear_height);

__M_END_DECLS

#endif  // __EDITOR_BLOCK_H
