/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "graphics.h"
#include "delay.h"
#include "world.h"
#include "configure.h"
#include "event.h"

// Base names for MZX's resource files.
// Prepends SHAREDIR from config.h for you, so these are ready to use.

#define MZX_DEFAULT_CHR SHAREDIR "mzx_default.chr"
#define MZX_BLANK_CHR   SHAREDIR "mzx_blank.chr"
#define MZX_SMZX_CHR    SHAREDIR "mzx_smzx.chr"
#define MZX_ASCII_CHR   SHAREDIR "mzx_ascii.chr"
#define MZX_EDIT_CHR    SHAREDIR "mzx_edit.chr"
#define SMZX_PAL        SHAREDIR "smzx.pal"

static graphics_data graphics;

static SDL_Color default_pal[16] =
{
  { 00, 00, 00 },
  { 00, 00, 170 },
  { 00, 170, 00 },
  { 00, 170, 170 },
  { 170, 00, 00 },
  { 170, 00, 170 },
  { 170, 85, 00 },
  { 170, 170, 170 },
  { 85, 85, 85 },
  { 85, 85, 255 },
  { 85, 255, 85 },
  { 85, 255, 255 },
  { 255, 85, 85 },
  { 255, 85, 255 },
  { 255, 255, 85 },
  { 255, 255, 255 }
};

/* SOFTWARE RENDERER CODE ****************************************************/

static void update_screen8(Uint8 *pixels, Uint32 pitch, Uint32 w, Uint32 h)
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  Uint32 cb_bg;
  Uint32 cb_fg;
  char_element *src = graphics.text_video;
  Uint8 *char_ptr;
  Uint32 char_colors[16];
  Uint32 current_char_byte;
  Uint8 current_color;
  Uint32 i, i2, i3;
  Uint32 line_advance = pitch / 4;
  Uint32 row_advance = (pitch * 14) / 4;
  Uint32 ticks = SDL_GetTicks();
  Uint32 *old_dest = NULL;

  dest = (Uint32 *)(pixels) + (pitch * ((h - 350) / 8)) + ((w - 640) / 8);

  old_dest = dest;

  if((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  if(!graphics.screen_mode)
  {
    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        // Fill in background color
        cb_bg = src->bg_color;
        cb_fg = src->fg_color;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        char_colors[0] = (cb_bg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[1] = (cb_bg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[2] = (cb_bg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[3] = (cb_bg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[4] = (cb_bg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[5] = (cb_bg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[6] = (cb_bg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[7] = (cb_bg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[8] = (cb_fg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[9] = (cb_fg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[10] = (cb_fg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[11] = (cb_fg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[12] = (cb_fg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[13] = (cb_fg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[14] = (cb_fg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[15] = (cb_fg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_fg;
#else
        char_colors[0] = (cb_bg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[1] = (cb_fg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[2] = (cb_bg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[3] = (cb_fg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_bg;
        char_colors[4] = (cb_bg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[5] = (cb_fg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[6] = (cb_bg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[7] = (cb_fg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_bg;
        char_colors[8] = (cb_bg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[9] = (cb_fg << 24) | (cb_bg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[10] = (cb_bg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[11] = (cb_fg << 24) | (cb_fg << 16) | (cb_bg << 8) | cb_fg;
        char_colors[12] = (cb_bg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[13] = (cb_fg << 24) | (cb_bg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[14] = (cb_bg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_fg;
        char_colors[15] = (cb_fg << 24) | (cb_fg << 16) | (cb_fg << 8) | cb_fg;
#endif

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 4];
          *(dest + 1) = char_colors[current_char_byte & 0x0F];
          dest += line_advance;
        }

        dest = ldest + 2;
      }
      dest = ldest2 + row_advance;
    }
  }
  else

  if(graphics.screen_mode != 3)
  {
    Uint32 cb_bb;
    Uint32 cb_bf;
    Uint32 cb_fb;
    Uint32 cb_ff;

    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        cb_bg = src->bg_color & 0x0F;
        cb_fg = src->fg_color & 0x0F;
        cb_bb = (cb_bg << 4) | cb_bg;
        cb_bb = (cb_bb << 8) | cb_bb;
        cb_bf = (cb_bg << 4) | cb_fg;
        cb_bf = (cb_bf << 8) | cb_bf;
        cb_fb = (cb_fg << 4) | cb_bg;
        cb_fb = (cb_fb << 8) | cb_fb;
        cb_ff = (cb_fg << 4) | cb_fg;
        cb_ff = (cb_ff << 8) | cb_ff;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        char_colors[0] = (cb_bb << 16) | cb_bb;
        char_colors[1] = (cb_bb << 16) | cb_bf;
        char_colors[2] = (cb_bb << 16) | cb_fb;
        char_colors[3] = (cb_bb << 16) | cb_ff;
        char_colors[4] = (cb_bf << 16) | cb_bb;
        char_colors[5] = (cb_bf << 16) | cb_bf;
        char_colors[6] = (cb_bf << 16) | cb_fb;
        char_colors[7] = (cb_bf << 16) | cb_ff;
        char_colors[8] = (cb_fb << 16) | cb_bb;
        char_colors[9] = (cb_fb << 16) | cb_bf;
        char_colors[10] = (cb_fb << 16) | cb_fb;
        char_colors[11] = (cb_fb << 16) | cb_ff;
        char_colors[12] = (cb_ff << 16) | cb_bb;
        char_colors[13] = (cb_ff << 16) | cb_bf;
        char_colors[14] = (cb_ff << 16) | cb_fb;
        char_colors[15] = (cb_ff << 16) | cb_ff;
#else
        char_colors[0] = (cb_bb << 16) | cb_bb;
        char_colors[1] = (cb_bf << 16) | cb_bb;
        char_colors[2] = (cb_fb << 16) | cb_bb;
        char_colors[3] = (cb_ff << 16) | cb_bb;
        char_colors[4] = (cb_bb << 16) | cb_bf;
        char_colors[5] = (cb_bf << 16) | cb_bf;
        char_colors[6] = (cb_fb << 16) | cb_bf;
        char_colors[7] = (cb_ff << 16) | cb_bf;
        char_colors[8] = (cb_bb << 16) | cb_fb;
        char_colors[9] = (cb_bf << 16) | cb_fb;
        char_colors[10] = (cb_fb << 16) | cb_fb;
        char_colors[11] = (cb_ff << 16) | cb_fb;
        char_colors[12] = (cb_bb << 16) | cb_ff;
        char_colors[13] = (cb_bf << 16) | cb_ff;
        char_colors[14] = (cb_fb << 16) | cb_ff;
        char_colors[15] = (cb_ff << 16) | cb_ff;
#endif

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 4];
          *(dest + 1) = char_colors[current_char_byte & 0x0F];
          dest += line_advance;
        }
        dest = ldest + 2;
      }
      dest = ldest2 + row_advance;
    }
  }
  else
  {
    Uint32 cb_0;
    Uint32 cb_1;
    Uint32 cb_2;
    Uint32 cb_3;

    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        current_color = (src->bg_color << 4) | (src->fg_color & 0x0F);
        // Fill in background color
        cb_0 = current_color;
        cb_1 = (current_color + 1) & 0xFF;
        cb_2 = (current_color + 2) & 0xFF;
        cb_3 = (current_color + 3) & 0xFF;

        cb_0 |= cb_0 << 8;
        cb_1 |= cb_1 << 8;
        cb_2 |= cb_2 << 8;
        cb_3 |= cb_3 << 8;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        char_colors[0] = (cb_0 << 16) | cb_0;
        char_colors[1] = (cb_0 << 16) | cb_2;
        char_colors[2] = (cb_0 << 16) | cb_1;
        char_colors[3] = (cb_0 << 16) | cb_3;
        char_colors[4] = (cb_2 << 16) | cb_0;
        char_colors[5] = (cb_2 << 16) | cb_2;
        char_colors[6] = (cb_2 << 16) | cb_1;
        char_colors[7] = (cb_2 << 16) | cb_3;
        char_colors[8] = (cb_1 << 16) | cb_0;
        char_colors[9] = (cb_1 << 16) | cb_2;
        char_colors[10] = (cb_1 << 16) | cb_1;
        char_colors[11] = (cb_1 << 16) | cb_3;
        char_colors[12] = (cb_3 << 16) | cb_0;
        char_colors[13] = (cb_3 << 16) | cb_2;
        char_colors[14] = (cb_3 << 16) | cb_1;
        char_colors[15] = (cb_3 << 16) | cb_3;
#else
        char_colors[0] = (cb_0 << 16) | cb_0;
        char_colors[1] = (cb_2 << 16) | cb_0;
        char_colors[2] = (cb_1 << 16) | cb_0;
        char_colors[3] = (cb_3 << 16) | cb_0;
        char_colors[4] = (cb_0 << 16) | cb_2;
        char_colors[5] = (cb_2 << 16) | cb_2;
        char_colors[6] = (cb_1 << 16) | cb_2;
        char_colors[7] = (cb_3 << 16) | cb_2;
        char_colors[8] = (cb_0 << 16) | cb_1;
        char_colors[9] = (cb_2 << 16) | cb_1;
        char_colors[10] = (cb_1 << 16) | cb_1;
        char_colors[11] = (cb_3 << 16) | cb_1;
        char_colors[12] = (cb_0 << 16) | cb_3;
        char_colors[13] = (cb_2 << 16) | cb_3;
        char_colors[14] = (cb_1 << 16) | cb_3;
        char_colors[15] = (cb_3 << 16) | cb_3;
#endif

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 4];
          *(dest + 1) = char_colors[current_char_byte & 0x0F];
          dest += line_advance;
        }
        dest = ldest + 2;
      }
      dest = ldest2 + row_advance;
    }
  }

  if(graphics.mouse_status)
  {
    int mouse_x, mouse_y;
    int current_colors;
    get_real_mouse_position(&mouse_x, &mouse_y);

    mouse_x = (mouse_x / graphics.mouse_width_mul) *
     graphics.mouse_width_mul;
    mouse_y = (mouse_y / graphics.mouse_height_mul) *
     graphics.mouse_height_mul;

    dest = old_dest + (mouse_x / 4) + (mouse_y * line_advance);

    for(i = 0; i < graphics.mouse_height_mul; i++)
    {
      ldest = dest;
      for(i2 = 0; i2 < (graphics.mouse_width_mul / 4);
       i2++, dest++)
      {
        current_colors = *dest;
        *dest =
         (0x0F0F0F0F - (current_colors & 0x0F0F0F0F)) |
         (current_colors & 0xF0F0F0F0);
      }
      dest = ldest + line_advance;
    }
  }

  // Draw cursor perhaps
  if(graphics.cursor_flipflop &&
   (graphics.cursor_mode != cursor_mode_invisible))
  {
    char_element *cursor_element = graphics.text_video +
     graphics.cursor_x +  (graphics.cursor_y * 80);
    Uint32 cursor_color;
    Uint32 cursor_char = cursor_element->char_value;
    Uint32 lines = 0;
    Uint32 *cursor_offset = old_dest;
    Uint32 i;
    Uint32 cursor_solid = 0xFFFFFFFF;
    Uint32 *char_offset = (Uint32 *)(graphics.charset + (cursor_char * 14));
    Uint32 bg_color = cursor_element->bg_color;
    cursor_offset += (graphics.cursor_x * 2) +
     (graphics.cursor_y * row_advance);

    // Choose FG
    cursor_color = cursor_element->fg_color;

    // See if the cursor char is completely solid or completely
    // empty
    for(i = 0; i < 3; i++)
    {
      cursor_solid &= *char_offset;
      char_offset++;
    }
    // Get the last bit
    cursor_solid &= (*((Uint16 *)char_offset)) | 0xFFFF0000;

    // Solid cursor, use BG instead
    if(cursor_solid == 0xFFFFFFFF)
    {
      // But wait! What if the background is the same as the foreground?
      // If so, use +8 instead.
      if(bg_color == cursor_color)
      {
        cursor_color = (bg_color + 8) & 0x0F;
      }
      else
      {
        cursor_color = bg_color;
      }
    }
    else

    // What if the foreground is the same as the background?
    // It needs to flash +8 then
    if(bg_color == cursor_color)
    {
      cursor_color = (cursor_color + 8) & 0x0F;
    }

    cursor_color = cursor_color | (cursor_color << 8);
    cursor_color = cursor_color | (cursor_color << 16);

    switch(graphics.cursor_mode)
    {
      case cursor_mode_underline:
      {
        lines = 2;
        cursor_offset += (12 * line_advance);
        break;
      }

      case cursor_mode_solid:
      {
        lines = 14;
        break;
      }

      default:
      {
        break;
      }
    }

    for(i = 0; i < lines; i++)
    {
      *cursor_offset = cursor_color;
      *(cursor_offset + 1) = cursor_color;
      cursor_offset += line_advance;
    }
  }
}

static void update_screen32(Uint32 *pixels, Uint32 pitch, Uint32 w, Uint32 h)
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  char_element *src = graphics.text_video;
  Uint8 *char_ptr;
  Uint32 char_colors[4];
  Uint32 current_char_byte;
  Uint8 current_color;
  Uint32 i, i2, i3;
  Sint32 i4;
  Uint32 line_advance = (pitch / sizeof(Uint32));
  Uint32 line_advance_sub;
  Uint32 row_advance = ((pitch / sizeof(Uint32)) * 14);
  Uint32 ticks = SDL_GetTicks();
  Uint32 *old_dest = NULL;

  dest = (Uint32 *)(pixels) + (line_advance * ((h - 350) / 2)) + ((w - 640) /
   2);

  line_advance_sub = line_advance - 8;

  old_dest = dest;

  if((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  if(!graphics.screen_mode)
  {
    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        // Fill in background color
        char_colors[0] = graphics.flat_intensity_palette[src->bg_color];
        char_colors[1] = graphics.flat_intensity_palette[src->fg_color];

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;

          for(i4 = 7; i4 >= 0; i4--, dest++)
          {
            *dest = char_colors[(current_char_byte >> i4) & 0x01];
          }

          dest += line_advance_sub;
        }
        dest = ldest + 8;
      }
      dest = ldest2 + row_advance;
    }
  }
  else

  if(graphics.screen_mode != 3)
  {
    Uint32 cb_bg, cb_fg;
    Uint32 c_color;

    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        cb_bg = (src->bg_color & 0x0F);
        cb_fg = (src->fg_color & 0x0F);

        char_colors[0] =
         graphics.flat_intensity_palette[(cb_bg << 4) | cb_bg];
        char_colors[1] =
         graphics.flat_intensity_palette[(cb_bg << 4) | cb_fg];
        char_colors[2] =
         graphics.flat_intensity_palette[(cb_fg << 4) | cb_bg];
        char_colors[3] =
         graphics.flat_intensity_palette[(cb_fg << 4) | cb_fg];

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          for(i4 = 6; i4 >= 0; i4 -= 2, dest += 2)
          {
            c_color = char_colors[(current_char_byte >> i4) & 0x03];
            *dest = c_color;
            *(dest + 1) = c_color;
          }
          dest += line_advance_sub;
        }
        dest = ldest + 8;
      }
      dest = ldest2 + row_advance;
    }
  }
  else
  {
    Uint32 c_color;

    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        current_color = (src->bg_color << 4) | (src->fg_color & 0x0F);
        // Fill in background color
        char_colors[0] = graphics.flat_intensity_palette[current_color];
        char_colors[1] =
         graphics.flat_intensity_palette[(current_color + 2) & 0xFF];
        char_colors[2] =
         graphics.flat_intensity_palette[(current_color + 1) & 0xFF];
        char_colors[3] =
         graphics.flat_intensity_palette[(current_color + 3) & 0xFF];

        // Fill in foreground color
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          for(i4 = 6; i4 >= 0; i4 -= 2, dest += 2)
          {
            c_color = char_colors[(current_char_byte >> i4) & 0x03];
            *dest = c_color;
            *(dest + 1) = c_color;
          }
          dest += line_advance_sub;
        }
        dest = ldest + 8;
      }
      dest = ldest2 + row_advance;
    }
  }

  if(graphics.mouse_status)
  {
    int mouse_x, mouse_y;
    get_real_mouse_position(&mouse_x, &mouse_y);

    mouse_x = (mouse_x / graphics.mouse_width_mul) *
     graphics.mouse_width_mul;
    mouse_y = (mouse_y / graphics.mouse_height_mul) *
     graphics.mouse_height_mul;

    dest = old_dest + mouse_x + (mouse_y * line_advance);

    for(i = 0; i < graphics.mouse_height_mul; i++)
    {
      ldest = dest;
      for(i2 = 0; i2 < graphics.mouse_width_mul; i2++, dest++)
      {
        *dest = 0xFFFFFFFF - *dest;
      }
      dest = ldest + line_advance;
    }
  }

  // Draw cursor perhaps
  if(graphics.cursor_flipflop &&
   (graphics.cursor_mode != cursor_mode_invisible))
  {
    char_element *cursor_element = graphics.text_video +
     graphics.cursor_x +  (graphics.cursor_y * 80);
    Uint32 cursor_color;
    Uint32 cursor_char = cursor_element->char_value;
    Uint32 lines = 0;
    Uint32 *cursor_offset = old_dest;
    Uint32 i;
    Uint32 cursor_solid = 0xFFFFFFFF;
    Uint32 *char_offset = (Uint32 *)(graphics.charset + (cursor_char * 14));
    Uint32 bg_color = cursor_element->bg_color;
    cursor_offset += (graphics.cursor_x * 8) + (graphics.cursor_y * row_advance);

    // Choose FG
    cursor_color = cursor_element->fg_color;

    // See if the cursor char is completely solid or completely
    // empty
    for(i = 0; i < 3; i++)
    {
      cursor_solid &= *char_offset;
      char_offset++;
    }
    // Get the last bit
    cursor_solid &= (*((Uint16 *)char_offset)) | 0xFFFF0000;

    // Solid cursor, use BG instead
    if(cursor_solid == 0xFFFFFFFF)
    {
      // But wait! What if the background is the same as the foreground?
      // If so, use +8 instead.
      if(bg_color == cursor_color)
      {
        cursor_color = (bg_color + 8) & 0x0F;
      }
      else
      {
        cursor_color = bg_color;
      }
    }
    else

    // What if the foreground is the same as the background?
    // It needs to flash +8 then
    if(bg_color == cursor_color)
    {
      cursor_color = (cursor_color + 8) & 0x0F;
    }

    switch(graphics.cursor_mode)
    {
      case cursor_mode_underline:
      {
        lines = 2;
        cursor_offset += (12 * line_advance);
        break;
      }

      case cursor_mode_solid:
      {
        lines = 14;
        break;
      }

      default:
      {
        break;
      }
    }

    cursor_color = graphics.flat_intensity_palette[cursor_color];

    for(i = 0; i < lines; i++)
    {
      for(i2 = 0; i2 < 8; i2++, cursor_offset++)
      {
        *cursor_offset = cursor_color;
      }
      cursor_offset += line_advance_sub;
    }
  }
}

/* SOFTWARE RENDERER CODE ****************************************************/

static int soft_init_video(config_info *conf)
{
  graphics.bits_per_pixel = 32;

  // we only have 8bit and 32bit software renderers
  if (conf->force_bpp == 8 || conf->force_bpp == 32)
    graphics.bits_per_pixel = conf->force_bpp;

  return set_video_mode();
}

static int soft_check_video_mode(int width, int height, int depth, int flags)
{
  return SDL_VideoModeOK(width, height, depth, flags);
}

static int soft_set_video_mode(int width, int height, int depth, int flags,
                                int fullscreen)
{
  graphics.screen = SDL_SetVideoMode(width, height, depth, flags);
  return graphics.screen != NULL;
}

static void soft_update_screen(void)
{
  SDL_LockSurface(graphics.screen);

  if (graphics.bits_per_pixel == 32)
  {
    update_screen32((Uint32*)graphics.screen->pixels, graphics.screen->pitch,
     graphics.screen->w, graphics.screen->h);
  }
  else
  {
    update_screen8((Uint8*)graphics.screen->pixels, graphics.screen->pitch,
     graphics.screen->w, graphics.screen->h);
  }

  SDL_UnlockSurface(graphics.screen);

  SDL_Flip(graphics.screen);
}

static void soft_update_colors(SDL_Color *palette, Uint32 count)
{
  if (graphics.bits_per_pixel == 32)
  {
    for (Uint32 i = 0; i < count; i++)
      graphics.flat_intensity_palette[i] = SDL_MapRGBA(graphics.screen->format,
       palette[i].r, palette[i].g, palette[i].b, 255);
  }
  else
  {
    SDL_SetColors(graphics.screen, palette, 0, count);
  }
}

static void soft_resize_screen(int w, int h)
{
  update_screen();
  update_palette();
}

static void soft_set_screen_coords(int x, int y, int *screen_x, int *screen_y)
{
  int target_width = graphics.window_width;
  int target_height = graphics.window_height;

  if(graphics.fullscreen)
  {
    target_width = graphics.resolution_width;
    target_height = graphics.resolution_height;
  }

  *screen_x = x + (target_width - 640) / 2;
  *screen_y = y + (target_height - 350) / 2;
}

static void hard_set_screen_coords(int x, int y, int *screen_x, int *screen_y)
{
  int target_width = graphics.window_width;
  int target_height = graphics.window_height;

  if(graphics.fullscreen)
  {
    target_width = graphics.resolution_width;
    target_height = graphics.resolution_height;
  }

  *screen_x = x * target_width / 640;
  *screen_y = y * target_height / 350;
}

#if defined(CONFIG_OPENGL) && !defined(PSP_BUILD)

/* OPENGL RENDERER #1 CODE ***************************************************/

#include "SDL_opengl.h"

#define GL_NON_POWER_2_WIDTH	640
#define GL_NON_POWER_2_HEIGHT	350
#define GL_POWER_2_WIDTH	1024
#define GL_POWER_2_HEIGHT	512

typedef struct
{
  void (APIENTRY *glBegin)(GLenum mode);
  void (APIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
  void (APIENTRY *glBindTexture)(GLenum target, GLuint texture);
  void (APIENTRY *glClear)(GLbitfield mask);
  void (APIENTRY *glColor3ubv)(const GLubyte *v) ;
  void (APIENTRY *glColor4f)(GLfloat red, GLfloat green, GLfloat blue,
                             GLfloat alpha);
  void (APIENTRY *glColor4ub)(GLubyte red, GLubyte green, GLubyte blue,
                              GLubyte alpha);
  void (APIENTRY *glCopyTexImage2D)(GLenum target, GLint level,
                                    GLenum internalFormat, GLint x, GLint y,
                                    GLsizei width, GLsizei height,
                                    GLint border);
  void (APIENTRY *glDisable)(GLenum cap);
  void (APIENTRY *glEnable)(GLenum cap);
  void (APIENTRY *glEnd)(void);
  void (APIENTRY *glFrustum)(GLdouble left, GLdouble right, GLdouble bottom,
                             GLdouble top, GLdouble near, GLdouble far);
  void (APIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
  const GLubyte* (APIENTRY *glGetString)(GLenum name);
  void (APIENTRY *glLoadIdentity)(void);
  void (APIENTRY *glMatrixMode)(GLenum mode);
  void (APIENTRY *glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
  void (APIENTRY *glPushMatrix)(void);
  void (APIENTRY *glTexCoord2f)(GLfloat s, GLfloat t);
  void (APIENTRY *glTexEnvf)(GLenum target, GLenum pname, GLfloat param);
  void (APIENTRY *glTexImage2D)(GLenum target, GLint level,
                                GLint internalformat, GLsizei width,
                                GLsizei height, GLint border, GLenum format,
                                GLenum type, const GLvoid *pixels);
  void (APIENTRY *glTexSubImage2D)(GLenum target, GLint level, GLint xoffset,
                                   GLint yoffset, GLsizei width, GLsizei height,
                                   GLenum format, GLenum type,
                                   const GLvoid *pixels);
  void (APIENTRY *glTranslatef)(GLfloat x, GLfloat y, GLfloat z);
  void (APIENTRY *glTexParameteri)(GLenum target, GLenum pname, GLint param);
  void (APIENTRY *glVertex3f)(GLfloat x, GLfloat y, GLfloat z);
  void (APIENTRY *glVertex3i)(GLint x, GLint y, GLint z);
  void (APIENTRY *glViewport)(GLint x, GLint y, GLsizei width,
                              GLsizei height);
} gl_syms;

static gl_syms gl;

#ifdef OPENGL_LINKED

#include "GL/gl.h"

/* On Windows, with ATi video cards, it seems that dynamically loading the
 * OpenGL symbols just doesn't work. There's no good reason why it doesn't
 * work, but it doesn't. So on win32, we'll link to GL, and we'll just
 * add one level of indirection (which should be optimized out anyway).
 */
static int load_gl_syms(void)
{
  // map to the actual OpenGL symbols
  gl.glBegin = glBegin;
  gl.glBlendFunc = glBlendFunc;
  gl.glBindTexture = glBindTexture;
  gl.glClear = glClear;
  gl.glColor3ubv = glColor3ubv;
  gl.glColor4f = glColor4f;
  gl.glColor4ub = glColor4ub;
  gl.glCopyTexImage2D = glCopyTexImage2D;
  gl.glDisable = glDisable;
  gl.glEnable = glEnable;
  gl.glEnd = glEnd;
  gl.glFrustum = glFrustum;
  gl.glGenTextures = glGenTextures;
  gl.glGetString = glGetString;
  gl.glLoadIdentity = glLoadIdentity;
  gl.glMatrixMode = glMatrixMode;
  gl.glTexParameterf = glTexParameterf;
  gl.glPushMatrix = glPushMatrix;
  gl.glTexCoord2f = glTexCoord2f;
  gl.glTexEnvf = glTexEnvf;
  gl.glTexImage2D = glTexImage2D;
  gl.glTexSubImage2D = glTexSubImage2D;
  gl.glTranslatef = glTranslatef;
  gl.glTexParameteri = glTexParameteri;
  gl.glVertex3f = glVertex3f;
  gl.glVertex3i = glVertex3i;
  gl.glViewport = glViewport;

  return true;
}

static inline int can_use_opengl(void)
{
  return true;
}

#else // !OPENGL_LINKED

static int gl_syms_loaded = false;

/* Here go the the loading of symbols from the shared object, that we care
 * about. It's unfortunate we can't recycle the function prototypes from GL.h,
 * but these are copied directly from it. If you want to access a GL function,
 * add it to the list here and then call gl.FunctionName, instead of calling
 * the function directly.
 */
static int load_gl_syms(void)
{
  if (gl_syms_loaded)
    return true;

  // Since 1.0
  gl.glBegin = (void (APIENTRY *)(GLenum mode))
   SDL_GL_GetProcAddress("glBegin");
  if (!gl.glBegin)
    return false;

  // Since 1.1
  gl.glBindTexture = (void (APIENTRY *)(GLenum target, GLuint texture))
   SDL_GL_GetProcAddress("glBindTexture");
  if (!gl.glBindTexture)
    return false;

  // Since 1.0
  gl.glBlendFunc = (void (APIENTRY *)(GLenum sfactor, GLenum dfactor))
   SDL_GL_GetProcAddress("glBlendFunc");
  if (!gl.glBlendFunc)
    return false;

  // Since 1.0
  gl.glClear = (void (APIENTRY *)(GLbitfield mask))
   SDL_GL_GetProcAddress("glClear");
  if (!gl.glClear)
    return false;

  // Since 1.0
  gl.glColor3ubv = (void (APIENTRY *)(const GLubyte *v))
   SDL_GL_GetProcAddress("glColor3ubv");
  if (!gl.glColor3ubv)
    return false;

  // Since 1.0
  gl.glColor4f = (void (APIENTRY *)(GLfloat red, GLfloat green, GLfloat blue,
                                     GLfloat alpha))
   SDL_GL_GetProcAddress("glColor4f");
  if (!gl.glColor4f)
    return false;

  // Since 1.0
  gl.glColor4ub = (void (APIENTRY *)(GLubyte red, GLubyte green, GLubyte blue,
                                      GLubyte alpha))
   SDL_GL_GetProcAddress("glColor4ub");
  if (!gl.glColor4ub)
    return false;

  // Since 1.1
  gl.glCopyTexImage2D = (void (APIENTRY *)(GLenum target, GLint level,
                                            GLenum internalFormat, GLint x,
                                            GLint y, GLsizei width,
                                            GLsizei height, GLint border))
   SDL_GL_GetProcAddress("glCopyTexImage2D");
  if (!gl.glCopyTexImage2D)
    return false;

  // Since 1.0 (parameters may require more recent version)
  gl.glDisable = (void (APIENTRY *)(GLenum cap))
   SDL_GL_GetProcAddress("glDisable");
  if (!gl.glDisable)
    return false;

  // Since 1.0 (parameters may require more recent version)
  gl.glEnable = (void (APIENTRY *)(GLenum cap))
   SDL_GL_GetProcAddress("glEnable");
  if (!gl.glEnable)
    return false;

  // Since 1.0
  gl.glEnd = (void (APIENTRY *)(void))
   SDL_GL_GetProcAddress("glEnd");
  if (!gl.glEnd)
    return false;

  // Since 1.0
  gl.glFrustum = (void (APIENTRY *)(GLdouble left, GLdouble right,
                                     GLdouble bottom, GLdouble top,
                                     GLdouble near, GLdouble far))
   SDL_GL_GetProcAddress("glFrustum");
  if (!gl.glFrustum)
    return false;

  // Since 1.1
  gl.glGenTextures = (void (APIENTRY *)(GLsizei n, GLuint *textures))
   SDL_GL_GetProcAddress("glGenTextures");
  if (!gl.glGenTextures)
    return false;

  // Since 1.0
  gl.glGetString = (const GLubyte *(APIENTRY *)(GLenum name))
   SDL_GL_GetProcAddress("glGetString");
  if (!gl.glGetString)
    return false;

  // Since 1.0
  gl.glLoadIdentity = (void (APIENTRY *)(void))
   SDL_GL_GetProcAddress("glLoadIdentity");
  if (!gl.glLoadIdentity)
    return false;

  // Since 1.0
  gl.glMatrixMode = (void (APIENTRY *)(GLenum mode))
   SDL_GL_GetProcAddress("glMatrixMode");
  if (!gl.glMatrixMode)
    return false;

  // Since 1.0
  gl.glPushMatrix = (void (APIENTRY *)())
   SDL_GL_GetProcAddress("glPushMatrix");
  if (!gl.glPushMatrix)
    return false;

  // Since 1.0
  gl.glTexCoord2f = (void (APIENTRY *)(GLfloat s, GLfloat t))
   SDL_GL_GetProcAddress("glTexCoord2f");
  if (!gl.glTexCoord2f)
    return false;

  // Since 1.0 (parameters may require more recent version)
  gl.glTexEnvf = (void (APIENTRY *)(GLenum target, GLenum pname,
                                     GLfloat param))
   SDL_GL_GetProcAddress("glTexEnvf");
  if (!gl.glTexEnvf)
    return false;

  // Since 1.0 (parameters may require more recent version)
  gl.glTexImage2D = (void (APIENTRY *)(GLenum target, GLint level,
                                       GLint internalformat, GLsizei width,
                                       GLsizei height, GLint border,
                                       GLenum format, GLenum type,
                                       const GLvoid *pixels))
   SDL_GL_GetProcAddress("glTexImage2D");
  if (!gl.glTexImage2D)
    return false;

  // Since 1.0
  gl.glTexParameterf = (void (APIENTRY *)(GLenum target, GLenum pname,
                                           GLfloat param))
   SDL_GL_GetProcAddress("glTexParameterf");
  if (!gl.glTexParameterf)
    return false;

  // Since 1.0
  gl.glTexParameteri = (void (APIENTRY *)(GLenum target, GLenum pname,
                                           GLint param))
   SDL_GL_GetProcAddress("glTexParameteri");
  if (!gl.glTexParameteri)
    return false;

  // Since 1.1
  gl.glTexSubImage2D = (void (APIENTRY *)(GLenum target, GLint level,
                                           GLint xoffset, GLint yoffset,
                                           GLsizei width, GLsizei height,
                                           GLenum format, GLenum type,
                                           const GLvoid *pixels))
   SDL_GL_GetProcAddress("glTexSubImage2D");
  if (!gl.glTexSubImage2D)
    return false;

  // Since 1.0
  gl.glTranslatef = (void (APIENTRY *)(GLfloat x, GLfloat y, GLfloat z))
   SDL_GL_GetProcAddress("glTranslatef");
  if (!gl.glTranslatef)
    return false;

  // Since 1.0
  gl.glVertex3f = (void (APIENTRY *)(GLfloat x, GLfloat y, GLfloat z))
   SDL_GL_GetProcAddress("glVertex3f");
  if (!gl.glVertex3f)
    return false;

  // Since 1.0
  gl.glVertex3i = (void (APIENTRY *)(GLint x, GLint y, GLint z))
   SDL_GL_GetProcAddress("glVertex3i");
  if (!gl.glVertex3i)
    return false;

  // Since 1.0
  gl.glViewport = (void (APIENTRY *)(GLint x, GLint y, GLsizei width,
                                      GLsizei height))
   SDL_GL_GetProcAddress("glViewport");
  if (!gl.glViewport)
    return false;

#ifdef DEBUG
  fprintf(stdout, "Loaded OpenGL symbol table successfully.\n");
#endif

  gl_syms_loaded = true;
  return true;
}

static inline int can_use_opengl(void)
{
  return SDL_GL_LoadLibrary(NULL) >= 0;
}

#endif // !OPENGL_LINKED

static int gl_strip_flags(int flags)
{
  int new_flags = 0;

  if (flags & SDL_FULLSCREEN)
    new_flags |= SDL_FULLSCREEN;
  if (flags & SDL_RESIZABLE)
    new_flags |= SDL_RESIZABLE;
  return new_flags | SDL_OPENGL;
}

static void gl_set_filter_method(char *method)
{
  GLint gl_filter_method = GL_LINEAR;

  if (!strcmp(method, "nearest"))
    gl_filter_method = GL_NEAREST;

  gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_method);
  gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_method);
}

static int gl1_init_video(config_info *conf)
{
  int internal_width, internal_height;

  graphics.allow_resize = conf->allow_resize;
  graphics.gl_filter_method = conf->gl_filter_method;
  graphics.bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if (conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics.bits_per_pixel = conf->force_bpp;

  if (!set_video_mode())
    return false;

  // NOTE: These must come AFTER set_video_mode()!
  const char *version = (const char *)gl.glGetString(GL_VERSION);
  const char *extensions = (const char *)gl.glGetString(GL_EXTENSIONS);

  // we need a specific "version" of OpenGL compatibility
  if (version && atof(version) < 1.1)
  {
    fprintf(stderr, "Your OpenGL implementation is too old (need v1.1).\n");
    return false;
  }

  // we also might be able to utilise an extension
  if (extensions && strstr(extensions, "GL_ARB_texture_non_power_of_two"))
  {
    internal_width = GL_NON_POWER_2_WIDTH;
    internal_height = GL_NON_POWER_2_HEIGHT;
  }
  else
  {
    internal_width = GL_POWER_2_WIDTH;
    internal_height = GL_POWER_2_HEIGHT;
  }

  // We want to deal internally with 32bit surfaces
  graphics.screen = SDL_CreateRGBSurface(SDL_SWSURFACE, internal_width,
   internal_height, 32, 0, 0, 0, 0);

  return true;
}

static int gl1_check_video_mode(int width, int height, int depth, int flags)
{
  return SDL_VideoModeOK(width, height, depth, gl_strip_flags(flags));
}

static int gl1_set_video_mode(int width, int height, int depth, int flags,
                              int fullscreen)
{
  GLuint texture_number;

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  if (!SDL_SetVideoMode(width, height, depth, gl_strip_flags(flags)))
    return false;

  if (!load_gl_syms())
    return false;

  gl.glViewport(0, 0, width, height);

  gl.glEnable(GL_TEXTURE_2D);

  gl.glGenTextures(1, &texture_number);
  gl.glBindTexture(GL_TEXTURE_2D, texture_number);

  gl_set_filter_method(graphics.gl_filter_method);

  return true;
}

static void gl1_update_screen(void)
{
  float texture_width;
  float texture_height;

  SDL_LockSurface(graphics.screen);

  update_screen32((Uint32*)graphics.screen->pixels, graphics.screen->pitch,
   640, 350);

  texture_width = (float)640 / graphics.screen->w;
  texture_height = (float)350 / graphics.screen->h;

  gl.glTexImage2D(GL_TEXTURE_2D, 0, 3, graphics.screen->w, graphics.screen->h,
   0, GL_BGRA, GL_UNSIGNED_BYTE, graphics.screen->pixels);

  SDL_UnlockSurface(graphics.screen);

  gl.glBegin(GL_QUADS);
    gl.glTexCoord2f(0.0, texture_height);
    gl.glVertex3f(-1.0, -1.0, 0.0);

    gl.glTexCoord2f(texture_width, texture_height);
    gl.glVertex3f(1.0, -1.0, 0.0);

    gl.glTexCoord2f(texture_width, 0.0);
    gl.glVertex3f(1.0, 1.0, 0.0);

    gl.glTexCoord2f(0.0, 0.0);
    gl.glVertex3f(-1.0, 1.0, 0.0);
  gl.glEnd();

  SDL_GL_SwapBuffers();
}

static void gl1_update_colors(SDL_Color *palette, Uint32 count)
{
  for (Uint32 i = 0; i < count; i++)
    graphics.flat_intensity_palette[i] = SDL_MapRGBA(graphics.screen->format,
      palette[i].r, palette[i].g, palette[i].b, 255);
}

/* OPENGL RENDERER #2 CODE ***************************************************/

#define SAFE_TEXTURE_MARGIN_X	0.0004
#define SAFE_TEXTURE_MARGIN_Y	0.0002

static struct
{
  Uint8 charset_texture[14 * 256 * 16];
  GLuint texture_number[2];
  GLubyte palette[3*256];
  Uint8 remap_texture;
  Uint8 ignore_linear;
} gl_state;

static char *gl2_char_bitmask_to_texture(signed char *c, char *p)
{
  // This tests the 7th bit, if 0, result is 0.
  // If 1, result is 255 (because of sign extended bit shift!).
  // Note the use of char constants to force 8bit calculation.
  *(p++) = *c << 24 >> 31;
  *(p++) = *c << 25 >> 31;
  *(p++) = *c << 26 >> 31;
  *(p++) = *c << 27 >> 31;
  *(p++) = *c << 28 >> 31;
  *(p++) = *c << 29 >> 31;
  *(p++) = *c << 30 >> 31;
  *(p++) = *c << 31 >> 31;
  return p;
}

static inline void gl2_do_remap_charsets(void)
{
  signed char *c = (signed char *)graphics.charset;
  char *p = (char *)gl_state.charset_texture;
  unsigned int i, j, k;

  for (i = 0; i < 16; i++, c += -14 + 32 * 14)
    for (j = 0; j < 14; j++, c += -32 * 14 + 1)
      for (k = 0; k < 32; k++, c += 14)
        p = gl2_char_bitmask_to_texture(c, p);

  gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32 * 8, 16 * 14, GL_ALPHA,
    GL_UNSIGNED_BYTE, gl_state.charset_texture);
}

static void gl2_remap_charsets(void)
{
  gl_state.remap_texture = true;
}

static int gl2_linear_filter_method(void)
{
  if (gl_state.ignore_linear)
    return false;
  return (strcmp(graphics.gl_filter_method, "linear") == 0);
}

// FIXME: Many magic numbers
void gl2_resize_screen(int viewport_width, int viewport_height)
{
  // this overrides the user's setting if they choose linear
  if (viewport_width < 640 || viewport_height < 350)
    gl_state.ignore_linear = true;
  else
    gl_state.ignore_linear = false;

  gl.glViewport(0, 0, viewport_width, viewport_height);

  gl.glMatrixMode(GL_PROJECTION);
  gl.glLoadIdentity();

  gl.glFrustum(-320, 320, 175, -175, 1, 100);

  gl.glMatrixMode(GL_MODELVIEW);
  gl.glLoadIdentity();

  gl.glTranslatef(-320, -175, 0);

  gl.glGenTextures(2, gl_state.texture_number);

  gl.glBindTexture(GL_TEXTURE_2D, gl_state.texture_number[0]);

  gl_set_filter_method(graphics.gl_filter_method);

  gl.glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl.glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  gl.glEnable(GL_TEXTURE_2D);

  gl.glEnable(GL_BLEND);

  gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  SDL_LockSurface(graphics.screen);

    SDL_FillRect(graphics.screen, NULL, ~0);

    gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GL_POWER_2_WIDTH,
      GL_POWER_2_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE,
      graphics.screen->pixels);

  SDL_UnlockSurface(graphics.screen);

  gl.glBindTexture(GL_TEXTURE_2D, gl_state.texture_number[1]);

  gl_set_filter_method(graphics.gl_filter_method);

  gl.glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl.glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  SDL_LockSurface(graphics.screen);

    gl.glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 256, 256, 0, GL_ALPHA,
      GL_UNSIGNED_BYTE, graphics.screen->pixels);

  SDL_UnlockSurface(graphics.screen);

  gl2_remap_charsets();
}

static int gl2_init_video(config_info *conf)
{
  graphics.allow_resize = conf->allow_resize;
  graphics.gl_filter_method = conf->gl_filter_method;
  graphics.bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if (conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics.bits_per_pixel = conf->force_bpp;

  // We want to deal internally with 32bit surfaces
  graphics.screen = SDL_CreateRGBSurface(SDL_SWSURFACE, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 32, 0, 0, 0, 0);

  if (!set_video_mode())
    return false;

  // NOTE: This must come AFTER set_video_mode()!
  const char *version = (const char *)gl.glGetString(GL_VERSION);

  // we need a specific "version" of OpenGL compatibility
  if (version && atof(version) < 1.1)
  {
    fprintf(stderr, "Your OpenGL implementation is too old (need v1.1).\n");
    return false;
  }

  return true;
}

static void gl2_update_screen(void)
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  char_element *src = graphics.text_video;
  Uint8 *char_ptr;
  Uint32 char_colors[4];
  Uint32 current_char_byte;
  Uint8 current_color;
  Uint32 i, i2, i3;
  Sint32 i4;
  Uint32 line_advance = 640;
  Uint32 line_advance_sub;
  Uint32 row_advance = 8960;
  Uint32 ticks = SDL_GetTicks();
  Uint32 *old_dest = NULL;

  SDL_LockSurface(graphics.screen);

  dest = (Uint32 *)graphics.screen->pixels;

  line_advance_sub = line_advance - 8;

  old_dest = dest;

  if((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  if(!graphics.screen_mode)
  {
    if (gl_state.remap_texture)
    {
      gl2_do_remap_charsets();
      gl_state.remap_texture = false;
    }

    if (gl2_linear_filter_method())
      gl.glViewport(0, 0, 640, 350);

    gl.glDisable(GL_TEXTURE_2D);
    gl.glDisable(GL_BLEND);

    gl.glBegin(GL_QUADS);
      for(i = 0; i < 350; i = i + 14)
      {
        for(i2 = 0; i2 < 640; i2 = i2 + 8)
        {
          gl.glColor3ubv(&gl_state.palette[src->bg_color *3]);
          gl.glVertex3i(i2,     i + 14, -1);
          gl.glVertex3i(i2,     i,      -1);
          gl.glVertex3i(i2 + 8, i,      -1);
          gl.glVertex3i(i2 + 8, i + 14, -1);
          src++;
        }
      }
    gl.glEnd();

    gl.glEnable(GL_TEXTURE_2D);
    gl.glEnable(GL_BLEND);

    gl.glBegin(GL_QUADS);
      src = graphics.text_video;
      for(i = 0; i < 350; i = i + 14)
      {
        for(i2 = 0; i2 < 640; i2 = i2 + 8)
        {
          gl.glColor3ubv(&gl_state.palette[src->fg_color * 3]);

          gl.glTexCoord2f(
            ((src->char_value % 32) + 1) * 0.03125   - SAFE_TEXTURE_MARGIN_X,
            ((src->char_value / 32) + 1) * 0.0546875 - SAFE_TEXTURE_MARGIN_Y
          );
          gl.glVertex3i(i2 + 8,i + 14, -1);

          gl.glTexCoord2f(
            ((src->char_value % 32) + 1) * 0.03125   - SAFE_TEXTURE_MARGIN_X,
             (src->char_value / 32)      * 0.0546875 + SAFE_TEXTURE_MARGIN_Y
          );
          gl.glVertex3i(i2 + 8, i, -1);

          gl.glTexCoord2f(
             (src->char_value % 32)      * 0.03125   + SAFE_TEXTURE_MARGIN_X,
             (src->char_value / 32)      * 0.0546875 + SAFE_TEXTURE_MARGIN_Y
          );
          gl.glVertex3i(i2, i, -1);

          gl.glTexCoord2f(
             (src->char_value % 32)      * 0.03125   + SAFE_TEXTURE_MARGIN_X,
            ((src->char_value / 32) + 1) * 0.0546875 - SAFE_TEXTURE_MARGIN_Y
          );
          gl.glVertex3i(i2, i + 14, -1);
          src++;
        }
      }
  }
  else
  {
    if (graphics.screen_mode != 3)
    {
      Uint32 cb_bg, cb_fg;
      Uint32 c_color;

      for(i = 0; i < 25; i++)
      {
        ldest2 = dest;
        for(i2 = 0; i2 < 80; i2++)
        {
          ldest = dest;
          cb_bg = (src->bg_color & 0x0F);
          cb_fg = (src->fg_color & 0x0F);
          char_colors[0] =
           graphics.flat_intensity_palette[(cb_bg << 4) | cb_bg];
          char_colors[1] =
           graphics.flat_intensity_palette[(cb_bg << 4) | cb_fg];
          char_colors[2] =
           graphics.flat_intensity_palette[(cb_fg << 4) | cb_bg];
          char_colors[3] =
           graphics.flat_intensity_palette[(cb_fg << 4) | cb_fg];

          // Fill in foreground color
          char_ptr = graphics.charset + (src->char_value * 14);
          src++;

          for(i3 = 0; i3 < 14; i3++)
          {
            current_char_byte = *char_ptr;
            char_ptr++;

            for(i4 = 6; i4 >= 0; i4 -= 2, dest += 2)
            {
              c_color = char_colors[(current_char_byte >> i4) & 0x03];
              *dest = c_color;
              *(dest + 1) = c_color;
            }
            dest += line_advance_sub;
          }
          dest = ldest + 8;
        }
        dest = ldest2 + row_advance;
      }
    }
    else
    {
      Uint32 c_color;

      for(i = 0; i < 25; i++)
      {
        ldest2 = dest;
        for(i2 = 0; i2 < 80; i2++)
        {
          ldest = dest;
          current_color = (src->bg_color << 4) | (src->fg_color & 0x0F);

          // Fill in background color
          char_colors[0] =
           graphics.flat_intensity_palette[current_color];
          char_colors[1] =
           graphics.flat_intensity_palette[(current_color + 2) & 0xFF];
          char_colors[2] =
           graphics.flat_intensity_palette[(current_color + 1) & 0xFF];
          char_colors[3] =
           graphics.flat_intensity_palette[(current_color + 3) & 0xFF];

          // Fill in foreground color
          char_ptr = graphics.charset + (src->char_value * 14);
          src++;

          for(i3 = 0; i3 < 14; i3++)
          {
            current_char_byte = *char_ptr;
            char_ptr++;

            for(i4 = 6; i4 >= 0; i4 -= 2, dest += 2)
            {
              c_color = char_colors[(current_char_byte >> i4) & 0x03];
              *dest = c_color;
              *(dest + 1) = c_color;
            }
            dest += line_advance_sub;
          }
          dest = ldest + 8;
        }
        dest = ldest2 + row_advance;
      }
    }
  }

  if (graphics.screen_mode)
  {
    gl.glBindTexture(GL_TEXTURE_2D, gl_state.texture_number[0]);

    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 350, GL_BGRA,
      GL_UNSIGNED_BYTE, graphics.screen->pixels);

    gl.glColor4f(1.0, 1.0, 1.0, 1.0);

    gl.glDisable(GL_BLEND);

    gl.glBegin(GL_QUADS);
      gl.glTexCoord2f(0, 0.68359375);
      gl.glVertex3i(0, 350, -1);
      gl.glTexCoord2f(0, 0);
      gl.glVertex3i(0, 0, -1);
      gl.glTexCoord2f(0.625, 0);
      gl.glVertex3i(640, 0, -1);
      gl.glTexCoord2f(0.625, 0.68359375);
      gl.glVertex3i(640, 350, -1);
    gl.glEnd();

    gl.glBindTexture(GL_TEXTURE_2D, gl_state.texture_number[1]);
    gl.glEnable(GL_BLEND);
    gl.glBegin(GL_QUADS);
  }

  if(graphics.mouse_status)
  {
    int mouse_x, mouse_y, mxb, myb, mw, mh;

    get_real_mouse_position(&mouse_x, &mouse_y);

    mxb = (mouse_x / graphics.mouse_width_mul) * graphics.mouse_width_mul;
    myb = (mouse_y / graphics.mouse_height_mul) * graphics.mouse_height_mul;

    mw = graphics.mouse_width_mul;
    mh = graphics.mouse_height_mul;

    gl.glColor4ub(0, 0, 0, 128);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(mxb,      myb + mh, -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(mxb,      myb,      -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(mxb + mw, myb,      -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(mxb + mw, myb + mh, -1);

    gl.glColor4ub(255, 255, 0, 255);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(mxb + 1,      myb + mh - 1, -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(mxb + 1,      myb + 1,      -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(mxb + mw - 1, myb + 1,      -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(mxb + mw - 1, myb + mh - 1, -1);
  }

  // Draw cursor perhaps
  if (graphics.cursor_flipflop &&
      (graphics.cursor_mode != cursor_mode_invisible))
  {
    char_element *cursor_element = graphics.text_video + graphics.cursor_x + 
                                   (graphics.cursor_y * 80);
    Uint32 cursor_color;
    Uint32 cursor_char = cursor_element->char_value;
    Uint32 lines = 0;
    Uint32 addy = 0;
    Uint32 *cursor_offset = old_dest;
    Uint32 i;
    Uint32 cursor_solid = 0xFFFFFFFF;
    Uint32 *char_offset = (Uint32 *)(graphics.charset + (cursor_char * 14));
    Uint32 bg_color = cursor_element->bg_color;
    cursor_offset += (graphics.cursor_x * 8) + (graphics.cursor_y *row_advance);

    // Choose FG
    cursor_color = cursor_element->fg_color;

    // See if the cursor char is completely solid or completely
    // empty
    for(i = 0; i < 3; i++)
    {
      cursor_solid &= *char_offset;
      char_offset++;
    }

    // Get the last bit
    cursor_solid &= (*((Uint16 *)char_offset)) | 0xFFFF0000;

    // Solid cursor, use BG instead
    if(cursor_solid == 0xFFFFFFFF)
    {
      // But wait! What if the background is the same as the foreground?
      // If so, use +8 instead.
      if(bg_color == cursor_color)
        cursor_color = (bg_color + 8) & 0x0F;
      else
        cursor_color = bg_color;
    }
    else
    {
      // What if the foreground is the same as the background?
      // It needs to flash +8 then
      if(bg_color == cursor_color)
        cursor_color = (cursor_color + 8) & 0x0F;
    }

    switch(graphics.cursor_mode)
    {
      case cursor_mode_underline:
        lines = 2;
        cursor_offset += (12 * line_advance);
        addy = 12;
        break;

      case cursor_mode_solid:
        lines = 14;
        break;

      case cursor_mode_invisible:
        // FIXME: Should we do nothing?
        break;
    }

    gl.glColor3ubv(&gl_state.palette[cursor_color * 3]);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(graphics.cursor_x * 8,     graphics.cursor_y * 14 + lines + addy, -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(graphics.cursor_x * 8,     graphics.cursor_y * 14 + addy,         -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(graphics.cursor_x * 8 + 8, graphics.cursor_y * 14 + addy,         -1);
    gl.glTexCoord2f(0, 0.8755);
    gl.glVertex3i(graphics.cursor_x * 8 + 8, graphics.cursor_y * 14 + lines + addy, -1);
  }

  gl.glEnd();

  //If you want linear filtering, you need to set the viewport to 640, 350 before rendering. 
  //Then, bind texture_number[0],
  //use glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 640, 350, 0);
  //set the viewport back to the original resolution,
  //display it on a quad.
  if (gl2_linear_filter_method() && !graphics.screen_mode)
  {
    gl.glBindTexture(GL_TEXTURE_2D, gl_state.texture_number[0]);
    gl.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1024, 512, 0);

    if(!graphics.fullscreen)
      gl.glViewport(0, 0, graphics.window_width, graphics.window_height);
    else
      gl.glViewport(0, 0, graphics.resolution_width,graphics.resolution_height);

    gl.glColor4f(1.0, 1.0, 1.0, 1.0);
    gl.glDisable(GL_BLEND);

    gl.glBegin(GL_QUADS);
      gl.glTexCoord2f(0,0.68359375);
      gl.glVertex3i(0,0,-1);
      gl.glTexCoord2f(0,0);
      gl.glVertex3i(0,350,-1);
      gl.glTexCoord2f(0.625,0);
      gl.glVertex3i(640,350,-1);
      gl.glTexCoord2f(0.625,0.68359375);
      gl.glVertex3i(640,0,-1);
    gl.glEnd();

    gl.glBindTexture(GL_TEXTURE_2D, gl_state.texture_number[1]);
  }

  SDL_UnlockSurface(graphics.screen);

  SDL_GL_SwapBuffers();

  gl.glClear(GL_COLOR_BUFFER_BIT);
}

static int gl2_set_video_mode(int width, int height, int depth, int flags,
                              int fullscreen)
{
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  if (!SDL_SetVideoMode(width, height, depth, gl_strip_flags(flags)))
    return false;

  if (!load_gl_syms())
    return false;

  gl2_resize_screen(width, height);

  return true;
}

static void gl2_update_colors(SDL_Color *palette, Uint32 count)
{
  for (Uint32 i = 0; i < count; i++)
  {
    graphics.flat_intensity_palette[i] = SDL_MapRGBA(graphics.screen->format,
      palette[i].r, palette[i].g, palette[i].b, 255) | 0xff000000;
    gl_state.palette[i*3  ] = (GLubyte)palette[i].r;
    gl_state.palette[i*3+1] = (GLubyte)palette[i].g;
    gl_state.palette[i*3+2] = (GLubyte)palette[i].b;
  }
}

#endif // CONFIG_OPENGL && !PSP_BUILD

#if !defined(PSP_BUILD)

/* COMMON YUV RENDERER CODE **************************************************/

#define SOFT_SOURCE_WIDTH	640
#define SOFT_SOURCE_HEIGHT	350

#define YUV1_OVERLAY_WIDTH	(SOFT_SOURCE_WIDTH * 2)
#define YUV1_OVERLAY_HEIGHT	SOFT_SOURCE_HEIGHT

#define YUV2_OVERLAY_WIDTH	SOFT_SOURCE_WIDTH
#define YUV2_OVERLAY_HEIGHT	SOFT_SOURCE_HEIGHT

// Optimized for packed-pixel 4:2:2 YUV [No height multiplier]

static void update_screen_yuv(Uint32 *pixels, Uint32 pitch, Uint32 w, Uint32 h,
                              Uint32 y1)
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  Uint32 cb_bg;
  Uint32 cb_fg;
  Uint32 y_bg;
  Uint32 y_fg;
  char_element *src = graphics.text_video;
  Uint8 *char_ptr;
  Uint32 char_colors[4];
  Uint32 current_char_byte;
  Uint8 current_color;
  Uint32 i, i2, i3;
  Uint32 line_advance = pitch / 4;
  Uint32 row_advance = (pitch * 14) / 4;
  Uint32 ticks = SDL_GetTicks();
  Uint32 *old_dest = NULL;

  dest = (Uint32 *)(pixels) + (pitch * ((h - 350) / 4)) + ((w - 640) / 4);

  old_dest = dest;

  if ((ticks - graphics.cursor_timestamp) > CURSOR_BLINK_RATE)
  {
    graphics.cursor_flipflop ^= 1;
    graphics.cursor_timestamp = ticks;
  }

  if (!graphics.screen_mode)
  {
    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for (i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        // Fill in background color?
        cb_bg = graphics.flat_intensity_palette[src->bg_color];
        cb_fg = graphics.flat_intensity_palette[src->fg_color];
        y_bg = ((Uint8*)(graphics.flat_intensity_palette + src->bg_color))[y1];
        y_fg = ((Uint8*)(graphics.flat_intensity_palette + src->fg_color))[y1];

        char_colors[0] = cb_bg;
        char_colors[1] = cb_bg;
        char_colors[2] = cb_fg;
        char_colors[3] = cb_fg;
        ((Uint8*)(char_colors + 0))[y1] = y_bg;
        ((Uint8*)(char_colors + 1))[y1] = y_fg;
        ((Uint8*)(char_colors + 2))[y1] = y_bg;
        ((Uint8*)(char_colors + 3))[y1] = y_fg;

        // Fill in foreground color?
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 6];
          *(dest + 1) = char_colors[(current_char_byte >> 4) & 0x03];
          *(dest + 2) = char_colors[(current_char_byte >> 2) & 0x03];
          *(dest + 3) = char_colors[current_char_byte & 0x03];
          dest += line_advance;
        }
        dest = ldest + 4;
      }
      dest = ldest2 + row_advance;
    }
  }
  else

  if(graphics.screen_mode != 3)
  {
    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for(i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;

        cb_bg = src->bg_color & 0x0F;
        cb_fg = src->fg_color & 0x0F;

        char_colors[0] = graphics.flat_intensity_palette[(cb_bg << 4) | cb_bg];
        char_colors[1] = graphics.flat_intensity_palette[(cb_bg << 4) | cb_fg];
        char_colors[2] = graphics.flat_intensity_palette[(cb_fg << 4) | cb_bg];
        char_colors[3] = graphics.flat_intensity_palette[(cb_fg << 4) | cb_fg];

        // Fill in foreground color? What do these comments mean?!
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 6];
          *(dest + 1) = char_colors[(current_char_byte >> 4) & 0x03];
          *(dest + 2) = char_colors[(current_char_byte >> 2) & 0x03];
          *(dest + 3) = char_colors[current_char_byte & 0x03];
          dest += line_advance;
        }
        dest = ldest + 4;
      }
      dest = ldest2 + row_advance;
    }
  }
  else
  {
    for(i = 0; i < 25; i++)
    {
      ldest2 = dest;
      for (i2 = 0; i2 < 80; i2++)
      {
        ldest = dest;
        current_color = (src->bg_color << 4) | (src->fg_color & 0x0F);
        // Fill in background color? I wonder why this comment was missing in
        // the mode 1/2 renderer.

        char_colors[0] = graphics.flat_intensity_palette[current_color];
        char_colors[1] = graphics.flat_intensity_palette[(current_color + 2) &
         0xFF];
        char_colors[2] = graphics.flat_intensity_palette[(current_color + 1) &
         0xFF];
        char_colors[3] = graphics.flat_intensity_palette[(current_color + 3) &
         0xFF];

        // Fill in foreground color. These comments don't make sense. :(
        char_ptr = graphics.charset + (src->char_value * 14);
        src++;
        for(i3 = 0; i3 < 14; i3++)
        {
          current_char_byte = *char_ptr;
          char_ptr++;
          *dest = char_colors[current_char_byte >> 6];
          *(dest + 1) = char_colors[(current_char_byte >> 4) & 0x03];
          *(dest + 2) = char_colors[(current_char_byte >> 2) & 0x03];
          *(dest + 3) = char_colors[current_char_byte & 0x03];
          dest += line_advance;
        }
        dest = ldest + 4;
      }
      dest = ldest2 + row_advance;
    }
  }

  if(graphics.mouse_status)
  {
    int mouse_x, mouse_y;
    int current_colors;
    get_real_mouse_position(&mouse_x, &mouse_y);

    mouse_x = (mouse_x / graphics.mouse_width_mul) * graphics.mouse_width_mul;
    mouse_y = (mouse_y / graphics.mouse_height_mul) * graphics.mouse_height_mul;

    dest = old_dest + ((mouse_x / 2) + mouse_y * line_advance);

    for(i = 0; i < graphics.mouse_height_mul; i++)
    {
      ldest = dest;
      for(i2 = 0; i2 < (graphics.mouse_width_mul / 2); i2++, dest++)
      {
        current_colors = *dest;
        *dest = current_colors ^ 0xFFFFFFFF;
      }
      dest = ldest + line_advance;
    }
  }

  // Draw cursor perhaps
  if(graphics.cursor_flipflop && (graphics.cursor_mode!=cursor_mode_invisible))
  {
    char_element *cursor_element = graphics.text_video + graphics.cursor_x +
     (graphics.cursor_y * 80);
    Uint32 cursor_color;
    Uint32 cursor_char = cursor_element->char_value;
    Uint32 lines = 0;
    Uint32 *cursor_offset = old_dest;
    Uint32 i;
    Uint32 cursor_solid = 0xFFFFFFFF;
    Uint32 *char_offset = (Uint32 *)(graphics.charset + (cursor_char * 14));
    Uint32 bg_color = cursor_element->bg_color;
    cursor_offset += (graphics.cursor_x * 4) + (graphics.cursor_y *
     row_advance);

    // Choose FG
    cursor_color = cursor_element->fg_color;

    // See if the cursor char is completely solid or completely
    // empty
    for(i = 0; i < 3; i ++)
    {
      cursor_solid &= *char_offset;
      char_offset++;
    }
    cursor_solid &= (*((Uint16 *)char_offset)) | 0xFFFF0000;
    if (cursor_solid == 0xFFFFFFFF)
    {
      // But wait! What if the background is the same as the foreground?
      // If so, use +8 instead.
      if(bg_color == cursor_color)
      {
        cursor_color = (bg_color + 8) & 0x0F;
      }
      else
      {
        cursor_color = bg_color;
      }
    }
    else
    {
      // What if the foreground is the same as the background?
      // It needs to flash +8 then
      if (bg_color == cursor_color)
      {
        cursor_color = (cursor_color + 8) & 0x0F;
      }
    }

    cursor_color = graphics.flat_intensity_palette[cursor_color];

    switch(graphics.cursor_mode)
    {
      case cursor_mode_underline:
        lines = 2;
        cursor_offset += (12 * line_advance);
        break;

      case cursor_mode_solid:
        lines = 14;
        break;

      default:
        break;
    }

    for(i = 0; i < lines; i++)
    {
      *cursor_offset = cursor_color;
      *(cursor_offset + 1) = cursor_color;
      *(cursor_offset + 2) = cursor_color;
      *(cursor_offset + 3) = cursor_color;
      cursor_offset += line_advance;
    }
  }

  // Line multiplier would go here
}

static int yuv_set_video_mode_size(int width, int height, int depth, int flags,
                                   int fullscreen, int yuv_width,
                                   int yuv_height)
{
  // the YUV renderer _requires_ 32bit colour
  graphics.screen = SDL_SetVideoMode(width, height, 32, flags | SDL_ANYFORMAT);

  if (!graphics.screen)
    return false;

  // try with a YUY2 pixel format first
  graphics.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
    SDL_YUY2_OVERLAY, graphics.screen);

  // didn't work, try with a UYVY pixel format next
  if (!graphics.overlay)
    graphics.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
      SDL_UYVY_OVERLAY, graphics.screen);

  // failed to create an overlay
  if (!graphics.overlay)
    return false;

  return true;
}

static inline void rgb_to_yuv(Uint8 r, Uint8 g, Uint8 b,
                              Uint8 *y, Uint8 *u, Uint8 *v)
{
  *y = (9797 * r + 19237 * g + 3734 * b) >> 15;
  *u = ((18492 * (b - *y)) >> 15) + 128;
  *v = ((23372 * (r - *y)) >> 15) + 128;
}

/* YUV RENDERER #1 CODE ******************************************************/

static int yuv1_init_video(config_info *conf)
{
  graphics.allow_resize = conf->allow_resize;
  return set_video_mode();
}

static int yuv1_check_video_mode(int width, int height, int depth, int flags)
{
  // requires 32bit colour
  return SDL_VideoModeOK(width, height, 32, flags | SDL_ANYFORMAT);
}

static int yuv1_set_video_mode(int width, int height, int depth, int flags,
                                int fullscreen)
{
  return yuv_set_video_mode_size(width, height, depth, flags, fullscreen,
                                 YUV1_OVERLAY_WIDTH, YUV1_OVERLAY_HEIGHT);
}

static void yuv1_update_screen(void)
{
  SDL_Rect rect = { 0, 0, graphics.screen->w, graphics.screen->h };

  SDL_LockYUVOverlay(graphics.overlay);

  // FIXME: yuv2 has special cases here?
  update_screen32((Uint32*)graphics.overlay->pixels[0],
   graphics.overlay->pitches[0], SOFT_SOURCE_WIDTH, SOFT_SOURCE_HEIGHT);

  SDL_UnlockYUVOverlay(graphics.overlay);

  SDL_DisplayYUVOverlay(graphics.overlay, &rect);
}

static void yuv1_update_colors(SDL_Color *palette, Uint32 count)
{
  Uint8 y, u, v;
  Uint32 i;

  // it's a YUY2 overlay
  if (graphics.overlay->format == SDL_YUY2_OVERLAY)
  {
    for (i = 0; i < count; i++)
    {
      rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b, &y, &u, &v);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
      graphics.flat_intensity_palette[i] =
        (v << 0) | (y << 8) | (u << 16) | (y << 24);
#else
      graphics.flat_intensity_palette[i] =
        (y << 0) | (u << 8) | (y << 16) | (v << 24);
#endif
    }
  }

  // it's a UYVY overlay
  else
  {
    for (i = 0; i < count; i++)
    {
      rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b, &y, &u, &v);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
      graphics.flat_intensity_palette[i] =
        (y << 0) | (v << 8) | (y << 16) | (u << 24);
#else
      graphics.flat_intensity_palette[i] =
        (u << 0) | (y << 8) | (v << 16) | (y << 24);
#endif
    }
  }
}

/* YUV RENDERER #2 CODE ******************************************************/

static int yuv2_set_video_mode(int width, int height, int depth, int flags,
                               int fullscreen)
{
  return yuv_set_video_mode_size(width, height, depth, flags, fullscreen,
                                 YUV2_OVERLAY_WIDTH, YUV2_OVERLAY_HEIGHT);
}

static void yuv2_update_screen(void)
{
  SDL_Rect rect = { 0, 0, graphics.screen->w, graphics.screen->h };

  SDL_LockYUVOverlay(graphics.overlay);

  if (graphics.overlay->format == SDL_YUY2_OVERLAY)
  {
    update_screen_yuv((Uint32*)graphics.overlay->pixels[0],
     graphics.overlay->pitches[0], YUV2_OVERLAY_WIDTH, YUV2_OVERLAY_HEIGHT, 2);
  }
  else
  {
    update_screen_yuv((Uint32*)graphics.overlay->pixels[0],
     graphics.overlay->pitches[0], YUV2_OVERLAY_WIDTH, YUV2_OVERLAY_HEIGHT, 3);
  }

  SDL_UnlockYUVOverlay(graphics.overlay);

  SDL_DisplayYUVOverlay(graphics.overlay, &rect);
}

#endif // PSP_BUILD

/* RENDERER NEUTRAL CODE *****************************************************/

static void set_graphics_output(char *video_output)
{
  char *selected_output = "software";

  // Default software renderer
  graphics.init_video = soft_init_video;
  graphics.check_video_mode = soft_check_video_mode;
  graphics.set_video_mode = soft_set_video_mode;
  graphics.update_screen = soft_update_screen;
  graphics.update_colors = soft_update_colors;
  graphics.resize_screen = soft_resize_screen;
  graphics.set_screen_coords = soft_set_screen_coords;
  graphics.remap_charsets = NULL;

#if defined(CONFIG_OPENGL) && !defined(PSP_BUILD)

  // Try to load system GL. if we succeed..
  if (can_use_opengl())
  {
    if (!strcmp(video_output, "opengl1"))
    {
      graphics.init_video = gl1_init_video;
      graphics.check_video_mode = gl1_check_video_mode;
      graphics.set_video_mode = gl1_set_video_mode;
      graphics.update_screen = gl1_update_screen;
      graphics.update_colors = gl1_update_colors;
      graphics.set_screen_coords = hard_set_screen_coords;
      selected_output = video_output;
    }

    if (!strcmp(video_output, "opengl2"))
    {
      graphics.init_video = gl2_init_video;
      graphics.check_video_mode = gl1_check_video_mode;
      graphics.set_video_mode = gl2_set_video_mode;
      graphics.update_screen = gl2_update_screen;
      graphics.update_colors = gl2_update_colors;
      graphics.resize_screen = gl2_resize_screen;
      graphics.remap_charsets = gl2_remap_charsets;
      graphics.set_screen_coords = hard_set_screen_coords;
      selected_output = video_output;
    }
  }

#ifdef DEBUG
  else
  {
    fprintf(stderr, "Platform GL support compiled in, but GL not found!\n");
  }
#endif

#endif // CONFIG_OPENGL && !PSP_BUILD

#if !defined(PSP_BUILD)
  // YUV overlay renderer
  if (!strcmp(video_output, "overlay1"))
  {
    graphics.init_video = yuv1_init_video;
    graphics.check_video_mode = yuv1_check_video_mode;
    graphics.set_video_mode = yuv1_set_video_mode;
    graphics.update_screen = yuv1_update_screen;
    graphics.update_colors = yuv1_update_colors;
    graphics.set_screen_coords = hard_set_screen_coords;
    selected_output = video_output;
  }
#endif

#if !defined(PSP_BUILD)
  // Low res YUV overlay renderer
  if (!strcmp(video_output, "overlay2"))
  {
    graphics.init_video = yuv1_init_video;
    graphics.check_video_mode = yuv1_check_video_mode;
    graphics.set_video_mode = yuv2_set_video_mode;
    graphics.update_screen = yuv2_update_screen;
    graphics.update_colors = yuv1_update_colors;
    graphics.set_screen_coords = hard_set_screen_coords;
    selected_output = video_output;
  }
#endif

#ifdef DEBUG
  fprintf(stdout, "Selected video output: %s\n", selected_output);
#endif
}

void init_video(config_info *conf)
{
  char temp[64];

  graphics.screen_mode = 0;
  graphics.fullscreen = conf->fullscreen;
  graphics.resolution_width = conf->resolution_width;
  graphics.resolution_height = conf->resolution_height;
  graphics.window_width = conf->window_width;
  graphics.window_height = conf->window_height;
  graphics.mouse_status = 0;
  graphics.cursor_timestamp = SDL_GetTicks();
  graphics.cursor_flipflop = 1;

  set_graphics_output(conf->video_output);

  if (!(graphics.init_video(conf)))
  {
    set_graphics_output("");
    graphics.init_video(conf);
  }

  sprintf(temp, "MegaZeux %s", version_number_string);
  SDL_WM_SetCaption(temp, "MZX");
  SDL_ShowCursor(SDL_DISABLE);

  ec_load_set_secondary(MZX_DEFAULT_CHR, graphics.default_charset);
  ec_load_set_secondary(MZX_BLANK_CHR, graphics.blank_charset);
  ec_load_set_secondary(MZX_SMZX_CHR, graphics.smzx_charset);
  ec_load_set_secondary(MZX_ASCII_CHR, graphics.ascii_charset);
  ec_load_set_secondary(MZX_EDIT_CHR, graphics.charset + (256 * 14));
  ec_load_mzx();

  init_palette();
}

int set_video_mode(void)
{
  int target_width, target_height;
  int target_depth = graphics.bits_per_pixel;
  int target_flags = 0;
  int fullscreen = graphics.fullscreen;

  if(fullscreen)
  {
    target_width = graphics.resolution_width;
    target_height = graphics.resolution_height;
    target_flags |= SDL_FULLSCREEN;

    if(target_depth == 8)
      target_flags |= SDL_HWPALETTE;
  }
  else
  {
    if(graphics.allow_resize)
      target_flags |= SDL_RESIZABLE;

    target_width = graphics.window_width;
    target_height = graphics.window_height;
  }

#ifdef PSP_BUILD
  target_width = 640;
  target_height = 363;
  target_depth = 8;
  fullscreen = 1;
#endif

  // If video mode fails, replace it with 'safe' defaults
  if (!(graphics.check_video_mode(target_width, target_height, target_depth,
                                  target_flags)))
  {
    target_width = 640;
    target_height = 350;
    target_depth = 8;
    fullscreen = 0;

    graphics.resolution_width = target_width;
    graphics.resolution_height = target_height;
    graphics.bits_per_pixel = target_depth;
    graphics.fullscreen = fullscreen;

    target_flags = SDL_SWSURFACE;
  }

  return graphics.set_video_mode(target_width, target_height, target_depth,
                                 target_flags, fullscreen);
}

void toggle_fullscreen()
{
  graphics.fullscreen ^= 1;
  set_video_mode();
  update_screen();
  update_palette();
}

void resize_screen(Uint32 w, Uint32 h)
{
  if(!graphics.fullscreen && graphics.allow_resize)
  {
    graphics.window_width = w;
    graphics.window_height = h;
    set_video_mode();
    graphics.resize_screen(w, h);
  }
}

void color_string(char *string, Uint32 x, Uint32 y, Uint8 color)
{
  color_string_ext(string, x, y, color, 256, 16);
}

void write_string(char *string, Uint32 x, Uint32 y, Uint8 color,
 Uint32 tab_allowed)
{
  write_string_ext(string, x, y, color, tab_allowed, 256, 16);
}

void write_line(char *string, Uint32 x, Uint32 y, Uint8 color,
 Uint32 tab_allowed)
{
  write_line_ext(string, x, y, color, tab_allowed, 256, 16);
}

void color_line(Uint32 length, Uint32 x, Uint32 y, Uint8 color)
{
  color_line_ext(length, x, y, color, 256, 16);
}

void fill_line(Uint32 length, Uint32 x, Uint32 y, Uint8 chr,
 Uint8 color)
{
  fill_line_ext(length, x, y, chr, color, 256, 16);
}
void draw_char(Uint8 chr, Uint8 color, Uint32 x, Uint32 y)
{
  draw_char_ext(chr, color, x, y, 256, 16);
}

void draw_char_linear(Uint8 color, Uint8 chr, Uint32 offset)
{
  draw_char_linear_ext(color, chr, offset, 256, 16);
}

void draw_char_nocolor(Uint8 chr, Uint32 x, Uint32 y)
{
  draw_char_nocolor_ext(chr, x, y, 256, 16);
}

void color_string_ext(char *str, Uint32 x, Uint32 y, Uint8 color,
 Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;

  char next_str[2];
  next_str[1] = 0;

  while(cur_char)
  {
    switch(cur_char)
    {
      // Newline
      case '\n':
      {
        y++;
        dest = graphics.text_video + (y * 80) + x;
        break;
      }
      // Color character
      case '@':
      {
        str++;
        next = *str;

        // If 0, stop right there
        if(!next)
          return;

        // If the next isn't hex, count as one
        if(isxdigit(next))
        {
          next_str[0] = next;
          bg_color = strtol(next_str, NULL, 16) + c_offset;
        }
        else
        {
          if(next == '@')
          {
            dest->char_value = '@' + offset;
            dest->bg_color = bg_color;
            dest->fg_color = fg_color;
            dest++;
          }
          else
          {
            str--;
          }
        }

        break;
      }
      case '~':
      {
        str++;
        next = *str;

        // If 0, stop right there
        if(!next)
          return;

        // If the next isn't hex, count as one
        if(isxdigit(next))
        {
          next_str[0] = next;
          fg_color = strtol(next_str, NULL, 16) + c_offset;
        }
        else
        {
          if(next == '~')
          {
            dest->char_value = '~' + offset;
            dest->bg_color = bg_color;
            dest->fg_color = fg_color;
            dest++;
          }
          else
          {
            str--;
          }
        }

        break;
      }
      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    if(dest >= graphics.text_video + (80 * 25))
      break;

    str++;
    cur_char = *str;
  }
}

// Write a normal string

void write_string_ext(char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next_str[2];
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  next_str[1] = 0;

  while(cur_char && (cur_char != 0))
  {
    switch(cur_char)
    {
      // Newline
      case '\n':
      {
        y++;
        dest = graphics.text_video + (y * 80) + x;
        break;
      }
      case '\t':
      {
        if(tab_allowed)
        {
          dest += 5;
          break;
        }
      }
      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    if(dest >= graphics.text_video + (80 * 25))
      break;

    str++;
    cur_char = *str;
  }
}

void write_string_mask(char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next_str[2];
  Uint8 bg_color = (color >> 4) + 16;
  Uint8 fg_color = (color & 0x0F) + 16;
  next_str[1] = 0;

  while(cur_char && (cur_char != 0))
  {
    switch(cur_char)
    {
      // Newline
      case '\n':
      {
        y++;
        dest = graphics.text_video + (y * 80) + x;
        break;
      }
      case '\t':
      {
        if(tab_allowed)
        {
          dest += 5;
          break;
        }
      }
      default:
      {
        if((cur_char >= 32) && (cur_char <= 127))
          dest->char_value = cur_char + 256;
        else
          dest->char_value = cur_char;

        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    if(dest >= graphics.text_video + (80 * 25))
      break;

    str++;
    cur_char = *str;
  }
}

// Write a normal string, without carriage returns

void write_line_ext(char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed, Uint32 offset,
 Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next_str[2];
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  next_str[1] = 0;

  while(cur_char && (cur_char != '\n'))
  {
    switch(cur_char)
    {
      // Color character
      case '\t':
      {
        if(tab_allowed)
        {
          dest += 10;
          break;
        }
      }
      default:
      {
        dest->char_value = cur_char + offset;
        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    str++;
    cur_char = *str;
  }
}

void write_line_mask(char *str, Uint32 x, Uint32 y,
 Uint8 color, Uint32 tab_allowed)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  char *src = str;
  Uint8 cur_char = *src;
  Uint8 next_str[2];
  Uint8 bg_color = (color >> 4) + 16;
  Uint8 fg_color = (color & 0x0F) + 16;
  next_str[1] = 0;

  while(cur_char && (cur_char != '\n'))
  {
    switch(cur_char)
    {
      // Color character
      case '\t':
      {
        if(tab_allowed)
        {
          dest += 10;
          break;
        }
      }
      default:
      {
        if((cur_char >= 32) && (cur_char <= 127))
          dest->char_value = cur_char + 256;
        else
          dest->char_value = cur_char;

        dest->bg_color = bg_color;
        dest->fg_color = fg_color;
        dest++;
      }
    }
    str++;
    cur_char = *str;
  }
}

void color_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 color, Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  Uint32 i;

  for(i = 0; i < length; i++)
  {
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    dest++;
  }
}

void fill_line_ext(Uint32 length, Uint32 x, Uint32 y,
 Uint8 chr, Uint8 color, Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  Uint8 bg_color = (color >> 4) + c_offset;
  Uint8 fg_color = (color & 0x0F) + c_offset;
  Uint32 i;

  for(i = 0; i < length; i++)
  {
    dest->char_value = chr + offset;
    dest->bg_color = bg_color;
    dest->fg_color = fg_color;
    dest++;
  }
}

void draw_char_ext(Uint8 chr, Uint8 color, Uint32 x,
 Uint32 y, Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  dest->char_value = chr + offset;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
}

void draw_char_nocolor_ext(Uint8 chr, Uint32 x, Uint32 y,
 Uint32 offset, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  dest->char_value = chr + offset;
}

void draw_char_linear_ext(Uint8 color, Uint8 chr,
 Uint32 offset, Uint32 offset_b, Uint32 c_offset)
{
  char_element *dest = graphics.text_video + offset;
  dest->char_value = chr + offset_b;
  dest->bg_color = (color >> 4) + c_offset;
  dest->fg_color = (color & 0x0F) + c_offset;
}

Uint8 get_char(Uint32 x, Uint32 y)
{
  char_element *dest = graphics.text_video + (y * 80) + x;
  return dest->char_value;
}

Uint8 get_char_linear(Uint32 offset)
{
  char_element *dest = graphics.text_video + offset;
  return dest->char_value;
}

Uint8 get_color_linear(Uint32 offset)
{
  char_element *dest = graphics.text_video + offset;
  return (dest->bg_color << 4) | (dest->fg_color & 0x0F);
}

Uint8 get_bg_linear(Uint32 offset)
{
  char_element *dest = graphics.text_video + offset;
  return dest->bg_color;
}

void clear_screen(Uint8 chr, Uint8 color)
{
  Uint32 i;
  Uint8 fg_color = color & 0x0F;
  Uint8 bg_color = color >> 4;
  char_element *dest = graphics.text_video;

  for(i = 0; i < (80 * 25); i++)
  {
    dest->char_value = chr;
    dest->fg_color = fg_color;
    dest->bg_color = bg_color;
    dest++;
  }
  update_screen();
}

void clear_screen_no_update(Uint8 chr, Uint8 color)
{
  Uint32 i;
  Uint8 fg_color = color & 0x0F;
  Uint8 bg_color = color >> 4;
  char_element *dest = graphics.text_video;

  for(i = 0; i < (80 * 25); i++)
  {
    dest->char_value = chr;
    dest->fg_color = fg_color;
    dest->bg_color = bg_color;
    dest++;
  }
}

void set_screen(char_element *src)
{
  memcpy(graphics.text_video, src,
   80 * 25 * sizeof(char_element));
}

void get_screen(char_element *dest)
{
  memcpy(dest, graphics.text_video,
   80 * 25 * sizeof(char_element));
}

void update_screen()
{
  graphics.update_screen();
}

void cursor_underline(void)
{
  graphics.cursor_mode = cursor_mode_underline;
}

void cursor_solid(void)
{
  graphics.cursor_mode = cursor_mode_solid;
}

void cursor_off(void)
{
  graphics.cursor_mode = cursor_mode_invisible;
}

void move_cursor(Uint32 x, Uint32 y)
{
  graphics.cursor_x = x;
  graphics.cursor_y = y;
}

void set_cursor_mode(cursor_mode_types mode)
{
  graphics.cursor_mode = mode;
}

cursor_mode_types get_cursor_mode()
{
  return graphics.cursor_mode;
}

void ec_change_byte(Uint8 chr, Uint8 byte, Uint8 new_value)
{
  graphics.charset[(chr * 14) + byte] = new_value;

#ifdef CONFIG_OPENGL
  // FIXME: This should be a function pointer stub
  if (graphics.remap_charsets)
  {
    char *p = (char *)gl_state.charset_texture;
    signed char *c = (signed char *)graphics.charset + (chr * 14) + byte;

    gl2_char_bitmask_to_texture(c, p);

    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, (chr % 32) * 8, (chr / 32) * 14 + byte,
      8, 1, GL_ALPHA, GL_UNSIGNED_BYTE, gl_state.charset_texture);
  }
#endif // CONFIG_OPENGL
}

void ec_change_char(Uint8 chr, char *matrix)
{
  memcpy(graphics.charset + (chr * 14), matrix, 14);

#ifdef CONFIG_OPENGL
  // FIXME: This should be a function pointer stub
  if (graphics.remap_charsets)
  {
    unsigned int i;
    char *p = (char *)gl_state.charset_texture;
    signed char *c = (signed char *)graphics.charset;

    for (i = 0; i < 14; i++)
      p = gl2_char_bitmask_to_texture(c++, p);

    gl.glTexSubImage2D(GL_TEXTURE_2D, 0, (chr % 32) * 8, (chr / 32) * 14, 8,
      14, GL_ALPHA, GL_UNSIGNED_BYTE, gl_state.charset_texture);
  }
#endif // CONFIG_OPENGL
}

Uint8 ec_read_byte(Uint8 chr, Uint8 byte)
{
  return graphics.charset[(chr * 14) + byte];
}

void ec_read_char(Uint8 chr, char *matrix)
{
  memcpy(matrix, graphics.charset + (chr * 14), 14);
}

Sint32 ec_load_set(char *name)
{
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
   return -1;

  fread(graphics.charset, 14, 256, fp);

  fclose(fp);

  // some renderers may want to map charsets to textures
  if (graphics.remap_charsets)
    graphics.remap_charsets();

  return 0;
}

Sint32 ec_load_set_secondary(char *name, Uint8 *dest)
{
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
    return -1;

  fread(dest, 14, 256, fp);

  fclose(fp);

  // some renderers may want to map charsets to textures
  if (graphics.remap_charsets)
    graphics.remap_charsets();

  return 0;
}

Sint32 ec_load_set_var(char *name, Uint8 pos)
{
  Uint32 size = 256;
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
    return -1;

  fseek(fp, 0, SEEK_END);
  size = ftell(fp) / 14;
  fseek(fp, 0, 0);

  if(size + pos > 256)
    size = 256 - pos;

  fread(graphics.charset + (pos * 14), 14, size, fp);
  fclose(fp);

  // some renderers may want to map charsets to textures
  if (graphics.remap_charsets)
    graphics.remap_charsets();

  return size;
}

Sint32 ec_load_set_ext(char *name, Uint8 offset)
{
  Uint32 size = 256;
  FILE *fp = fopen(name, "rb");

  if(fp == NULL)
    return -1;

  fseek(fp, 0, SEEK_END);
  size = ftell(fp) / 14;
  fseek(fp, 0, 0);

  if(size + offset > (16 * 256))
  {
    size = (16 * 256) - offset;
  }

  fread(graphics.charset + (offset * 14), 14, size, fp);
  fclose(fp);

  // some renderers may want to map charsets to textures
  if (graphics.remap_charsets)
    graphics.remap_charsets();

  return size;
}

void ec_save_set(char *name)
{
  FILE *fp = fopen(name, "wb");

  if(fp)
  {
    fwrite(graphics.charset, 14, 256, fp);
    fclose(fp);
  }
}

void ec_save_set_var(char *name, Uint8 offset, Uint32 size)
{
  FILE *fp = fopen(name, "wb");

  if(fp)
  {
    if(size + offset > 256)
    {
      size = 256 - offset;
    }

    fwrite(graphics.charset + (offset * 14), 14, size, fp);
    fclose(fp);
  }
}

void ec_mem_load_set(Uint8 *chars)
{
  memcpy(graphics.charset, chars, 3584);

  // some renderers may want to map charsets to textures
  if (graphics.remap_charsets)
    graphics.remap_charsets();
}

void ec_mem_save_set(Uint8 *chars)
{
  memcpy(chars, graphics.charset, 3584);
}

void ec_load_mzx(void)
{
  ec_mem_load_set(graphics.default_charset);
}

void ec_load_smzx(void)
{
  ec_mem_load_set(graphics.smzx_charset);
}

void ec_load_blank(void)
{
  ec_mem_load_set(graphics.blank_charset);
}

void ec_load_ascii(void)
{
  ec_mem_load_set(graphics.ascii_charset);
}

void ec_load_char_mzx(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * 14),
   graphics.default_charset + (char_number * 14), 14);

  // some renderers may want to map charsets to textures
  if (graphics.remap_charsets)
    graphics.remap_charsets();
}

void ec_load_char_smzx(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * 14),
   graphics.smzx_charset + (char_number * 14), 14);

  // some renderers may want to map charsets to textures
  if (graphics.remap_charsets)
    graphics.remap_charsets();
}

void ec_load_char_ascii(Uint32 char_number)
{
  memcpy(graphics.charset + (char_number * 14),
   graphics.ascii_charset + (char_number * 14), 14);

  // some renderers may want to map charsets to textures
  if (graphics.remap_charsets)
    graphics.remap_charsets();
}

void init_palette(void)
{
  Uint32 i;

  memcpy(graphics.palette, default_pal, sizeof(SDL_Color) * 16);
  memcpy(graphics.palette + 16, default_pal, sizeof(SDL_Color) * 16);
  memcpy(graphics.intensity_palette, default_pal, sizeof(SDL_Color) * 16);
  memcpy(graphics.intensity_palette + 16, default_pal,
   sizeof(SDL_Color) * 16);
  memset(graphics.current_intensity, 0, sizeof(Uint32) * 16);

  for(i = 0; i < 256; i++)
    graphics.saved_intensity[i] = 100;

  for(i = 16; i < 32; i++)
    graphics.current_intensity[i] = 100;

  graphics.fade_status = 1;

  update_palette();
}

void update_colors(SDL_Surface *screen, SDL_Color *palette,
 Uint32 count)
{
  graphics.update_colors(palette, count);
}

void update_palette(void)
{
  Uint32 i;

  // Is SMZX mode set?
  if(graphics.screen_mode)
  {
    // SMZX mode 1: set all colors to diagonals
    if(graphics.screen_mode == 1)
    {
      SDL_Color new_palette[256];
      for(i = 0; i < 256; i++)
      {
        new_palette[i].r =
         ((graphics.intensity_palette[i & 15].r << 1) +
          graphics.intensity_palette[i >> 4].r) / 3;
        new_palette[i].g =
         ((graphics.intensity_palette[i & 15].g << 1) +
          graphics.intensity_palette[i >> 4].g) / 3;
        new_palette[i].b =
         ((graphics.intensity_palette[i & 15].b << 1) +
          graphics.intensity_palette[i >> 4].b) / 3;
      }
      update_colors(graphics.screen, new_palette, 256);
    }
    else
    {
      update_colors(graphics.screen, graphics.intensity_palette, 256);
    }
  }
  else
  {
    update_colors(graphics.screen, graphics.intensity_palette, 32);
  }
}

void load_palette(char *fname)
{
  FILE *pal_file = fopen(fname, "rb");

  if(pal_file)
  {
    int file_size;
    int i;
    int r, g, b;

    fseek(pal_file, 0, SEEK_END);
    file_size = ftell(pal_file);
    fseek(pal_file, 0, SEEK_SET);

    if(!graphics.screen_mode && (file_size > 48))
      file_size = 48;

    for(i = 0; i < file_size / 3; i++)
    {
      r = fgetc(pal_file);
      g = fgetc(pal_file);
      b = fgetc(pal_file);
      set_rgb(i, r, g, b);
    }

    fclose(pal_file);
  }
}

void load_palette_mem(char mem_pal[][3], int count)
{
  int i;

  for(i = 0; i < count; i++)
  {
    set_rgb(i, mem_pal[i][0], mem_pal[i][1], mem_pal[i][2]);
  }
}

void save_palette_mem(char mem_pal[][3], int count)
{
  int i;

  for(i = 0; i < count; i++)
  {
    mem_pal[i][0] = get_red_component(i);
    mem_pal[i][1] = get_green_component(i);
    mem_pal[i][2] = get_blue_component(i);
  }
}

void save_palette(char *fname)
{
  FILE *pal_file = fopen(fname, "wb");

  if(pal_file)
  {
    int num_colors = 256;
    int i;

    if(!graphics.screen_mode)
      num_colors = 16;

    for(i = 0; i < num_colors; i++)
    {
      fputc(get_red_component(i), pal_file);
      fputc(get_green_component(i), pal_file);
      fputc(get_blue_component(i), pal_file);
    }

    fclose(pal_file);
  }
}

void smzx_palette_loaded(int val)
{
  graphics.default_smzx_loaded = val;
}

void update_intensity_palette()
{
  int i;
  for(i = 0; i < 256; i++)
  {
    set_color_intensity(i, graphics.current_intensity[i]);
  }
}

void swap_palettes()
{
  SDL_Color temp_colors[256];
  Uint32 temp_intensities[256];
  memcpy(temp_colors, graphics.backup_palette,
   sizeof(SDL_Color) * 256);
  memcpy(graphics.backup_palette, graphics.palette,
   sizeof(SDL_Color) * 256);
  memcpy(graphics.palette, temp_colors,
   sizeof(SDL_Color) * 256);
  memcpy(temp_intensities, graphics.backup_intensity,
   sizeof(Uint32) * 256);
  if(graphics.fade_status)
  {
    memcpy(graphics.backup_intensity,
     graphics.saved_intensity, sizeof(Uint32) * 256);
    memcpy(graphics.saved_intensity, temp_intensities,
     sizeof(Uint32) * 256);
  }
  else
  {
    memcpy(graphics.backup_intensity,
     graphics.current_intensity, sizeof(Uint32) * 256);
    memcpy(graphics.current_intensity, temp_intensities,
     sizeof(Uint32) * 256);
    update_intensity_palette();
  }
}

void set_screen_mode(Uint32 mode)
{
  mode %= 4;

  if((mode >= 2) && (graphics.screen_mode < 2))
  {
    swap_palettes();
    graphics.screen_mode = mode;
    if(!graphics.default_smzx_loaded)
    {
      int i;

      if(graphics.fade_status)
      {
        for(i = 0; i < 256; i++)
        {
          graphics.current_intensity[i] = 0;
        }
      }
      set_palette_intensity(100);
      load_palette(SMZX_PAL);
      graphics.default_smzx_loaded = 1;
    }
    update_palette();
  }
  else

  if((graphics.screen_mode >= 2) && (mode < 2))
  {
    int fade_status = get_fade_status();
    int i;
    swap_palettes();

    if(fade_status)
    {
      set_fade_status(0);
      for(i = 16; i < 32; i++)
      {
        set_color_intensity(i, 100);
      }
      set_fade_status(fade_status);
    }
    graphics.screen_mode = mode;
    update_palette();
  }
  else
  {
    graphics.screen_mode = mode;
  }

  if(mode == 1)
  {
    update_palette();
  }
}

Uint32 get_screen_mode()
{
  return graphics.screen_mode;
}

void set_palette_intensity(Uint32 percent)
{
  Uint32 i, num_colors;

  if(graphics.screen_mode >= 2)
    num_colors = 256;
  else
    num_colors = 16;

  for(i = 0; i < num_colors; i++)
  {
    set_color_intensity(i, percent);
  }
}

void set_color_intensity(Uint32 color, Uint32 percent)
{
  if(graphics.fade_status)
  {
    graphics.saved_intensity[color] = percent;
  }
  else
  {
    int r = (graphics.palette[color].r * percent) / 100;
    int g = (graphics.palette[color].g * percent) / 100;
    int b = (graphics.palette[color].b * percent) / 100;

    if(r > 255)
      r = 255;

    if(g > 255)
      g = 255;

    if(b > 255)
      b = 255;

    graphics.intensity_palette[color].r = r;
    graphics.intensity_palette[color].g = g;
    graphics.intensity_palette[color].b = b;

    graphics.current_intensity[color] = percent;
  }
}

void set_rgb(Uint32 color, Uint32 r, Uint32 g, Uint32 b)
{
  int intensity = graphics.current_intensity[color];
  r = r * 255 / 63;
  g = g * 255 / 63;
  b = b * 255 / 63;

  graphics.palette[color].r = r;
  r = (r * intensity) / 100;
  if(r > 255)
    r = 255;
  graphics.intensity_palette[color].r = r;

  graphics.palette[color].g = g;
  g = (g * intensity) / 100;
  if(g > 255)
    g = 255;
  graphics.intensity_palette[color].g = g;

  graphics.palette[color].b = b;
  b = (b * intensity) / 100;
  if(b > 255)
    b = 255;
  graphics.intensity_palette[color].b = b;
}

void set_red_component(Uint32 color, Uint32 r)
{
  r = r * 255 / 63;
  graphics.palette[color].r = r;

  r = r * graphics.current_intensity[color] / 100;
  if(r > 255)
    r = 255;

  graphics.intensity_palette[color].r = r;
}

void set_green_component(Uint32 color, Uint32 g)
{
  g = g * 255 / 63;
  graphics.palette[color].g = g;

  g = g * graphics.current_intensity[color] / 100;
  if(g > 255)
    g = 255;

  graphics.intensity_palette[color].g = g;
}

void set_blue_component(Uint32 color, Uint32 b)
{
  b = b * 255 / 63;
  graphics.palette[color].b = b;

  b = b * graphics.current_intensity[color] / 100;
  if(b > 255)
    b = 255;

  graphics.intensity_palette[color].b = b;
}

Uint32 get_color_intensity(Uint32 color)
{
  return graphics.current_intensity[color];
}

void get_rgb(Uint32 color, Uint8 *r, Uint8 *g, Uint8 *b)
{
  *r = ((graphics.palette[color].r * 126) + 255) / 510;
  *g = ((graphics.palette[color].g * 126) + 255) / 510;
  *b = ((graphics.palette[color].b * 126) + 255) / 510;
}

Uint32 get_red_component(Uint32 color)
{
  return ((graphics.palette[color].r * 126) + 255) / 510;
}

Uint32 get_green_component(Uint32 color)
{
  return ((graphics.palette[color].g * 126) + 255) / 510;
}

Uint32 get_blue_component(Uint32 color)
{
  return ((graphics.palette[color].b * 126) + 255) / 510;
}

Uint32 get_fade_status()
{
  return graphics.fade_status;
}

void set_fade_status(Uint32 fade)
{
  graphics.fade_status = fade;
}

void get_palette(Uint8 *dest)
{
  Uint32 i, i2;

  for(i = 0, i2 = 0; i < 16; i++, i2 += 3)
  {
    dest[i2] = graphics.intensity_palette[i].r;
    dest[i2 + 1] = graphics.intensity_palette[i].g;
    dest[i2 + 2] = graphics.intensity_palette[i].b;
  }
}

void get_palette_smzx(Uint8 *dest)
{
  Uint32 i, i2;

  for(i = 0, i2 = 0; i < 16; i++, i2 += 3)
  {
    dest[i2] = graphics.intensity_palette[i].r;
    dest[i2 + 1] = graphics.intensity_palette[i].g;
    dest[i2 + 2] = graphics.intensity_palette[i].b;
  }
}

// Very quick fade out. Saves intensity table for fade in. Be sure
// to use in conjuction with the next function.
void vquick_fadeout(void)
{
  if(!graphics.fade_status)
  {
    Sint32 i, num_colors;
    Uint32 ticks;

    if(graphics.screen_mode >= 2)
      num_colors = 256;
    else
      num_colors = 16;

    memcpy(graphics.saved_intensity, graphics.current_intensity,
     sizeof(Uint32) * num_colors);

    for(i = 100; i >= 0; i -= 10)
    {
      ticks = SDL_GetTicks();
      set_palette_intensity(i);
      update_palette();
      update_screen();
      ticks = SDL_GetTicks() - ticks;
      if(ticks <= 16)
        delay(16 - ticks);
    }
    graphics.fade_status = 1;
  }
}

// Very quick fade in. Uses intensity table saved from fade out. For
// use in conjuction with the previous function.
void vquick_fadein(void)
{
  if(graphics.fade_status)
  {
    Uint32 i, i2, num_colors;
    Uint32 ticks;

    graphics.fade_status = 0;

    if(graphics.screen_mode >= 2)
      num_colors = 256;
    else
      num_colors = 16;

    for(i = 0; i < 10; i++)
    {
      ticks = SDL_GetTicks();
      for(i2 = 0; i2 < num_colors; i2++)
      {
        graphics.current_intensity[i2] += 10;
        if(graphics.current_intensity[i2] > graphics.saved_intensity[i2])
          graphics.current_intensity[i2] = graphics.saved_intensity[i2];
        set_color_intensity(i2, graphics.current_intensity[i2]);
      }
      update_palette();
      update_screen();
      ticks = SDL_GetTicks() - ticks;
      if(ticks <= 16)
        delay(16 - ticks);
    }
  }
}

// Instant fade out
void insta_fadeout(void)
{
  Uint32 i, num_colors;

  if(graphics.fade_status)
    return;

  if(graphics.screen_mode >= 2)
    num_colors = 256;
  else
    num_colors = 16;

  for(i = 0; i < num_colors; i++)
    set_color_intensity(i, 0);

  delay(1);
  update_palette();
  update_screen(); // NOTE: this was called conditionally in 2.81e

  graphics.fade_status = true;
}

// Instant fade in
void insta_fadein(void)
{
  Uint32 i, num_colors;

  if(!graphics.fade_status)
    return;

  graphics.fade_status = false;

  if(graphics.screen_mode >= 2)
    num_colors = 256;
  else
    num_colors = 16;

  for(i = 0; i < num_colors; i++)
    set_color_intensity(i, graphics.saved_intensity[i]);

  update_palette();
  update_screen(); // NOTE: this was called conditionally in 2.81e
}

void default_palette(void)
{
  memcpy(graphics.palette, default_pal, sizeof(SDL_Color) * 16);
  memcpy(graphics.intensity_palette, default_pal, sizeof(SDL_Color) * 16);
  update_palette();
}

void m_hide(void)
{
  graphics.mouse_status = 0;
}

void m_show(void)
{
  graphics.mouse_status = 1;
}

void dump_screen()
{
  int i;
  char name[16];
  struct stat file_info;

  for(i = 0; i < 99999; i++)
  {
    sprintf(name, "screen%d.bmp", i);
    if(stat(name, &file_info))
      break;
  }
  SDL_SaveBMP(graphics.screen, name);
}

void get_screen_coords(int screen_x, int screen_y, int *x, int *y,
 int *min_x, int *min_y, int *max_x, int *max_y)
{
  int target_width = graphics.window_width;
  int target_height = graphics.window_height;

  if(graphics.fullscreen)
  {
    target_width = graphics.resolution_width;
    target_height = graphics.resolution_height;
  }

  if(graphics.allow_resize)
  {
    *x = screen_x * 640 / target_width;
    *y = screen_y * 350 / target_height;
    *min_x = 0;
    *min_y = 0;
    *max_x = target_width - 1;
    *max_y = target_height - 1;
  }
  else
  {
    int w_offset = (target_width - 640) / 2;
    int h_offset = (target_height - 350) / 2;

    *x = screen_x - w_offset;
    *y = screen_y - h_offset;

    *min_x = w_offset;
    *min_y = h_offset;
    *max_x = 639 + w_offset;
    *max_y = 349 + h_offset;
  }
}

void set_screen_coords(int x, int y, int *screen_x, int *screen_y)
{
  graphics.set_screen_coords(x, y, screen_x, screen_y);
}

void set_mouse_mul(int width_mul, int height_mul)
{
  graphics.mouse_width_mul = width_mul;
  graphics.mouse_height_mul = height_mul;
}
