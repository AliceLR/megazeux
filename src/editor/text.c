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

#include "../platform_endian.h"
#include "../util.h"

#include <stdlib.h>
#include <string.h>

#define TEXT_BUFFER_MIN       256
#define TEXT_BUFFER_MIN_GAP   128

static size_t next_power_of_two_cbrt_z(size_t value)
{
  size_t cube = 1;
  size_t i;
#if ARCHITECTURE_BITS >= 64
  for(i = 1; i < 21; i++)
#else
  for(i = 1; i < 10; i++)
#endif
  {
    cube <<= 3;
    if(cube > value)
      break;
  }
  return 1 << i;
}

static size_t next_power_of_two_z(size_t value)
{
  value--;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
#if ARCHITECTURE_BITS >= 64
  value |= value >> 32;
#endif
  return value + 1;
}

/**
 * Set the edit buffer allocation to an appropriate size for the current
 * document. Actual buffer size can be calculated by
 *
 * MAX(nextpow2(size + TEXT_BUFFER_MIN_GAP), TEXT_BUFFER_MIN) + 1
 *
 * where TEXT_BUFFER_MIN_GAP + 1 is extra space allocated for the gap and for
 * a final nul byte (to allow for safe degapping to either the front or the
 * back of the edit buffer). The nul byte is not counted by td->edit_buffer_alloc.
 */
static void text_set_edit_buffer(struct text_document *td, size_t size)
{
  size_t size_pw = next_power_of_two_z(size + TEXT_BUFFER_MIN_GAP);
  size_pw = MAX(size_pw, TEXT_BUFFER_MIN);

  if(td->edit_buffer_alloc < size_pw)
  {
    ptrdiff_t back_offset = td->back_ptr - td->edit_buffer;
    char *new_back;

    trace("--TEXT-- text_set_edit_buffer %p size %zu (%zu -> %zu)\n",
     (void *)td, size, td->edit_buffer_alloc, size_pw);

    td->edit_buffer = (char *)crealloc(td->edit_buffer, size_pw + 1);
    td->edit_buffer_alloc = size_pw;
    td->edit_buffer[td->edit_buffer_alloc] = '\0';

    // Move back to the end of the buffer.
    if(td->back_ptr && td->back_size)
    {
      td->back_ptr = td->edit_buffer + back_offset;
      new_back = td->edit_buffer + td->edit_buffer_alloc - td->back_size;
      memmove(new_back, td->back_ptr, td->back_size);
      td->back_ptr = new_back;
      td->back_extra = 0;
    }
  }
}

/**
 * Move the contents of all source text regions into a single region.
 * This will overwrite the contents of the destination region. The destination
 * region may be included in the source region(s).
 */
static void region_combine(struct text_region *dest,
 const struct text_region *src, size_t num_src)
{
  size_t length = 0;
  size_t lines = 0;
  size_t i;

  for(i = 0; i < num_src; i++)
  {
    lines += src[i].num_lines;
    length += src[i].length;
  }

  dest->num_lines = lines;
  dest->length = length;
}

/**
 * Get the next line in a range, starting at the position pointed to by `pos`
 * and of the length pointed to by `length`. These values will be updated to
 * reflect the position of the current line and remaining length.
 */
static char *text_next_line_in_range(struct text_document *td, size_t *pos,
 size_t *length)
{
  char line_end_char = td->line_type == TEXT_LINES_MAC ? '\r' : '\n';
  char *front_end = td->edit_buffer + td->front_size;
  char *start = td->edit_buffer + *pos;
  size_t end = *pos + *length;
  size_t prev_line_len;

  // Before gap.
  if(*pos < td->front_size)
  {
    size_t stop = MIN(end, td->front_size);
    char *line_end = (char *)memchr(start, line_end_char, stop);
    if(line_end && line_end + 1 < front_end)
    {
      prev_line_len = line_end + 1 - start;
      *pos += prev_line_len;
      *length -= prev_line_len;
      return line_end + 1;
    }
    prev_line_len = td->front_size - *pos;
    *pos += prev_line_len;
    *length -= prev_line_len;
  }

  // After gap.
  start = td->back_ptr + *pos - td->front_size;
  if(end > td->front_size)
  {
    size_t stop = MIN(end - td->front_size, td->back_size);
    char *line_end = (char *)memchr(start, line_end_char, stop);
    if(line_end)
    {
      prev_line_len = line_end + 1 - start;
      *pos += prev_line_len;
      *length -= prev_line_len;
      return line_end + 1;
    }
  }
  *pos = td->document_size;
  *length = 0;
  return NULL;
}

/**
 * Navigate to a given line number and column.
 */
static boolean text_set_line(struct text_document *td, int line_number, int column)
{
  struct text_region *tr = NULL;
  size_t region_line = 0;
  size_t region_pos = 0;
  size_t region_length;
  size_t region_start;
  size_t region_start_line;
  size_t region;
  size_t line_start;
  size_t line_length;
  size_t target_line = line_number;
  size_t target_column;
  size_t next_pos;

  // Find region.
  for(region = 0; region < td->num_regions; region++)
  {
    tr = &(td->regions[region]);
    if(target_line < region_line + tr->num_lines)
      break;

    region_pos += tr->length;
    region_line += tr->num_lines;
  }
  if(!tr)
    return false;

  // Find line in region.
  line_start = region_pos;
  region_start = region_pos;
  region_start_line = region_line;
  region_length = tr->length;
  while(text_next_line_in_range(td, &region_pos, &region_length))
  {
    if(target_line < region_line)
      break;

    line_start = region_pos;
    region_line++;
  }

  line_length = region_pos - line_start; // FIXME line end
  target_column = CLAMP((size_t)column, 0, line_length);

  td->document_pos = region_pos + target_column;
  td->current_line = target_line;
  td->current_col = target_column;
  td->current_length = line_length;
  td->region = region;
  td->region_start = region_start;
  td->region_start_line = region_start_line;

  return false;
}

/**
 * Determine the current region, line, line length, and column from the current
 * position in the document.
 */
static boolean text_recompute_region(struct text_document *td)
{
  struct text_region *tr = NULL;
  size_t region_line = 0;
  size_t region_pos = 0;
  size_t region_length;
  size_t region_start;
  size_t region_start_line;
  size_t region;
  size_t line_start;
  size_t pos = td->document_pos;

  // Find region.
  for(region = 0; region < td->num_regions; region++)
  {
    tr = &(td->regions[region]);
    if(pos <= region_pos + tr->length)
      break;

    region_pos += tr->length;
    region_line += tr->num_lines;
  }
  if(!tr)
    return false;

  // Find line in region.
  line_start = region_pos;
  region_start = region_pos;
  region_start_line = region_line;
  region_length = tr->length;
  while(text_next_line_in_range(td, &region_pos, &region_length))
  {
    if(pos < region_pos)
      break;

    line_start = region_pos;
    region_line++;
  }

  if(line_start > (size_t)td->document_size)
    return false;

  td->current_line = region_line;
  td->current_col = pos - line_start;
  td->current_length = region_pos - line_start; // FIXME line end
  td->region = region;
  td->region_start = region_start;
  td->region_start_line = region_start_line;

  return true;
}

/**
 * Scan a region to determine its line count.
 */
static void text_scan_region(struct text_document *td, struct text_region *tr,
 size_t start)
{
  size_t lines = (start == 0) ? 1 : 0;
  size_t length = tr->length;
  size_t pos = start;

  while(text_next_line_in_range(td, &pos, &length))
    lines++;

  tr->num_lines = lines;
}

/**
 * Initialize text editing line length metadata.
 */
static void text_initialize_regions(struct text_document *td)
{
  if(!td->regions)
  {
    size_t num_regions = next_power_of_two_cbrt_z(td->document_size);
    size_t num_regions_alloc = num_regions * 2;
    size_t target_size = (td->document_size / num_regions) + 1;
    size_t running_length = 0;
    size_t i;
    td->regions = (struct text_region *)ccalloc(num_regions_alloc, sizeof(struct text_region));
    td->num_regions = num_regions;
    td->num_regions_alloc = num_regions_alloc;

    td->num_lines = 0;
    for(i = 0; i < num_regions; i++)
    {
      struct text_region *tr = &(td->regions[i]);
      tr->length = MIN(target_size, td->document_size - running_length);

      text_scan_region(td, tr, running_length);

      running_length += tr->length;
      td->num_lines += tr->num_lines;
    }
    if(!text_recompute_region(td))
      warn("fuk\n");
  }
}

/**
 * Set the absolute position of the cursor within the document.
 */
static boolean text_set_position(struct text_document *td, size_t pos)
{
  size_t old_pos;
  if(!td || pos > (size_t)td->document_size)
    return false;

  // FIXME gap management?
  old_pos = td->document_pos;
  td->document_pos = pos;

  if(td->regions)
  {
    struct text_region *tr = &(td->regions[td->region]);
    // FIXME refresh line data
    // FIXME only do this if it can't easily be calculated off of relative info.
    return text_recompute_region(td);
  }
  return true;
}

/**
 * Set the absolute position of the cursor within the document.
 * FIXME documentation
 */
boolean text_move_position(struct text_document *td, size_t pos)
{
  trace("--TEXT-- text_move_position %p pos %zu\n", (void *)td, pos);
  return text_set_position(td, pos);
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
boolean text_move_lines(struct text_document *td, int amount)
{
  trace("--TEXT-- text_move_lines %p amount:%d\n", (void *)td, amount);

  text_initialize_regions(td);

  return text_set_line(td, td->current_line + amount, td->current_col);
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
 * @param   column        position within line to jump to.
 * @return                `true` on success, otherwise `false`.
 */
boolean text_move_to_line(struct text_document *td, int line_number, int column)
{
  trace("--TEXT-- text_move_to_line %p line:%d col:%d\n", (void *)td,
   line_number, column);

  text_initialize_regions(td);

  return text_set_line(td, line_number, column);
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
  trace("--TEXT-- text_move_start %p\n", (void *)td);
  return text_set_position(td, 0);
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
  trace("--TEXT-- text_move_end %p\n", (void *)td);
  return text_set_position(td, td->document_size);
}

void text_clear(struct text_document *td)
{
  trace("--TEXT-- text_clear %p\n", (void *)td);

  free(td->regions);
  td->regions = NULL;
  td->num_regions = 0;

  free(td->edit_buffer);
  td->edit_buffer = NULL;
  td->back_ptr = NULL;
}

boolean text_init(struct text_document *td, const char *src, size_t src_len)
{
  trace("--TEXT-- text_init %p (size %zu)\n", (void *)td, src_len);
  memset(td, 0, sizeof(struct text_document));

  text_set_edit_buffer(td, src_len);
  td->back_ptr = td->edit_buffer + td->edit_buffer_alloc - src_len;

  memcpy(td->back_ptr, src, src_len);
  td->back_size = src_len;
  td->front_size = 0;

  td->document_size = src_len;
  td->document_pos = 0;

  return true;
}
