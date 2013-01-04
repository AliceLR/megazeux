/* MegaZeux
 *
 * Copyright (C) 1996 Greg Janson
 * Copyright (C) 2004 Gilead Kutnick - exophase@adelphia.net
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

#include "char_ed.h"

#include "../helpsys.h"
#include "../window.h"
#include "../graphics.h"
#include "../data.h"
#include "../event.h"
#include "../world.h"

#include "graphics.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

//----------------------------------------------
//  +----------------+ Current char-     (#000)
//  |................|    ..........0..........
//  |................| %%      Move cursor
//  |................| -+      Change char
//  |................| Space   Toggle pixel
//  |................| Enter   Select char
//  |................| Del     Clear char
//  |................| N       'Negative'
//  |................| Alt+%%  Shift char
//  |................| M       'Mirror'
//  |................| F       'Flip'
//  |................| F2      Copy to buffer
//  |................| F3      Copy from buffer
//  |................| Tab     Draw (off)
//  |................| F4      Revert to ASCII
//  +----------------+ F5      Revert to MZX
//----------------------------------------------

//Mouse- Menu=Command
//       Grid=Toggle
//       "Current Char-"=Enter
//       "..0.."=Change char

static char char_copy_buffer[8 * 14 * 32];
static int char_copy_width = 8;
static int char_copy_height = 14;
static char current_char = 1;
static int current_width = 1;
static int current_height = 1;
static int highlight_width = 1;
static int highlight_height = 1;
static int num_files = 1;

static int char_copy_use_offset = 0;

static Uint8 **history = NULL;
static int history_size = 0;
static int history_pos = 0;

static void fill_region(char *buffer, int x, int y,
 int buffer_width, int buffer_height, int check, int draw)
{
  int offset = x + (y * buffer_width);
  if((x >= 0) && (x < buffer_width) &&
   (y >= 0) && (y < buffer_height) &&
   (buffer[offset] == check))
  {
    buffer[offset] = draw;

    fill_region(buffer, x - 1, y, buffer_width,
     buffer_height, check, draw);
    fill_region(buffer, x + 1, y, buffer_width,
     buffer_height, check, draw);
    fill_region(buffer, x, y - 1, buffer_width,
     buffer_height, check, draw);
    fill_region(buffer, x, y + 1, buffer_width,
     buffer_height, check, draw);
  }
}

static void expand_buffer(char *buffer, int width, int height,
 int cwidth, int cheight, int offset, int smzx)
{
  char char_buffer[32 * 14];
  char *buffer_ptr = buffer;
  char *cbuffer_ptr = char_buffer;
  int current_char = offset;
  int cx, cy, x, y, i, shift;
  int cskip = 32 - cwidth;
  char *last_ptr, *last_ptr2;

  for(cy = 0; cy < cheight; cy++, current_char += cskip)
  {
    for(cx = 0; cx < cwidth; cx++, current_char++,
     cbuffer_ptr += 14)
    {
      ec_read_char(current_char, cbuffer_ptr);
    }
  }

  if(smzx)
  {
    int skip = (width - 1) * 4;
    int skip2 = width * 4 * 14;

    for(cbuffer_ptr = char_buffer, y = 0; y < height; y++)
    {
      last_ptr = buffer_ptr;
      for(x = 0; x < width; x++)
      {
        last_ptr2 = buffer_ptr;
        for(i = 0; i < 14; i++, cbuffer_ptr++,
         buffer_ptr += skip)
        {
          for(shift = 6; shift >= 0; shift -= 2, buffer_ptr++)
          {
            *buffer_ptr = ((*cbuffer_ptr) >> shift) & 3;
          }
        }
        buffer_ptr = last_ptr2 + 4;
      }

      buffer_ptr = last_ptr + skip2;
    }
  }
  else
  {
    int skip = (width - 1) * 8;
    int skip2 = width * 8 * 14;

    for(cbuffer_ptr = char_buffer, y = 0; y < height; y++)
    {
      last_ptr = buffer_ptr;
      for(x = 0; x < width; x++)
      {
        last_ptr2 = buffer_ptr;
        for(i = 0; i < 14; i++, cbuffer_ptr++,
         buffer_ptr += skip)
        {
          for(shift = 7; shift >= 0; shift--, buffer_ptr++)
          {
            *buffer_ptr = ((*cbuffer_ptr) >> shift) & 1;
          }
        }
        buffer_ptr = last_ptr2 + 8;
      }

      buffer_ptr = last_ptr + skip2;
    }
  }
}

static void collapse_buffer(char *buffer, int width, int height,
 int cwidth, int cheight, int offset, int smzx)
{
  char char_buffer[32 * 14];
  char *buffer_ptr = buffer;
  char *cbuffer_ptr = char_buffer;
  int current_char = offset;
  int cx, cy, x, y, i, shift;
  int cskip = 32 - cwidth;
  char *last_ptr, *last_ptr2;
  int current_row;

  if(smzx)
  {
    int skip = (width - 1) * 4;
    int skip2 = width * 4 * 14;

    for(y = 0; y < height; y++)
    {
      last_ptr = buffer_ptr;
      for(x = 0; x < width; x++)
      {
        last_ptr2 = buffer_ptr;
        for(i = 0; i < 14; i++, cbuffer_ptr++,
         buffer_ptr += skip)
        {
          current_row = 0;
          for(shift = 6; shift >= 0; shift -= 2, buffer_ptr++)
          {
            current_row |= (*buffer_ptr) << shift;
          }
          *cbuffer_ptr = current_row;
        }
        buffer_ptr = last_ptr2 + 4;
      }

      buffer_ptr = last_ptr + skip2;
    }
  }
  else
  {
    int skip = (width - 1) * 8;
    int skip2 = width * 8 * 14;

    for(y = 0; y < height; y++)
    {
      last_ptr = buffer_ptr;
      for(x = 0; x < width; x++)
      {
        last_ptr2 = buffer_ptr;
        for(i = 0; i < 14; i++, cbuffer_ptr++,
         buffer_ptr += skip)
        {
          current_row = 0;
          for(shift = 7; shift >= 0; shift--, buffer_ptr++)
          {
            current_row |= (*buffer_ptr) << shift;
          }
          *cbuffer_ptr = current_row;
        }
        buffer_ptr = last_ptr2 + 8;
      }

      buffer_ptr = last_ptr + skip2;
    }
  }

  for(cbuffer_ptr = char_buffer, cy = 0; cy < cheight;
   cy++, current_char += cskip)
  {
    for(cx = 0; cx < cwidth; cx++, current_char++,
     cbuffer_ptr += 14)
    {
      ec_change_char(current_char, cbuffer_ptr);
    }
  }
}

static void draw_multichar(char *buffer, int start_x, int start_y,
 int width, int height, int current_x, int current_y,
 int highlight_width, int highlight_height)
{
  char *buffer_ptr = buffer;
  int offset = start_x + (start_y * 80);
  int buffer_width = width * 8;
  int x, y;

  if((height == 1) && (width <= 3))
  {
    int skip = 80 - (buffer_width * 2);
    char chars[2] = { 250, 219 };

    for(y = 0; y < 14; y++, offset += skip)
    {
      for(x = 0; x < (width * 8); x++, offset += 2, buffer_ptr++)
      {
        draw_char_linear(135, chars[(int)(*buffer_ptr)], offset);
        draw_char_linear(135, chars[(int)(*buffer_ptr)], offset + 1);
      }
    }

    for(y = 0; y < highlight_height; y++)
    {
      color_line(highlight_width * 2, start_x + (current_x * 2),
       start_y + current_y + y, 0x1B);
    }
  }
  else
  {
    char half_chars[4] = { 32, 223, 220, 219 };
    int skip = 80 - buffer_width;
    int skip2 = buffer_width;
    int current_char;

    for(y = 0; y < (height * 7); y++, offset += skip,
     buffer_ptr += skip2)
    {
      for(x = 0; x < skip2; x++, offset++, buffer_ptr++)
      {
        current_char = (*buffer_ptr) |
         (*(buffer_ptr + skip2) << 1);
        draw_char_linear(135, half_chars[current_char], offset);
      }
    }

    // Start at the top
    y = highlight_height;
    if(current_y & 1)
    {
      char colors[4] = { 0x81, 0x71, 0x8B, 0x7B };
      buffer_ptr = buffer + ((current_y - 1) * buffer_width)
       + current_x;
      for(x = 0; x < highlight_width; x++, offset++, buffer_ptr++)
      {
        current_char =
         *(buffer_ptr) | (*(buffer_ptr + buffer_width) << 1);
        draw_char(half_chars[2], colors[current_char], start_x +
         x + current_x, start_y + (current_y / 2));
      }
      current_y++;
      y--;
    }

    while(y > 1)
    {
      buffer_ptr = buffer + (current_y * buffer_width) + current_x;
      color_line(highlight_width, start_x + current_x,
       start_y + (current_y / 2), 0x1B);
      current_y += 2;
      y -= 2;
    }

    if(y)
    {
      char colors[4] = { 0x81, 0x8B, 0x71, 0x7B };
      buffer_ptr = buffer + (current_y * buffer_width) + current_x;
      for(x = 0; x < highlight_width; x++, offset++, buffer_ptr++)
      {
        current_char =
         *(buffer_ptr) | (*(buffer_ptr + buffer_width) << 1);
        draw_char(half_chars[1], colors[current_char], start_x +
         x + current_x, start_y + (current_y / 2));
      }
    }
  }
}

static void draw_multichar_smzx(char *buffer, int start_x, int start_y,
 int width, int height, int current_x, int current_y,
 int highlight_width, int highlight_height)
{
  char *buffer_ptr = buffer;
  char current_color;
  char base_colors[4] = { 0x8, 0x2, 0x3, 0x7 };
  char highlight_colors[4] = { 0x1, 0x4, 0x5, 0xB };
  int offset = start_x + (start_y * 80);
  int buffer_width = width * 4;

  int x, y;

  if((height == 1) && (width <= 3))
  {
    int skip = 80 - (buffer_width * 4);
    int skip2 = buffer_width - highlight_width;

    int current_pixel;
    for(y = 0; y < 14; y++, offset += skip)
    {
      for(x = 0; x < buffer_width; x++, buffer_ptr++)
      {
        current_pixel = *buffer_ptr;

        if(current_pixel)
        {
          write_string("\xDB\xDB\xDB\xDB", start_x + (x * 4),
           start_y + y, base_colors[current_pixel], 1);
        }
        else
        {
          write_string("\xFA\xFA\xFA\xFA", start_x + (x * 4),
           start_y + y, 0x87, 1);
        }
      }
    }

    buffer_ptr = buffer + current_x + ((current_y) * buffer_width);

    for(y = 0; y < highlight_height; y++,
     buffer_ptr += skip2)
    {
      for(x = 0; x < highlight_width; x++, buffer_ptr++)
      {
        current_pixel = *buffer_ptr;

        if(current_pixel)
        {
          write_string("\xDB\xDB\xDB\xDB", start_x +
           ((x + current_x) * 4), start_y + current_y + y,
           highlight_colors[current_pixel], 1);
        }
        else
        {
          write_string("\xFA\xFA\xFA\xFA", start_x +
           ((x + current_x) * 4), start_y + y + current_y,
           0x1B, 1);
        }
      }
    }
  }
  else
  {
    int skip = 80 - (buffer_width * 2);
    int skip2 = buffer_width - highlight_width;
    int skip3 = 80 - (highlight_width * 2);

    for(y = 0; y < (height * 7); y++, offset += skip,
     buffer_ptr += buffer_width)
    {
      for(x = 0; x < buffer_width; x++, offset += 2, buffer_ptr++)
      {
        current_color = (base_colors[(int)(*buffer_ptr)]) |
         ((base_colors[(int)(*(buffer_ptr + buffer_width))]) << 4);
        draw_char_linear(current_color, 223, offset);
        draw_char_linear(current_color, 223, offset + 1);
      }
    }

    offset = start_x + (current_x * 2) +
     ((start_y + (current_y / 2)) * 80);

    // Start at the top
    y = highlight_height;
    if(current_y & 1)
    {
      buffer_ptr = buffer + current_x +
       ((current_y & ~1) * buffer_width);

      for(x = 0; x < highlight_width; x++, offset += 2,
       buffer_ptr++)
      {
        current_color = (base_colors[(int)(*buffer_ptr)]) |
         ((highlight_colors[(int)(*(buffer_ptr + buffer_width))]) << 4);
        draw_char_linear(current_color, 223, offset);
        draw_char_linear(current_color, 223, offset + 1);
      }

      buffer_ptr += (skip2 * 2);
      offset += skip3;
      y--;

      current_y++;
    }

    while(y > 1)
    {
      buffer_ptr = buffer + current_x +
       ((current_y & ~1) * buffer_width);

      for(x = 0; x < highlight_width; x++, offset += 2,
       buffer_ptr++)
      {
        current_color = (highlight_colors[(int)(*buffer_ptr)]) |
         ((highlight_colors[(int)(*(buffer_ptr + buffer_width))]) << 4);
        draw_char_linear(current_color, 223, offset);
        draw_char_linear(current_color, 223, offset + 1);
      }
      buffer_ptr += (skip2 * 2);
      offset += skip3;
      current_y += 2;
      y -= 2;
    }

    if(y)
    {
      buffer_ptr = buffer + current_x +
       ((current_y & ~1) * buffer_width);

      for(x = 0; x < highlight_width; x++, offset += 2,
       buffer_ptr++)
      {
        current_color = (highlight_colors[(int)(*buffer_ptr)]) |
         ((base_colors[(int)(*(buffer_ptr + buffer_width))]) << 4);
        draw_char_linear(current_color, 223, offset);
        draw_char_linear(current_color, 223, offset + 1);
      }
    }
  }
}

static void replace_filenum(char *src, char *dest, int num)
{
  char *src_ptr = src;
  char *dest_ptr = dest;

  while(*src_ptr)
  {
    if(*src_ptr == '#')
    {
      sprintf(dest_ptr, "%d", num);
      dest_ptr += strlen(dest_ptr);
    }
    else
    {
      *dest_ptr = *src_ptr;
      dest_ptr++;
    }

    src_ptr++;
  }

  *dest_ptr = 0;
}


/****************/
/* UNDO HISTORY */
/****************/

// It's nice to have this stuff separated out in functions
// so it can maybe be replaced with something that gobbles
// less memory at a later point.

// We're saving the CURRENT frame here, not the previous.
static void save_history(void)
{
  if(history_size < 2)
    return;

  if(!history)
  {
    int i;
    history = ccalloc(history_size, sizeof(Uint8 *));

    for(i = 0; i < history_size; i++)
      history[i] = NULL;

    // Decrement it so the first save populates pos 0
    history_pos--;
  }

  if(history_pos == (history_size - 1))
  {
    Uint8 **buffer = ccalloc(history_size, sizeof(Uint8 *));

    // We're about to overwrite this version of the char set,
    // so free it.
    free(history[0]);

    memcpy(buffer, history + 1,
     sizeof(Uint8 *) * (history_size - 1));
    buffer[history_size - 1] = NULL;
    memcpy(history, buffer,
     sizeof(Uint8 *) * history_size);
    free(buffer);
  }
  else
    history_pos++;

  // If we have it, we might as well keep it
  if(!history[history_pos])
    history[history_pos] = cmalloc(14 * 256);

  ec_mem_save_set(history[history_pos]);

  // If there's still space on the undo buffer, free the next
  // slot and put a null so we can't redo undone history.
  if(history_pos < (history_size - 1))
  {
    free(history[history_pos + 1]);
    history[history_pos + 1] = NULL;
  }
}

static void restore_history(void)
{
  if(history_size < 2)
    return;

  if(history_pos < 1)
    return;

  history_pos--;

  ec_mem_load_set(history[history_pos]);
}

static void redo_history(void)
{
  if(history_size < 2)
    return;

  if(history_pos == (history_size - 1))
    return;

  // If the next point in history isn't null
  if(history[history_pos + 1])
  {
    history_pos++;

    ec_mem_load_set(history[history_pos]);
  }
}

static void free_history(void)
{
  int i;

  if(history)
  {
    for(i = 0; i < history_size; i++)
      if(history[i])
        free(history[i]);

    free(history);
  }
  history = NULL;
  history_pos = 0;
}

/********************/
/* END UNDO HISTORY */
/********************/



int char_editor(struct world *mzx_world)
{
  int x = 0;
  int y = 0;
  int draw = 0;
  int draw_new;
  int chars_width, chars_height;
  int chars_x, chars_y;
  int info_height;
  int info_x, info_y;
  int buffer_width = current_width * 8;
  int buffer_height = current_height * 14;
  int dialog_width, dialog_height;
  int dialog_x, dialog_y;
  int chars_show_width;
  char *buffer = cmalloc(8 * 14 * 32);
  char mini_buffer[32];
  int last_x = -1, last_y = 0;
  int block_x = 0;
  int block_y = 0;
  int block_start_x = 0;
  int block_start_y = 0;
  int block_width = buffer_width;
  int block_height = buffer_height;
  int block_mode = 0;
  int shifted = 0;
  int offset;
  int i, i2, key;
  int pad_height;
  int small_chars;
  int screen_mode = get_screen_mode();
  int current_pixel = 0;
  char smzx_colors[4] = { 0x8, 0x2, 0x3, 0x7 };
  unsigned char saved_rgb[4][3];

  // Prepare the history
  history_pos = 0;
  history_size = mzx_world->editor_conf.undo_history_size;
  save_history();

  if(screen_mode)
  {
    if(screen_mode != 1)
      set_screen_mode(1);

    for(i = 0; i < 4; i++)
    {
      get_rgb(i + 2, saved_rgb[i], saved_rgb[i] + 1,
       saved_rgb[i] + 2);
    }

    set_rgb(2, 35, 35, 35);
    set_rgb(3, 28, 28, 28);
    set_rgb(4, 14, 42, 56);
    set_rgb(5, 7, 21, 49);
    update_palette();
  }

  set_context(79);

  m_show();
  // Get char
  expand_buffer(buffer, current_width,
   current_height, highlight_width, highlight_height,
   current_char, screen_mode);

  save_screen();

  do
  {
    buffer_width = current_width * 8;
    buffer_height = current_height * 14;

    if((current_height == 1) &&
     (current_width <= 3))
    {
      small_chars = 0;
      set_mouse_mul(8, 14);
      chars_width = buffer_width * 2;
      chars_height = buffer_height;
    }
    else
    {
      small_chars = 1;
      set_mouse_mul(8, 7);
      chars_width = buffer_width;
      chars_height = buffer_height / 2;
    }

    if(screen_mode)
      buffer_width /= 2;

    if(x >= buffer_width)
      x = buffer_width - 1;

    if(y >= buffer_height)
      y = buffer_height - 1;

    dialog_width = chars_width + 4 + 27;
    dialog_height = chars_height + 4;
    pad_height = current_height;
    if(highlight_height > pad_height)
      pad_height = highlight_height;

    info_height = 15 + pad_height;

    if(dialog_height < (info_height + 2))
      dialog_height = info_height + 2;

    if(dialog_width == 79)
      dialog_width++;

    dialog_x = 40 - (dialog_width / 2);
    dialog_y = 12 - (dialog_height / 2);

    chars_x = dialog_x + 3;
    chars_y = dialog_y + (dialog_height / 2) - (chars_height / 2);
    info_x = chars_x + chars_width + 2;
    info_y = dialog_y + (dialog_height / 2) - (info_height / 2);

    chars_show_width = 22 - current_width;

    restore_screen();
    save_screen();
    draw_window_box(dialog_x, dialog_y,
     dialog_x + dialog_width - 1,
     dialog_y + dialog_height - 1, 143, 128, 135, 1, 1);
    // Draw in static elements
    draw_window_box(chars_x - 1, chars_y - 1,
     chars_x + chars_width, chars_y + chars_height,
     128, 143, 135, 0, 0);
    write_string("Current char-\t(#000)",
     info_x, info_y, 143, 1);

    if(screen_mode)
    {
      write_string
      (
        "\x12\x1d\t Move cursor\n"
        "-+\t Change char\n"
        "Space   Place pixel\n"
        "Enter   Select char\n"
        "Del\tClear char\n"
        "N\t  'Negative'\n"
        "Alt+\x12\x1d  Shift char\n"
        "M\t  'Mirror'\n"
        "F\t  'Flip'\n"
        "F2\t Copy to buffer\n"
        "F3\t Copy from buffer\n"
        "Tab\tDraw\n"
        "1-4\tSelect\n"
        "F5\t Revert to SMZX",
         info_x, info_y + pad_height + 1, 143, 1
      );

      if(current_pixel)
      {
        write_string("\xDB\xDB\xDB\xDB", info_x + 16,
         info_y + pad_height + 13, smzx_colors[current_pixel], 1);
      }
      else
      {
        write_string("\xFA\xFA\xFA\xFA", info_x + 16,
         info_y + pad_height + 13, 0x87, 1);
      }
    }
    else
    {
      write_string
      (
        "\x12\x1d\t Move cursor\n"
        "-+\t Change char\n"
        "Space   Toggle pixel\n"
        "Enter   Select char\n"
        "Del\tClear char\n"
        "N\t  'Negative'\n"
        "Alt+\x12\x1d  Shift char\n"
        "M\t  'Mirror'\n"
        "F\t  'Flip'\n"
        "F2\t Copy to buffer\n"
        "F3\t Copy from buffer\n"
        "Tab\tDraw\n"
        "F4\t Revert to ASCII\n"
        "F5\t Revert to MZX",
         info_x, info_y + pad_height + 1, 143, 1
      );
    }

    // Update char
    if(block_mode)
    {
      if(screen_mode)
      {
        draw_multichar_smzx(buffer, chars_x, chars_y,
         current_width, current_height, block_start_x,
         block_start_y, block_width, block_height);
      }
      else
      {
        draw_multichar(buffer, chars_x, chars_y,
         current_width, current_height, block_start_x,
         block_start_y, block_width, block_height);
      }
    }
    else
    {
      if(screen_mode)
      {
        draw_multichar_smzx(buffer, chars_x, chars_y,
         current_width, current_height, x, y, 1, 1);
      }
      else
      {
        draw_multichar(buffer, chars_x, chars_y,
         current_width, current_height, x, y, 1, 1);
      }
    }

    switch(draw)
    {
      case 0:
        write_string("(off)   ", info_x + 14,
         info_y + pad_height + 12, 143, 0);
        break;

      case 1:
        write_string("(set)   ", info_x + 14,
         info_y + pad_height + 12, 143, 0);
        break;

      case 2:
        write_string("(clear) ", info_x + 14,
         info_y + pad_height + 12, 143, 0);
        break;
    }

    // Current character
    write_number(current_char, 143, info_x + 20, info_y, 3, 0, 10);

    for(i = 0, offset = 0; i < highlight_height; i++)
    {
      for(i2 = 0; i2 < highlight_width; i2++, offset++)
      {
        mini_buffer[offset] = current_char + i2 + (i * 32);
      }
    }

    for(i = 0, offset = 0; i < current_height; i++)
    {
      for(i2 = 0; i2 < current_width; i2++, offset++)
      {
        draw_char_ext(mini_buffer[offset], 143,
         i2 + info_x, info_y + i + 1, 0, 16);
      }
    }

    for(i = 0; i < highlight_height; i++)
    {
      for(i2 = -(chars_show_width / 2);
       i2 < -(chars_show_width / 2) + chars_show_width; i2++)
      {
        if((i2 >= 0) && (i2 < highlight_width) &&
         (i < highlight_height))
        {
          draw_char_ext(current_char + i2 + (i * 32),
           143, i2 + info_x + current_width + 11,
           info_y + i + 1, 0, 16);
        }
        else
        {
          draw_char_ext(current_char + i2 + (i * 32),
           128, i2 + info_x + current_width + 11,
           info_y + i + 1, 0, 16);
        }
      }
    }

    update_screen();
    update_event_status_delay();

    key = get_key(keycode_internal);

    draw_new = 0;

    if(get_shift_status(keycode_internal) || (block_mode == 2))
    {
      if(!shifted)
      {
        block_x = x;
        block_y = y;
        block_start_x = x;
        block_start_y = y;
        block_mode = 1;
        block_width = 1;
        block_height = 1;
        shifted = 1;
      }
      else
      {
        block_start_x = block_x;
        block_start_y = block_y;

        if(block_start_x > x)
        {
          block_start_x = x;
          block_width = block_x - x + 1;
        }
        else
        {
          block_width = x - block_x + 1;
        }

        if(block_start_y > y)
        {
          block_start_y = y;
          block_height = block_y - y + 1;
        }
        else
        {
          block_height = y - block_y + 1;
        }
      }
    }
    else
    {
      shifted = 0;
      block_mode = 0;

      block_width = buffer_width;
      block_height = buffer_height;
      block_start_x = 0;
      block_start_y = 0;
    }

    if(get_mouse_drag())
    {
      int mouse_x, mouse_y;
      int draw_pixel;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_x >= chars_x) &&
       (mouse_x < chars_x + chars_width) &&
       (mouse_y >= chars_y) &&
       (mouse_y < chars_y + chars_height))
      {
        // Grid.
        if(small_chars)
        {
          get_real_mouse_position(&mouse_x, &mouse_y);

          x = (mouse_x / 8) - chars_x;
          y = (mouse_y / 7) - (chars_y * 2);
        }
        else
        {
          x = (mouse_x - chars_x) / 2;
          y = mouse_y - chars_y;
        }

        if(screen_mode)
        {
          x /= 2;
          draw_pixel = current_pixel;
        }
        else
        {
          draw_pixel = 1;
        }

        if(!block_mode)
        {
          if(get_mouse_status() & MOUSE_BUTTON(MOUSE_BUTTON_LEFT))
          {
            int dx = x - last_x;
            int dy = y - last_y;
            int first_x, first_y;
            int draw_x, draw_y;

            if(((last_x != x) || (last_y != y)) && (last_x != -1))
            {
              if(abs(dx) > abs(dy))
              {
                if(dx < 0)
                {
                  first_x = x;
                  first_y = y;
                  dx *= -1;
                  dy *= -1;
                }
                else
                {
                  first_x = last_x;
                  first_y = last_y;
                }

                for(i = 0; i <= dx; i++)
                {
                  draw_y = (dy * i / dx) + first_y;
                  buffer[(first_x + i) + (buffer_width * draw_y)] =
                   draw_pixel;
                }
              }
              else
              {
                if(dy < 0)
                {
                  first_x = x;
                  first_y = y;
                  dx *= -1;
                  dy *= -1;
                }
                else
                {
                  first_x = last_x;
                  first_y = last_y;
                }

                for(i = 0; i <= dy; i++)
                {
                  draw_x = (dx * i / dy) + first_x;
                  buffer[draw_x + (buffer_width * (first_y + i))] =
                   draw_pixel;
                }
              }
            }
            else
            {
              buffer[x + (buffer_width * y)] = draw_pixel;
            }
          }
          else
          {
            if(screen_mode)
              current_pixel = buffer[x + (buffer_width * y)];
            else
              buffer[x + (buffer_width * y)] = 0;
          }

          last_x = x;
          last_y = y;

          collapse_buffer(buffer, current_width,
           current_height, highlight_width, highlight_height,
           current_char, screen_mode);

          save_history();
        }
      }
    }
    else
    {
      last_x = -1;
    }

    switch(key)
    {
      case IKEY_LEFT:
      {
        if(get_alt_status(keycode_internal))
        {
          char *buffer_ptr = buffer +
           block_start_x + (block_start_y * buffer_width);
          int wrap;

          for(i = 0; i < block_height; i++,
           buffer_ptr += buffer_width)
          {
            wrap = buffer_ptr[0];
            memmove(buffer_ptr, buffer_ptr + 1,
             block_width - 1);
            buffer_ptr[block_width - 1] = wrap;
          }

          collapse_buffer(buffer, current_width,
           current_height, highlight_width, highlight_height,
           current_char, screen_mode);

          save_history();
        }
        else
        {
          x--;
          if(x < 0)
            x = buffer_width - 1;

          if(draw)
            draw_new = 1;
        }
        break;
      }

      case IKEY_RIGHT:
      {
        if(get_alt_status(keycode_internal))
        {
          char *buffer_ptr = buffer +
           block_start_x + (block_start_y * buffer_width);
          int wrap;

          for(i = 0; i < block_height; i++,
           buffer_ptr += buffer_width)
          {
            wrap = buffer_ptr[block_width - 1];
            memmove(buffer_ptr + 1, buffer_ptr,
             block_width - 1);
            buffer_ptr[0] = wrap;
          }

          collapse_buffer(buffer, current_width,
           current_height, highlight_width, highlight_height,
           current_char, screen_mode);

          save_history();
        }
        else
        {
          x++;
          if(x >= buffer_width)
            x = 0;

          if(draw)
            draw_new = 1;
        }
        break;
      }

      case IKEY_UP:
      {
        if(get_alt_status(keycode_internal))
        {
          char *buffer_ptr = buffer +
           block_start_x + (block_start_y * buffer_width);
          char *wrap_row = cmalloc(block_width);

          memcpy(wrap_row, buffer_ptr, block_width);

          for(i = 0; i < block_height - 1; i++,
           buffer_ptr += buffer_width)
          {
            memcpy(buffer_ptr, buffer_ptr + buffer_width,
             block_width);
          }

          memcpy(buffer_ptr, wrap_row, block_width);
          collapse_buffer(buffer, current_width,
           current_height, highlight_width, highlight_height,
           current_char, screen_mode);

          save_history();

          free(wrap_row);
        }
        else
        {
          y--;
          if(y < 0)
            y = buffer_height - 1;

          if(draw)
            draw_new = 1;
        }
        break;
      }

      case IKEY_DOWN:
      {
        if(get_alt_status(keycode_internal))
        {
          char *buffer_ptr = buffer +
           block_start_x + ((block_start_y + (block_height - 1)) *
           buffer_width);
          char *wrap_row = cmalloc(block_width);

          memcpy(wrap_row, buffer_ptr, block_width);

          for(i = 0; i < block_height - 1; i++,
           buffer_ptr -= buffer_width)
          {
            memcpy(buffer_ptr, buffer_ptr - buffer_width,
             block_width);
          }

          memcpy(buffer_ptr, wrap_row, block_width);
          collapse_buffer(buffer, current_width,
           current_height, highlight_width, highlight_height,
           current_char, screen_mode);

          save_history();

          free(wrap_row);
        }
        else
        {
          y++;
          if(y >= buffer_height)
            y = 0;

          if(draw)
            draw_new = 1;
        }
        break;
      }

      case IKEY_KP_MINUS:
      case IKEY_MINUS:
      {
        current_char -= highlight_width;
        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char, screen_mode);
        break;
      }

      case IKEY_KP_PLUS:
      case IKEY_EQUALS:
      {
        current_char += highlight_width;
        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char, screen_mode);
        break;
      }

      case IKEY_SPACE:
      {
        if(screen_mode)
          buffer[x + (y * buffer_width)] = current_pixel;
        else
          buffer[x + (y * buffer_width)] ^= 1;

        collapse_buffer(buffer, current_width,
         current_height, highlight_width, highlight_height,
         current_char, screen_mode);

        save_history();
        break;
      }

      case IKEY_RETURN:
      {
        int new_char =
         char_selection_ext(current_char, 1, &highlight_width,
         &highlight_height);
        int highlight_size = highlight_width * highlight_height;
        int search_order[16];
        int factors[6];
        int num_factors = 0;
        int low, high;
        int subdivision = 0;

        // The first choice should be about the square root,
        // biased high.
        search_order[0] = (int)(sqrt(highlight_size) + 0.75);

        // Subsequent choices should expand outwards
        low = search_order[0];
        high = search_order[0];
        i = 1;
        while((low > 1) || (high < 6))
        {
          if(high < 6)
          {
            high++;
            search_order[i] = high;
            i++;
          }

          if(low > 1)
          {
            low--;
            search_order[i] = low;
            i++;
          }
        }

        for(i = 0; i < 6; i++)
        {
          if(!(highlight_size % search_order[i]) &&
           (highlight_size / search_order[i] <= 3))
          {
            factors[num_factors] = search_order[i];
            num_factors++;
          }
        }

        if(num_factors > 1)
        {
          struct element **elements =
           cmalloc(sizeof(struct element *) * num_factors);
          char **radio_button_strings =
           cmalloc(sizeof(char *) * num_factors);
          char **radio_button_substrings =
           cmalloc(sizeof(char *) * num_factors);
          struct dialog di;

          for(i = 0; i < num_factors; i++)
          {
            radio_button_substrings[i] = cmalloc(32);
            radio_button_strings[i] = radio_button_substrings[i];
            sprintf(radio_button_strings[i], "%d x %d",
             factors[i], highlight_size / factors[i]);
          }

          elements[0] =
           construct_radio_button(8, 2,
           (const char **)radio_button_strings,
           num_factors, 5, &subdivision);
          elements[1] = construct_button(10, num_factors + 3,
           "OK", 0);

          construct_dialog(&di, "Choose subdivision", 28,
           8, 24, 6 + num_factors, elements, 2, 0);

          run_dialog(mzx_world, &di);
          destruct_dialog(&di);

          for(i = 0; i < num_factors; i++)
            free(radio_button_substrings[i]);
          free(radio_button_substrings);
          free(radio_button_strings);
          free(elements);
        }

        current_width = factors[subdivision];
        current_height = highlight_size / factors[subdivision];

        if(new_char >= 0)
          current_char = new_char;

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char, screen_mode);

        break;
      }

      case IKEY_q:
      {
        key = IKEY_ESCAPE;
        break;
      }

      case IKEY_DELETE:
      {
        char *buffer_ptr = buffer +
         block_start_x + (block_start_y * buffer_width);

        for(i = 0; i < block_height; i++,
         buffer_ptr += buffer_width)
        {
          memset(buffer_ptr, 0, block_width);
        }
        collapse_buffer(buffer, current_width,
         current_height, highlight_width, highlight_height,
         current_char, screen_mode);

        save_history();
        break;
      }

      case IKEY_F2:
      {
        // Copy buffer
        char *buffer_ptr = buffer +
         block_start_x + (block_start_y * buffer_width);
        char *dest_ptr = char_copy_buffer;

        if(block_mode)
          char_copy_use_offset = 1;
        else
          char_copy_use_offset = 0;

        char_copy_width = block_width;
        char_copy_height = block_height;

        if(get_ctrl_status(keycode_internal))
        {
          for(i = 0; i < block_height; i++,
           buffer_ptr += buffer_width, dest_ptr += block_width)
          {
            memcpy(dest_ptr, buffer_ptr, block_width);
            memset(buffer_ptr, 0, block_width);
          }
        }
        else
        {
          for(i = 0; i < block_height; i++,
           buffer_ptr += buffer_width, dest_ptr += block_width)
          {
            memcpy(dest_ptr, buffer_ptr, block_width);
          }
        }
        break;
      }

      case IKEY_F3:
      {
        // Paste buffer
        int copy_width = char_copy_width;
        int copy_height = char_copy_height;
        int dest_x = x;
        int dest_y = y;
        char *src_ptr = char_copy_buffer;
        char *buffer_ptr;

        if(!char_copy_use_offset)
        {
          dest_x = 0;
          dest_y = 0;
        }

        buffer_ptr = buffer +
         dest_x + (dest_y * buffer_width);

        if((dest_x + copy_width) > buffer_width)
          copy_width = buffer_width - dest_x;

        if((dest_y + copy_height) > buffer_height)
          copy_height = buffer_height - dest_y;

        for(i = 0; i < copy_height; i++,
         buffer_ptr += buffer_width, src_ptr += char_copy_width)
        {
          memcpy(buffer_ptr, src_ptr, copy_width);
        }

        collapse_buffer(buffer, current_width,
         current_height, highlight_width, highlight_height,
         current_char, screen_mode);

        save_history();
        break;
      }

      case IKEY_m:
      {
        // Mirror
        int temp, i2;
        int start_offset = block_start_x +
         (block_start_y * buffer_width);
        int previous_offset;
        int end_offset;

        for(i = 0; i < block_height; i++)
        {
          previous_offset = start_offset;
          end_offset = start_offset + block_width - 1;
          for(i2 = 0; i2 < (block_width / 2);
           i2++, start_offset++, end_offset--)
          {
            temp = buffer[start_offset];
            buffer[start_offset] = buffer[end_offset];
            buffer[end_offset] = temp;
          }
          start_offset = previous_offset + buffer_width;
        }

        collapse_buffer(buffer, current_width,
         current_height, highlight_width, highlight_height,
         current_char, screen_mode);

        save_history();
        break;
      }

      case IKEY_f:
      {
        if(get_alt_status(keycode_internal))
        {
          // Flood fill
          int check = buffer[x + (y * buffer_width)];
          int fill = check ^ 1;

          if(screen_mode)
            fill = current_pixel;

          if(check != fill)
          {
            fill_region(buffer, x, y, buffer_width, buffer_height,
             check, fill);
            collapse_buffer(buffer, current_width,
             current_height, highlight_width, highlight_height,
             current_char, screen_mode);
          }
        }
        else
        {
          // Flip
          char *temp_buffer = cmalloc(sizeof(char) * block_width);
          int start_offset = block_start_x +
           (block_start_y * buffer_width);
          int end_offset = start_offset +
           ((block_height - 1) * buffer_width);

          for(i = 0; i < (block_height / 2); i++,
           start_offset += buffer_width, end_offset -= buffer_width)
          {
            memcpy(temp_buffer, buffer + start_offset,
             block_width);
            memcpy(buffer + start_offset, buffer + end_offset,
             block_width);
            memcpy(buffer + end_offset, temp_buffer,
             block_width);
          }

          collapse_buffer(buffer, current_width,
           current_height, highlight_width, highlight_height,
           current_char, screen_mode);

          free(temp_buffer);
        }

        save_history();
        break;
      }

      case IKEY_TAB:
      {
        if(draw)
        {
          draw = 0;
        }
        else
        {
          if(get_shift_status(keycode_internal))
            draw = 2;
          else
            draw = 1;

          draw_new = 1;
        }

        break;
      }

      case IKEY_n:
      {
        // Negative
        char *buffer_ptr = buffer +
         block_start_x + (block_start_y * buffer_width);

        if(screen_mode)
        {
          for(i = 0; i < block_height; i++,
           buffer_ptr += (buffer_width - block_width))
          {
            for(i2 = 0; i2 < block_width; i2++, buffer_ptr++)
            {
              *buffer_ptr = 3 - *buffer_ptr;
            }
          }
        }
        else
        {
          for(i = 0; i < block_height; i++,
           buffer_ptr += (buffer_width - block_width))
          {
            for(i2 = 0; i2 < block_width; i2++, buffer_ptr++)
            {
              *buffer_ptr ^= 1;
            }
          }
        }

        collapse_buffer(buffer, current_width,
         current_height, highlight_width, highlight_height,
         current_char, screen_mode);

        save_history();
        break;
      }

      case IKEY_r:
      {
        // Redo
        if(get_alt_status(keycode_internal))
        {
          redo_history();

          expand_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           screen_mode);
        }
        break;
      }

      case IKEY_u:
      {
        // Undo
        if(get_alt_status(keycode_internal))
        {
          restore_history();

          expand_buffer(buffer, current_width, current_height,
           highlight_width, highlight_height, current_char,
           screen_mode);
        }
        break;
      }

      case IKEY_F4:
      {
        // ASCII Default
        int char_offset = current_char;
        int skip = 32 - highlight_width;

        for(i = 0; i < highlight_height; i++,
         char_offset += skip)
        {
          for(i2 = 0; i2 < highlight_width; i2++,
           char_offset++)
          {
            char_offset = char_offset % 256; // Wrap away from the protected set
            ec_load_char_ascii(char_offset);
          }
        }

        save_history();

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char, screen_mode);

        break;
      }

      case IKEY_F5:
      {
        // MZX Default
        int char_offset = current_char;
        int skip = 32 - highlight_width;

        for(i = 0; i < highlight_height; i++,
         char_offset += skip)
        {
          for(i2 = 0; i2 < highlight_width; i2++,
           char_offset++)
          {
            char_offset = char_offset % 256; // Wrap away from the protected set
            ec_load_char_mzx(char_offset);
          }
        }

        expand_buffer(buffer, current_width, current_height,
         highlight_width, highlight_height, current_char, screen_mode);

        save_history();
        break;
      }

#ifdef CONFIG_HELPSYS
      case IKEY_F1:
      {
        m_show();
        help_system(mzx_world);
        break;
      }
#endif

      case IKEY_HOME:
      {
        x = 0;
        y = 0;
        break;
      }

      case IKEY_END:
      {
        x = buffer_width - 1;
        y = buffer_height - 1;
        break;
      }

      case IKEY_x:
      {
        if(get_alt_status(keycode_internal))
        {
          // Character set
          const char *chr_ext[] = { ".CHR", NULL };
          char export_string[256] = { 0, };
          char export_name[256];
          int char_offset = current_char;
          int char_size = current_width * current_height;
          int current_file = 0;

          struct element *elements[] =
          {
            construct_number_box(3, 20, "Offset:  ",
             0, 255, 0, &char_offset),
            construct_number_box(28, 20, "First: ",
             0, 255, 0, &current_file),
            construct_number_box(52, 20, "Count: ",
             1, 256, 0, &num_files)
          };

          if(!file_manager(mzx_world, chr_ext, ".chr", export_string,
           "Export character set(s)", 1, 1, elements, 3, 2))
          {
            int num_files_present = num_files;

            if(strchr(export_string, '#') == NULL)
              num_files_present = 1;

            for(i = 0; i < num_files_present; i++, current_file++,
             char_offset += char_size)
            {
              replace_filenum(export_string, export_name,
               current_file);

              add_ext(export_name, ".chr");
              ec_save_set_var(export_name, char_offset,
               char_size);
            }
          }
        }
        break;
      }

      case IKEY_i:
      {
        if(get_alt_status(keycode_internal))
        {
          // Character set
          const char *chr_ext[] = { ".CHR", NULL };
          char import_string[256] = { 0, };
          char import_name[256];
          int char_offset = current_char;
          int current_file = 0;
          int char_size;

          struct element *elements[] =
          {
            construct_number_box(3, 20, "Offset:  ",
             0, 255, 0, &char_offset),
            construct_number_box(28, 20, "First: ",
             0, 255, 0, &current_file),
            construct_number_box(52, 20, "Count: ",
             1, 256, 0, &num_files)
          };

          if(!file_manager(mzx_world, chr_ext, NULL, import_string,
           "Import character set(s)", 1, 2, elements, 3, 2))
          {
            int num_files_present = num_files;

            if(strchr(import_string, '#') == NULL)
              num_files_present = 1;

            for(i = 0; i < num_files_present; i++, current_file++)
            {
              replace_filenum(import_string, import_name,
               current_file);

              add_ext(import_name, ".chr");
              char_size =
               ec_load_set_var(import_name, char_offset);

              if(char_size != -1)
                char_offset += char_size;
            }

            save_history();

            expand_buffer(buffer, current_width, current_height,
             highlight_width, highlight_height, current_char, screen_mode);
          }
        }
        break;
      }

      case IKEY_b:
      {
        if(get_alt_status(keycode_internal))
        {
          block_mode = 2;
          block_x = x;
          block_y = y;
          block_start_x = x;
          block_start_y = y;
          block_width = 1;
          block_height = 1;
          shifted = 1;
        }

        break;
      }

      case IKEY_ESCAPE:
      {
        if(block_mode == 2)
        {
          block_mode = 0;
          key = 0;
        }
        break;
      }

      case IKEY_KP1:
      case IKEY_1:
      {
        current_pixel = 0;
        break;
      }

      case IKEY_KP2:
      case IKEY_2:
      {
        current_pixel = 2;
        break;
      }

      case IKEY_KP3:
      case IKEY_3:
      {
        current_pixel = 1;
        break;
      }

      case IKEY_KP4:
      case IKEY_4:
      {
        current_pixel = 3;
        break;
      }

      case IKEY_INSERT:
      {
        current_pixel = buffer[x + (y * buffer_width)];
        break;
      }

    }

    if(draw_new)
    {
      switch(draw)
      {
        case 1:
        {
          if(screen_mode)
            buffer[x + (y * buffer_width)] = current_pixel;
          else
            buffer[x + (y * buffer_width)] = 1;
          break;
        }

        case 2:
        {
          buffer[x + (y * buffer_width)] = 0;
          break;
        }
      }
      collapse_buffer(buffer, current_width,
       current_height, highlight_width, highlight_height,
       current_char, screen_mode);

      save_history();
    }
  } while(key != IKEY_ESCAPE);

  set_mouse_mul(8, 14);

  restore_screen();
  update_screen();
  pop_context();

  if(screen_mode)
  {
    for(i = 0; i < 4; i++)
    {
      set_rgb(i + 2, saved_rgb[i][0], saved_rgb[i][1],
       saved_rgb[i][2]);
    }

    update_palette();
    set_screen_mode(screen_mode);
  }

  free_history();

  free(buffer);

  return current_char;
}
