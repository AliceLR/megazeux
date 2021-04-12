/* MegaZeux
 *
 * Copyright (C) 2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __EDITOR_TEXT_H
#define __EDITOR_TEXT_H

#include "../compat.h"

__M_BEGIN_DECLS

struct text_document
{
  char *edit_buffer;
  size_t edit_buffer_alloc;
  int edit_buffer_size;
  int num_lines;
  int current_line;
  int current_col;
};

boolean text_move(struct text_document *td, int amount);
boolean text_move_to_line(struct text_document *td, int line_number, int pos);
boolean text_move_start(struct text_document *td);
boolean text_move_end(struct text_document *td);

boolean text_init(struct text_document *td, const char *src, size_t src_len);
void text_clear(struct text_document *td);

__M_END_DECLS

#endif /* __EDITOR_TEXT_H */
