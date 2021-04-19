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

enum text_line_end
{
  TEXT_LINES_UNIX,
  TEXT_LINES_DOS,
  TEXT_LINES_MAC
};

struct text_region
{
  unsigned int length;
  unsigned int num_lines;
};

struct text_document
{
  char *edit_buffer;
  size_t edit_buffer_alloc;

  // These are provided to intake2().
  int document_size;
  int document_pos;

  char *back_ptr;
  size_t front_size;
  size_t front_extra;
  size_t back_size;
  size_t back_extra;

  /* Region data for less slowly determining the current line number/column and
   * line numbers of arbitrary positions. Allocated and maintained only after
   * a line-based function is called.
   */
  struct text_region *regions;
  size_t num_regions;
  size_t num_regions_alloc;
  size_t region_min;
  size_t region_max;
  size_t region;
  size_t region_start;
  size_t region_start_line;
  enum text_line_end line_type;
  int num_lines;
  int current_line;
  int current_col;
  int current_length;
};

struct text_area
{
  // NOTE: not guaranteed to be terminated unless it's the full document.
  const char *text;
  int length;
  int document_offset;
};

boolean text_move_position(struct text_document *td, size_t pos);
boolean text_move_lines(struct text_document *td, int amount);
boolean text_move_to_line(struct text_document *td, int line_number, int column);
boolean text_move_start(struct text_document *td);
boolean text_move_end(struct text_document *td);

boolean text_init(struct text_document *td, const char *src, size_t src_len);
void text_clear(struct text_document *td);

__M_END_DECLS

#endif /* __EDITOR_TEXT_H */
