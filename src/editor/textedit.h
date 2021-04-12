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

#ifndef __EDITOR_TEXTEDIT_H
#define __EDITOR_TEXTEDIT_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../core.h"

enum text_edit_type
{
  TE_BOX,
  TE_DIALOG
};

struct text_line
{
  struct text_line *prev;
  struct text_line *next;
  int size;
  char data[];
};

struct text_document
{
  struct text_line *head;
  struct text_line *tail;
  struct text_line *current;
  char *edit_buffer;
  size_t edit_buffer_alloc;
  int edit_buffer_size;
  int num_lines;
  int current_line;
  int current_col;
};

boolean text_update_current(struct text_document *td);
struct text_line *text_insert_line(struct text_document *td,
 struct text_line *at, int init_size, int direction);
boolean text_delete_line(struct text_document *td, struct text_line *t);

boolean text_split_current(struct text_document *td);
boolean text_join_current(struct text_document *td, int direction);
boolean text_insert_current(struct text_document *td,
 const char *src, size_t src_len, char linebreak_char);

struct text_line *text_get_line(struct text_document *td, int line_number);
boolean text_move(struct text_document *td, int amount);
boolean text_move_to_line(struct text_document *td, int line_number, int pos);
boolean text_move_start(struct text_document *td);
boolean text_move_end(struct text_document *td);

/**
 * Context functions.
 */

void text_editor(context *parent, int x, int y, int w, int h, int type,
 const char *title, const char *src, size_t src_len, void *p,
 void (*flush_cb)(struct world *, void *, char *, size_t));

void hex_editor(context *parent, int x, int y, int w, int h,
 const char *title, const void *src, size_t src_len, void *p,
 void (*flush_cb)(struct world *, void *, char *, size_t));

__M_END_DECLS

#endif /* __EDITOR_TEXTEDIT_H */
