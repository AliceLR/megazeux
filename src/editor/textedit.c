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

#include "textedit.h"
#include "clipboard.h"

#include "../event.h"
#include "../graphics.h"
#include "../intake.h"
#include "../util.h"
#include "../window.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_EDIT_BUFFER_MIN 256

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
};

struct text_edit_context
{
  context ctx;
  subcontext *intk;
  struct text_document td;
  char *title;
  enum text_edit_type type;
  int x;
  int y;
  int w;
  int h;
  int edit_x;
  int edit_y;
  int edit_w;
  int edit_h;
  int current_col;
  int intk_row;
  int text_color;
  int text_highlight;
  int border_color;
  int border_highlight;
  boolean dos_line_ends;
  void *priv;
  void (*flush_cb)(struct world *mzx_world, void *priv, char *src, size_t src_len);
};

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

    td->edit_buffer = crealloc(td->edit_buffer, size_pw);
    td->edit_buffer_alloc = size_pw;
  }
}

/**
 * Resize the given line to ensure at least `size` characters, plus a null
 * terminator, will fit in the line.
 */
static boolean text_resize_line(struct text_document *td,
 struct text_line **_t, int size)
{
  struct text_line *old;
  struct text_line *t;
  if(!td || !_t || !*_t)
    return false;

  old = t = *_t;
  if(t->size == size)
    return true;

  trace("--TEXTDOC-- text_resize_line %p . %p (size %d) to %d\n",
   (void *)td, (void *)t, t->size, size);

  if(t->size != size)
  {
    t = crealloc(t, sizeof(struct text_line) + size + 1);
    t->size = size;
    t->data[size] = '\0';
  }

  if(t->prev)
    t->prev->next = t;
  if(t->next)
    t->next->prev = t;
  if(td->head == old)
    td->head = t;
  if(td->tail == old)
    td->tail = t;
  if(td->current == old)
    td->current = t;

  *_t = t;
  return true;
}

static boolean text_set_line(struct text_document *td, struct text_line *t,
 const char *src, size_t src_len)
{
  if(!td)
    return false;

  if(src && t)
  {
    trace("--TEXTDOC-- text_set_line %p . %p (size %zu)\n",
     (void *)td, (void *)t, src_len);

    if(!text_resize_line(td, &t, src_len))
      return false;

    if(src_len)
      memcpy(t->data, src, src_len);
    t->data[src_len] = '\0';
  }
  return true;
}

static boolean text_update_current(struct text_document *td)
{
  return text_set_line(td, td->current, td->edit_buffer, td->edit_buffer_size);
}

static boolean text_set_current(struct text_document *td, struct text_line *t)
{
  struct text_line *current;
  if(!td || !t)
    return false;

  current = td->current;
  if(t == current)
    return true;

  if(current && !text_update_current(td))
    return false;

  // Switch to a new line.
  trace("--TEXTDOC-- text_set_current %p . %p\n", (void *)td, (void *)t);

  td->current = t;
  text_set_edit_buffer(td, t->size);
  memcpy(td->edit_buffer, t->data, t->size);
  td->edit_buffer[t->size] = '\0';
  td->edit_buffer_size = t->size;
  return true;
}

static struct text_line *text_insert_line(struct text_document *td,
 struct text_line *at, int init_size, int direction)
{
  struct text_line *prev;
  struct text_line *t;

  if(!td || direction == 0)
    return NULL;

  if(direction < 0 && at)
  {
    prev = at->prev;
    td->current_line++;
  }
  else
    prev = at;

  t = ccalloc(1, sizeof(struct text_line) + init_size + 1);
  t->size = init_size;

  trace("--TEXTDOC-- text_insert_line %p . %p (size %d)\n",
   (void *)td, (void *)t, init_size);
  if(prev)
  {
    if(prev->next)
    {
      t->next = prev->next;
      t->next->prev = t;
    }
    else
      td->tail = t;

    prev->next = t;
    t->prev = prev;
  }
  else
  {
    if(td->head)
      td->head->prev = t;
    else
      td->tail = t;
    t->next = td->head;
    td->head = t;
  }
  td->num_lines++;
  return t;
}

static boolean text_delete_line(struct text_document *td, struct text_line *t)
{
  struct text_line *prev;
  struct text_line *next;
  if(!t)
    return false;

  trace("--TEXTDOC-- text_delete_line %p . %p (size %d)\n",
   (void *)td, (void *)t, t->size);

  prev = t->prev;
  next = t->next;
  free(t);

  if(prev)
    prev->next = next;
  if(next)
    next->prev = prev;
  if(t == td->head)
    td->head = next;
  if(t == td->tail)
    td->tail = prev;

  td->num_lines--;
  return true;
}

static boolean text_split_current(struct text_document *td, int pos)
{
  struct text_line *next;
  size_t next_size;
  if(!td->edit_buffer)
    return false;

  trace("--TEXTDOC-- text_split_current %p (size %d) at pos %d\n",
   (void *)td, td->edit_buffer_size, pos);

  if(pos == 0)
  {
    // If pos is 0, just insert a blank line before the current...
    text_insert_line(td, td->current, 0, -1);
    return true;
  }

  next_size = td->edit_buffer_size - pos;
  next = text_insert_line(td, td->current, next_size, 1);
  if(pos < td->edit_buffer_size)
  {
    memcpy(next->data, td->edit_buffer + pos, next_size);
    next->data[next_size] = '\0';
    td->edit_buffer[pos] = '\0';
    td->edit_buffer_size = pos;
  }
  return true;
}

static boolean text_join_current(struct text_document *td, int direction)
{
  struct text_line *t = td->current;
  struct text_line *next;

  if(!t || !direction || (direction < 0 && !t->prev) || (direction > 0 && !t->next))
    return false;

  trace("--TEXTDOC-- text_join_current %p (direction: %d)\n",
   (void *)td, direction);

  if(direction < 0)
  {
    if(t->prev && t->prev->size == 0)
    {
      // Previous line is blank? Just delete it...
      if(!text_delete_line(td, t->prev))
        return false;

      td->current_line--;
      return true;
    }

    if(!text_set_current(td, t->prev))
      return false;

    td->current_line--;
    t = t->prev;
  }

  next = t->next;
  if(next->size)
  {
    text_set_edit_buffer(td, td->edit_buffer_size + next->size);

    memcpy(td->edit_buffer + td->edit_buffer_size, next->data, next->size);
    td->edit_buffer_size += next->size;
    td->edit_buffer[td->edit_buffer_size] = '\0';
  }

  text_delete_line(td, next);
  return true;
}

static int text_insert_current(struct text_document *td, int insert_pos,
 const char *src, size_t src_len, char linebreak_char)
{
  struct text_line *t;
  char *tmp = NULL;
  size_t tmp_size = 0;
  size_t linestart;
  size_t i;
  int col;
  if(!td || !src || insert_pos < 0)
    return -1;

  trace("--TEXTDOC-- text_insert_current pos:%d, size:%zu, linebreak:%d\n",
   insert_pos, src_len, linebreak_char);

  if(src_len == 0)
    return insert_pos;

  if(insert_pos < td->edit_buffer_size)
  {
    tmp_size = td->edit_buffer_size - insert_pos;
    tmp = cmalloc(tmp_size);
    memcpy(tmp, td->edit_buffer + insert_pos, tmp_size);
    td->edit_buffer_size = insert_pos;
  }

  for(i = 0, linestart = 0; i < src_len; i++)
  {
    if(src[i] == linebreak_char)
    {
      size_t len = i - linestart;
      if(linebreak_char == '\n' && len > 0 && src[i - 1] == '\r')
        len--;

      t = td->current;
      if(len)
      {
        size_t pos = td->edit_buffer_size;
        text_set_edit_buffer(td, td->edit_buffer_size + len);
        memcpy(td->edit_buffer + pos, src + linestart, len);
        td->edit_buffer_size += len;
      }
      t = text_insert_line(td, t, 0, 1);
      text_set_current(td, t);
      td->current_line++;
      linestart = i + 1;
      continue;
    }
  }

  col = td->edit_buffer_size;
  if(i > linestart || tmp_size)
  {
    size_t new_size = td->edit_buffer_size + tmp_size;
    size_t pos = td->edit_buffer_size;
    t = td->current;

    if(i > linestart)
      new_size += i - linestart;

    text_set_edit_buffer(td, new_size);
    if(i > linestart)
    {
      memcpy(td->edit_buffer + pos, src + linestart, i - linestart);
      pos += i - linestart;
      col = pos;
    }
    if(tmp_size)
    {
      memcpy(td->edit_buffer + pos, tmp, tmp_size);
      pos += tmp_size;
    }
    td->edit_buffer[pos] = '\0';
  }

  text_update_current(td);
  return col;
}

static boolean text_move(struct text_document *td, int amount)
{
  struct text_line *t;
  if(!td)
    return false;

  t = td->current;
  if(amount > 0)
  {
    while(t && t->next && amount)
    {
      t = t->next;
      td->current_line++;
      amount--;
    }
  }
  else

  if(amount < 0)
  {
    while(t && t->prev && amount)
    {
      t = t->prev;
      td->current_line--;
      amount++;
    }
  }
  if(!text_set_current(td, t))
    return false;
  return true;
}

static boolean text_move_start(struct text_document *td)
{
  if(!td || !td->head)
    return false;

  if(!text_set_current(td, td->head))
    return false;

  td->current_line = 0;
  return true;
}

static boolean text_move_end(struct text_document *td)
{
  if(!td || !td->tail)
    return false;

  if(!text_set_current(td, td->tail))
    return false;

  td->current_line = td->num_lines - 1;
  return true;
}

static void text_clear(struct text_document *td)
{
  struct text_line *next = td->head;
  struct text_line *t;

  trace("--TEXTDOC-- text_clear %p (head: %p, tail: %p)\n",
   (void *)td, (void *)td->head, (void *)td->tail);
  while(next)
  {
    t = next;
    next = t->next;
    free(t);
  }
  free(td->edit_buffer);

  td->head = NULL;
  td->tail = NULL;
  td->current = NULL;
  td->edit_buffer = NULL;
  td->edit_buffer_size = 0;
  td->edit_buffer_alloc = 0;
  td->num_lines = 0;
  td->current_line = 0;
}

static boolean text_init(struct text_document *td,
 const char *src, size_t src_len)
{
  const char *pos = src;
  const char *end = src + src_len;
  const char *next;
  struct text_line *t = NULL;
  size_t len;

  trace("--TEXTDOC-- text_init %p (size %zu)\n", (void *)td, src_len);
  if(td->head || td->tail || td->edit_buffer)
    return false;

  while(pos < end)
  {
    next = memchr(pos, '\n', end - pos);
    if(!next)
      next = end;

    len = 0;
    if(next > pos)
    {
      const char *line_end = next;
      if(line_end > pos && line_end[-1] == '\r')
        line_end--;

      len = line_end - pos;
    }

    t = text_insert_line(td, t, len, 1);
    if(!t)
      return false;
    if(len)
      memcpy(t->data, pos, len);

    pos = next + 1;
  }

  // Insert an extra line if there's a terminating EOL.
  if(src[src_len - 1] == '\n')
    text_insert_line(td, t, 0, 1);

  return true;
}

static boolean text_edit_flush(struct text_edit_context *te)
{
  struct text_document *td = &(te->td);
  struct text_line *next;
  struct text_line *t;
  size_t new_len = 0;
  char *dest;
  char *pos;

  trace("--TEXTEDIT-- text_edit_flush\n");
  text_update_current(td);

  /* Precalculate the new size. */
  next = td->head;
  while(next)
  {
    t = next;
    next = t->next;
    new_len += t->size;
  }
  if(te->dos_line_ends)
    new_len += td->num_lines - 1;
  new_len += td->num_lines - 1;

  dest = cmalloc(new_len + 1);
  pos = dest;
  next = td->head;
  while(next)
  {
    t = next;
    next = t->next;
    if(t->size)
      memcpy(pos, t->data, t->size);
    pos += t->size;

    if(next)
    {
      if(te->dos_line_ends)
        *(pos++) = '\r';
      *(pos++) = '\n';
    }
  }
  *pos = '\0';
  te->flush_cb(te->ctx.world, te->priv, dest, new_len);
  free(dest);
  return true;
}

static void text_edit_reset_intake(struct text_edit_context *te)
{
  struct text_document *td = &(te->td);
  if(te->intk)
  {
    destroy_context(te->intk);
    te->intk = NULL;
  }

  te->intk =
   intake2((context *)te, td->edit_buffer, td->edit_buffer_alloc,
   te->edit_x, te->edit_y + te->intk_row, te->edit_w,
   te->text_highlight, &(te->current_col), &(td->edit_buffer_size));
}

/**
 * Text edit context draw function.
 */
static boolean text_edit_draw(context *ctx)
{
  struct text_edit_context *te = (struct text_edit_context *)ctx;
  struct text_document *td = &(te->td);
  struct text_line *t;
  int line_col = te->text_color;
  int active_col = te->text_highlight;
  int top_col = te->border_color;
  int stat_col = te->border_highlight;
  int bottom_y = te->y + te->h - 1;
  int left_chr;
  int right_chr;
  int line;
  int i;

  switch(te->type)
  {
    case TE_BOX:
      fill_line(te->w, te->x, te->y, 0, top_col);
      fill_line(te->w, te->x, bottom_y, 0, top_col);

      for(i = 0; i < te->edit_h; i++)
      {
        draw_char(179, line_col, te->x, te->edit_y + i);
        draw_char(179, line_col, te->x + te->w - 1, te->edit_y + i);
        fill_line(te->w - 2, te->x + 1, te->edit_y + i, 0, line_col);
      }
      left_chr = (te->current_col >= te->edit_w) ? '\xae' : '\x10';
      right_chr = (te->current_col < td->edit_buffer_size && td->edit_buffer_size >= te->edit_w) ? '\xaf' : '\x11';

      draw_char(left_chr, active_col, te->x, te->edit_y + te->intk_row);
      draw_char(right_chr, active_col, te->x + te->w - 1, te->edit_y + te->intk_row);

      if(te->title)
        write_string(te->title, te->x + 2, te->y, top_col, false);

      write_string("Line:", te->x + 2, bottom_y, top_col, false);
      draw_char('/', top_col, te->x + 14, bottom_y);
      write_number(td->current_line + 1, stat_col, te->x + 8, bottom_y, 6, false, 10);
      write_number(td->num_lines, stat_col, te->x + 15, bottom_y, 6, false, 10);

      write_string("Col:", te->x + 23, bottom_y, top_col, false);
      draw_char('/', top_col, te->x + 35, bottom_y);
      write_number(te->current_col + 1, stat_col, te->x + 28, bottom_y, 7, false, 10);
      write_number(td->edit_buffer_size + 1, stat_col, te->x + 36, bottom_y, 7, false, 10);
      break;

    case TE_DIALOG:
      break;
  }

  if(!td->current)
    return true;

  // Lines above.
  t = td->current->prev;
  for(line = te->intk_row - 1; t && line >= 0; line--)
  {
    for(i = 0; i < t->size && i < te->edit_w; i++)
      draw_char(t->data[i], line_col, te->edit_x + i, te->edit_y + line);

    t = t->prev;
  }

  // Lines below.
  t = td->current->next;
  for(line = te->intk_row + 1; t && line < te->edit_h; line++)
  {
    for(i = 0; i < t->size && i < te->edit_w; i++)
      draw_char(t->data[i], line_col, te->edit_x + i, te->edit_y + line);

    t = t->next;
  }
  return true;
}

/**
 * Text edit context idle function.
 */
static boolean text_edit_idle(context *ctx)
{
  struct text_edit_context *te = (struct text_edit_context *)ctx;
  struct text_document *td = &(te->td);
  size_t old_size = td->edit_buffer_alloc;

  // Make sure the edit buffer is sized to fit reasonable text input.
  text_set_edit_buffer(td, td->edit_buffer_size + KEY_UNICODE_MAX);
  if(td->edit_buffer_alloc != old_size)
    text_edit_reset_intake(te);

  return false;
}

/**
 * Text edit context mouse click function.
 */
static boolean text_edit_mouse(context *ctx, int *key, int button, int x, int y)
{
  struct text_edit_context *te = (struct text_edit_context *)ctx;
  struct text_document *td = &(te->td);

  if(button && button <= MOUSE_BUTTON_RIGHT)
  {
    if(x >= te->edit_x && x < te->edit_x + te->edit_w &&
     y >= te->edit_y && y < te->edit_y + te->edit_h)
    {
      // Jump to clicked line.
      int amount = y - (te->edit_y + te->intk_row);
      if(amount)
      {
        text_move(td, amount);
        text_edit_reset_intake(te);
        te->current_col = x - te->edit_x;
        warp_mouse(x, te->edit_y + te->intk_row);
        return true;
      }
    }
  }
  else

  if(button == MOUSE_BUTTON_WHEELUP)
  {
    text_move(td, -3);
    text_edit_reset_intake(te);
    return true;
  }
  else

  if(button == MOUSE_BUTTON_WHEELDOWN)
  {
    text_move(td, 3);
    text_edit_reset_intake(te);
    return true;
  }

  return false;
}

/**
 * Text edit context joystick action function.
 */
static boolean text_edit_joystick(context *ctx, int *key, int action)
{
  struct text_edit_context *te = (struct text_edit_context *)ctx;
  struct text_document *td = &(te->td);

  // Note: intake context also handles X, Y.
  switch(action)
  {
    case JOY_Y:
    {
      // Backspace: join previous if at the start of the current line.
      if(te->current_col == 0)
      {
        if(td->current && td->current->prev)
          te->current_col = td->current->prev->size;

        text_join_current(td, -1);
        text_edit_reset_intake(te);
        return true;
      }
      break;
    }

    default:
    {
      enum keycode ui_key = get_joystick_ui_key();
      if(ui_key)
      {
        *key = ui_key;
        return true;
      }
      break;
    }
  }
  return false;
}

/**
 * Text edit context key function.
 */
static boolean text_edit_key(context *ctx, int *key)
{
  struct text_edit_context *te = (struct text_edit_context *)ctx;
  struct text_document *td = &(te->td);

  if(get_exit_status())
    *key = IKEY_ESCAPE;

  switch(*key)
  {
    case IKEY_ESCAPE:
    {
      // Exit.
      destroy_context(ctx);
      return true;
    }

    case IKEY_RETURN:
    {
      // Split line.
      if(text_split_current(td, te->current_col))
      {
        // Move to the start of the new line unless already at the start.
        if(te->current_col)
        {
          te->current_col = 0;
          text_move(td, 1);
          text_edit_reset_intake(te);
        }
      }
      return true;
    }

    case IKEY_BACKSPACE:
    {
      // Let intake handle Alt+Backspace and Ctrl+Backspace.
      if(get_alt_status(keycode_internal) || get_ctrl_status(keycode_internal))
        break;

      // Join previous line if at the start of the current line.
      if(te->current_col == 0)
      {
        if(td->current && td->current->prev)
          te->current_col = td->current->prev->size;

        text_join_current(td, -1);
        text_edit_reset_intake(te);
        return true;
      }
      break;
    }

    case IKEY_DELETE:
    {
      // Join next line if at the end of the current line.
      if(te->current_col == td->edit_buffer_size)
      {
        text_join_current(td, 1);
        text_edit_reset_intake(te);
        return true;
      }
      break;
    }

    case IKEY_UP:
    {
      // Cursor up.
      text_move(td, -1);
      text_edit_reset_intake(te);
      return true;
    }

    case IKEY_DOWN:
    {
      // Cursor down.
      text_move(td, 1);
      text_edit_reset_intake(te);
      return true;
    }

    case IKEY_PAGEUP:
    {
      // Cursor up * half edit height.
      text_move(td, -te->edit_h / 2);
      text_edit_reset_intake(te);
      return true;
    }

    case IKEY_PAGEDOWN:
    {
      // Cursor down * half edit height.
      text_move(td, te->edit_h / 2);
      text_edit_reset_intake(te);
      return true;
    }

    case IKEY_HOME:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Cursor to start of text document.
        text_move_start(td);
        text_edit_reset_intake(te);
        return true;
      }
      break;
    }

    case IKEY_END:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Cursor to end of text document.
        text_move_end(td);
        text_edit_reset_intake(te);
        return true;
      }
      break;
    }

    case IKEY_INSERT:
    case IKEY_p:
    {
      if(get_alt_status(keycode_internal))
      {
        // Paste clipboard.
        char *clipboard_text = get_clipboard_buffer();
        if(clipboard_text)
        {
          int new_col = text_insert_current(td, te->current_col,
           clipboard_text, strlen(clipboard_text), '\n');
          free(clipboard_text);

          if(new_col >= 0)
            te->current_col = new_col;
          text_edit_reset_intake(te);
        }
        return true;
      }
      break;
    }
  }
  return false;
}

/**
 * Text edit context destroy function.
 */
static void text_edit_destroy(context *ctx)
{
  struct text_edit_context *te = (struct text_edit_context *)ctx;
  text_edit_flush(te);
  text_clear(&te->td);
  free(te->title);
  restore_screen();
}

/**
 * Create a text editor context at a given location for a given input string.
 * A callback and callback data must be provided to take the output data from
 * this context.
 *
 * @param  x          x position of edit box (or edit area for `TE_DIALOG`).
 * @param  y          y position of edit box (or edit area for `TE_DIALOG`).
 * @param  w          width of edit box (or edit area for `TE_DIALOG`).
 * @param  h          height of edit box (or edit area for `TE_DIALOG`).
 * @param  type       `TE_BOX` for standalone editor, `TE_DIALOG` for embedded.
 * @param  title      title of the editor box (ignore for `TE_DIALOG`).
 * @param  src        input text to edit.
 * @param  src_len    size of input text.
 * @param  p          private data for flush_cb.
 * @param  flush_cb   callback to set the destination buffer.
 */
void text_editor(context *parent, int x, int y, int w, int h, int type,
 const char *title, const char *src, size_t src_len, void *p,
 void (*flush_cb)(struct world *, void *, char *, size_t))
{
  struct text_edit_context *te = ccalloc(1, sizeof(struct text_edit_context));
  struct text_document *td = &te->td;
  struct context_spec spec;

  te->x = x;
  te->y = y;
  te->w = w;
  te->h = h;
  te->type = type;
  te->priv = p;
  te->flush_cb = flush_cb;

  if(type == TE_BOX)
  {
    te->edit_x = x + 2;
    te->edit_y = y + 1;
    te->edit_w = w - 4;
    te->edit_h = h - 2;
    te->text_color = 0x87;
    te->text_highlight = 0x8b;
    te->border_color = 0x4e;
    te->border_highlight = 0x4f;
    if(title)
    {
      size_t title_len = strlen(title);
      te->title = cmalloc(title_len + 1);
      memcpy(te->title, title, title_len + 1);
    }
  }
  else
  {
    te->edit_x = te->x;
    te->edit_y = te->y;
    te->edit_w = te->w;
    te->edit_h = te->h;
    te->text_color = 0x19;
    te->text_highlight = 0x9f;
  }

  te->intk_row = te->edit_h / 2;

  text_init(td, src, src_len);
  if(td->num_lines < 1)
    text_insert_line(td, NULL, 0, 1);
  text_set_current(td, td->head);

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw           = text_edit_draw;
  spec.idle           = text_edit_idle;
  spec.click          = text_edit_mouse;
  spec.joystick       = text_edit_joystick;
  spec.key            = text_edit_key;
  spec.destroy        = text_edit_destroy;
  create_context((context *)te, parent, &spec, CTX_TEXT_EDITOR);

  /* Create a new intake subcontext for the current edit buffer. */
  text_edit_reset_intake(te);

  save_screen();
}

void hex_editor(context *parent, int x, int y, int w, int h,
 const char *title, const void *src, size_t src_len, void *p,
 void (*flush_cb)(struct world *, void *, char *, size_t))
{
  // FIXME
}
