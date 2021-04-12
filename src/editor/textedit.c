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
#include "configure.h"
#include "text.h"
#include "undo.h"

#include "../event.h"
#include "../graphics.h"
#include "../intake.h"
#include "../util.h"
#include "../window.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_IDLE_TIMER_MAX 15

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
  int intk_row;
  int text_color;
  int text_highlight;
  int border_color;
  int border_highlight;
  boolean dos_line_ends;
  void *priv;
  void (*flush_cb)(struct world *mzx_world, void *priv, char *src, size_t src_len);

  /* Undo history and related variables. */
  struct undo_history *u;
  enum intake_event_type current_frame_type;
  int idle_timer;
};

static boolean text_edit_flush(struct text_edit_context *te)
{
  struct text_document *td = &(te->td);
  size_t new_len = 0;
  char *dest;
  char *pos;

  trace("--TEXTEDIT-- text_edit_flush\n");

  /* Precalculate the new size. */
  // FIXME

  dest = (char *)cmalloc(new_len + 1);
  pos = dest;
  // FIXME
  *pos = '\0';
  te->flush_cb(te->ctx.world, te->priv, dest, new_len);
  free(dest);
  return true;
}

/* End the current undo frame, if any. */
static void text_edit_end_frame(struct text_edit_context *te)
{
  if(te->current_frame_type != INTK_NO_EVENT)
  {
    trace("--TEXTEDIT-- text_edit_end_frame (type=%d)\n", te->current_frame_type);
    update_undo_frame(te->u);
    te->current_frame_type = INTK_NO_EVENT;
    te->idle_timer = 0;
  }
}

/* Start a new undo frame from the current edit buffer. */
static void text_edit_start_frame(struct text_edit_context *te)
{
  struct text_document *td = &(te->td);
  text_edit_end_frame(te);
  // FIXME
  /*
  add_text_editor_undo_frame(te->u, td);
  add_text_editor_undo_line(te->u, TX_OLD_LINE, td->current_line,
   td->current_col, td->edit_buffer, td->edit_buffer_size);
  */
}

static boolean text_edit_intake_callback(void *priv, subcontext *sub,
 enum intake_event_type type, int old_pos, int new_pos, int value, const char *data)
{
  struct text_edit_context *te = (struct text_edit_context *)priv;
  struct text_document *td = &(te->td);

  if(te->current_frame_type != type)
    text_edit_end_frame(te);

  switch(type)
  {
    case INTK_NO_EVENT:
    case INTK_MOVE:
    case INTK_MOVE_WORDS:
      intake_apply_event_fixed(sub, type, new_pos, value, data);
      break;
    case INTK_INSERT:
    case INTK_OVERWRITE:
    case INTK_DELETE:
    case INTK_BACKSPACE:
    case INTK_BACKSPACE_WORDS:
    case INTK_CLEAR:
    case INTK_INSERT_BLOCK:
    case INTK_OVERWRITE_BLOCK:
      if(te->current_frame_type != type)
      {
        te->current_frame_type = type;
        // FIXME
        /*
        add_text_editor_undo_frame(te->u, td);
        add_text_editor_undo_line(te->u, TX_OLD_LINE, td->current_line,
         old_pos, td->current->data, td->current->size);
        */
      }
      intake_apply_event_fixed(sub, type, new_pos, value, data);
      /*
      add_text_editor_undo_line(te->u, TX_SAME_LINE, td->current_line,
       new_pos, td->edit_buffer, td->edit_buffer_size);
      */

      te->idle_timer = TEXT_IDLE_TIMER_MAX;
      break;
  }
  return true;
}

/**
 * Text edit context draw function.
 */
static boolean text_edit_draw(context *ctx)
{
  struct text_edit_context *te = (struct text_edit_context *)ctx;
  struct text_document *td = &(te->td);
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
      left_chr = (td->current_col >= te->edit_w) ? '\xae' : '\x10';
      right_chr = (td->current_col < td->edit_buffer_size && td->edit_buffer_size >= te->edit_w) ? '\xaf' : '\x11';

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
      write_number(td->current_col + 1, stat_col, te->x + 28, bottom_y, 7, false, 10);
      write_number(td->edit_buffer_size + 1, stat_col, te->x + 36, bottom_y, 7, false, 10);
      break;

    case TE_DIALOG:
      break;
  }

  // FIXME
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

  // End the current undo frame after a period of no input.
  if(te->idle_timer > 0)
  {
    te->idle_timer--;
    if(!te->idle_timer)
      text_edit_end_frame(te);
  }
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
        text_edit_end_frame(te);
        text_move(td, amount);
        td->current_col = x - te->edit_x;
        warp_mouse(x, te->edit_y + te->intk_row);
        return true;
      }
    }
  }
  else

  if(button == MOUSE_BUTTON_WHEELUP)
  {
    text_edit_end_frame(te);
    text_move(td, -3);
    return true;
  }
  else

  if(button == MOUSE_BUTTON_WHEELDOWN)
  {
    text_edit_end_frame(te);
    text_move(td, 3);
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
      if(td->current_col == 0)
      {
        *key = IKEY_BACKSPACE;
        return true;
      }
      break;
    }

    default:
    {
      Uint32 ui_key = get_joystick_ui_key();
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
      // FIXME
      /*
      if(td->current)
      {
        text_edit_start_frame(te);

        // Split, then move to the start of the second line.
        text_split_current(td);
        if(td->current_col)
        {
          td->current_col = 0;
          text_move(td, 1);
        }

        add_text_editor_undo_line(te->u, TX_NEW_LINE, td->current_line - 1, 0,
         td->current->prev->data, td->current->prev->size);
        add_text_editor_undo_line(te->u, TX_NEW_LINE, td->current_line, 0,
         td->current->data, td->current->size);
        update_undo_frame(te->u);
      }
      */
      return true;
    }

    case IKEY_UP:
    {
      // Cursor up.
      text_edit_end_frame(te);
      text_move(td, -1);
      return true;
    }

    case IKEY_DOWN:
    {
      // Cursor down.
      text_edit_end_frame(te);
      text_move(td, 1);
      return true;
    }

    case IKEY_PAGEUP:
    {
      // Cursor up * half edit height.
      text_edit_end_frame(te);
      text_move(td, -te->edit_h / 2);
      return true;
    }

    case IKEY_PAGEDOWN:
    {
      // Cursor down * half edit height.
      text_edit_end_frame(te);
      text_move(td, te->edit_h / 2);
      return true;
    }

    case IKEY_HOME:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Cursor to start of text document.
        text_edit_end_frame(te);
        text_move_start(td);
        return true;
      }
      break;
    }

    case IKEY_END:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Cursor to end of text document.
        text_edit_end_frame(te);
        text_move_end(td);
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
          // FIXME
          //text_edit_start_frame(te);
          //text_insert_current(td, clipboard_text, strlen(clipboard_text), '\n');
          free(clipboard_text);
        }
        return true;
      }
      break;
    }

    case IKEY_z:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Undo.
        text_edit_end_frame(te);
        apply_undo(te->u);
        return true;
      }
      break;
    }

    case IKEY_y:
    {
      if(get_ctrl_status(keycode_internal))
      {
        // Redo.
        text_edit_end_frame(te);
        apply_redo(te->u);
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
  // FIXME
  //destruct_undo_history(te->u);
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
void text_editor(context *parent, int x, int y, int w, int h,
 enum text_edit_type type, const char *title, const char *src, size_t src_len,
 void *p, void (*flush_cb)(struct world *, void *, char *, size_t))
{
  struct text_edit_context *te =
   (struct text_edit_context *)ccalloc(1, sizeof(struct text_edit_context));
  struct editor_config_info *editor_conf = get_editor_config();
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
      te->title = (char *)cmalloc(title_len + 1);
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

  te->current_frame_type = INTK_NO_EVENT;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw           = text_edit_draw;
  spec.idle           = text_edit_idle;
  spec.click          = text_edit_mouse;
  spec.joystick       = text_edit_joystick;
  spec.key            = text_edit_key;
  spec.destroy        = text_edit_destroy;
  create_context((context *)te, parent, &spec, CTX_TEXT_EDITOR);

  /* Create a new intake subcontext for the current edit buffer. */

  // FIXME
  //te->u = construct_text_editor_undo_history(editor_conf->undo_history_size);

  te->intk =
   intake2((context *)te, td->edit_buffer, td->edit_buffer_alloc,
   &(td->current_col), &(td->edit_buffer_size));

  intake_event_callback(te->intk, te, text_edit_intake_callback);

  save_screen();
}

void hex_editor(context *parent, int x, int y, int w, int h,
 const char *title, const void *src, size_t src_len, void *p,
 void (*flush_cb)(struct world *, void *, char *, size_t))
{
  // FIXME
}
