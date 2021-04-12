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

#include "text.h"

#include "../util.h"

#include <stdlib.h>
#include <string.h>

#define TEXT_EDIT_BUFFER_MIN 256

static void text_set_edit_buffer(struct text_document *td, size_t size)
{
  size_t size_pw = size - 1;
  size_pw |= size_pw >> 1;
  size_pw |= size_pw >> 2;
  size_pw |= size_pw >> 4;
  size_pw |= size_pw >> 8;
  size_pw |= size_pw >> 16;
  size_pw = MAX(size_pw + 1, TEXT_EDIT_BUFFER_MIN);

  if(td->edit_buffer_alloc < size_pw)
  {
    trace("--TEXTDOC-- text_set_edit_buffer %p size %zu (%zu -> %zu)\n",
     (void *)td, size, td->edit_buffer_alloc, size_pw);

    td->edit_buffer = (char *)crealloc(td->edit_buffer, size_pw);
    td->edit_buffer_alloc = size_pw;
  }
}

/**
 * Increase/decrease the current line in the text document by an amount. If the
 * full amount provided can not be moved, the current position will be moved as
 * far as possible in the provided direction. The current column is not modified.
 * On success, this function flushes the current buffer to its respective line
 * and updates `current_line`, `current`, and the edit buffer for the new line.
 *
 * If there are no lines in the text document, this will fail and the text
 * document state will be left unmodified.
 *
 * @param   td      text document.
 * @param   amount  number of lines to move down (positive) or up (negative).
 * @return          `true` on success, otherwise `false`.
 */
boolean text_move(struct text_document *td, int amount)
{
  return false; // FIXME
}

/**
 * Set the current line and column in line for the text document.
 * On success, this function flushes the current buffer to its respective line
 * and updates `current_line`, `current_col`, `current`, and the edit buffer
 * for the new line.
 *
 * If the given line number doesn't exist, this function will fail and the
 * text document state will be left unmodified. The given column will be
 * clamped to a valid position within the new line on success.
 *
 * @param   td            text document.
 * @param   line_number   line number in the document to jump to.
 * @param   pos           position within line to jump to.
 * @return                `true` on success, otherwise `false`.
 */
boolean text_move_to_line(struct text_document *td, int line_number, int pos)
{
  trace("--TEXTDOC-- text_move_to_line %p line:%d col:%d\n", (void *)td,
   line_number, pos);

  return false; // FIXME
}

/**
 * Set the current line and column to the start of the text document.
 * On success, this function flushes the current buffer to its respective line
 * and updates `current_line`, `current_col`, `current`, and the edit buffer
 * for the new line.
 *
 * If there are no lines in the text document, this will fail and the text
 * document state will be left unmodified.
 *
 * @param   td  text document.
 * @return      `true` on success, otherwise `false`.
 */
boolean text_move_start(struct text_document *td)
{
  return false; // FIXME
}

/**
 * Set the current line and column to the end of the text document.
 * On success, this function flushes the current buffer to its respective line
 * and updates `current_line`, `current_col`, `current`, and the edit buffer
 * for the new line.
 *
 * If there are no lines in the text document, this will fail and the text
 * document state will be left unmodified.
 *
 * @param   td  text document.
 * @return      `true` on success, otherwise `false`.
 */
boolean text_move_end(struct text_document *td)
{
  return false; // FIXME
}

void text_clear(struct text_document *td)
{
  trace("--TEXTDOC-- text_clear %p\n", (void *)td);
  free(td->edit_buffer);

  td->edit_buffer = NULL;
  td->edit_buffer_size = 0;
  td->edit_buffer_alloc = 0;
  td->num_lines = 0;
  td->current_line = 0;
}

boolean text_init(struct text_document *td, const char *src, size_t src_len)
{
  const char *pos = src;
  const char *end = src + src_len;
  const char *next;
  size_t len;

  trace("--TEXTDOC-- text_init %p (size %zu)\n", (void *)td, src_len);
  if(td->edit_buffer)
    return false;

  // FIXME

  // Insert an extra line if there's a terminating EOL.
  // FIXME

  return true;
}
