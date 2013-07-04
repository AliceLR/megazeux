/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#include <stdlib.h>

#include "platform.h"
#include "graphics.h"
#include "render.h"
#include "util.h"

static void set_colors8_mzx (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  char_colors[0] = (bg << 24) | (bg << 16) | (bg << 8) | bg;
  char_colors[1] = (bg << 24) | (bg << 16) | (bg << 8) | fg;
  char_colors[2] = (bg << 24) | (bg << 16) | (fg << 8) | bg;
  char_colors[3] = (bg << 24) | (bg << 16) | (fg << 8) | fg;
  char_colors[4] = (bg << 24) | (fg << 16) | (bg << 8) | bg;
  char_colors[5] = (bg << 24) | (fg << 16) | (bg << 8) | fg;
  char_colors[6] = (bg << 24) | (fg << 16) | (fg << 8) | bg;
  char_colors[7] = (bg << 24) | (fg << 16) | (fg << 8) | fg;
  char_colors[8] = (fg << 24) | (bg << 16) | (bg << 8) | bg;
  char_colors[9] = (fg << 24) | (bg << 16) | (bg << 8) | fg;
  char_colors[10] = (fg << 24) | (bg << 16) | (fg << 8) | bg;
  char_colors[11] = (fg << 24) | (bg << 16) | (fg << 8) | fg;
  char_colors[12] = (fg << 24) | (fg << 16) | (bg << 8) | bg;
  char_colors[13] = (fg << 24) | (fg << 16) | (bg << 8) | fg;
  char_colors[14] = (fg << 24) | (fg << 16) | (fg << 8) | bg;
  char_colors[15] = (fg << 24) | (fg << 16) | (fg << 8) | fg;
#else
  char_colors[0] = (bg << 24) | (bg << 16) | (bg << 8) | bg;
  char_colors[1] = (fg << 24) | (bg << 16) | (bg << 8) | bg;
  char_colors[2] = (bg << 24) | (fg << 16) | (bg << 8) | bg;
  char_colors[3] = (fg << 24) | (fg << 16) | (bg << 8) | bg;
  char_colors[4] = (bg << 24) | (bg << 16) | (fg << 8) | bg;
  char_colors[5] = (fg << 24) | (bg << 16) | (fg << 8) | bg;
  char_colors[6] = (bg << 24) | (fg << 16) | (fg << 8) | bg;
  char_colors[7] = (fg << 24) | (fg << 16) | (fg << 8) | bg;
  char_colors[8] = (bg << 24) | (bg << 16) | (bg << 8) | fg;
  char_colors[9] = (fg << 24) | (bg << 16) | (bg << 8) | fg;
  char_colors[10] = (bg << 24) | (fg << 16) | (bg << 8) | fg;
  char_colors[11] = (fg << 24) | (fg << 16) | (bg << 8) | fg;
  char_colors[12] = (bg << 24) | (bg << 16) | (fg << 8) | fg;
  char_colors[13] = (fg << 24) | (bg << 16) | (fg << 8) | fg;
  char_colors[14] = (bg << 24) | (fg << 16) | (fg << 8) | fg;
  char_colors[15] = (fg << 24) | (fg << 16) | (fg << 8) | fg;
#endif
}

static void set_colors8_smzx (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  Uint32 bb, bf, fb, ff;
  bg &= 0x0F;
  fg &= 0x0F;
  bb = (bg << 4) | bg;
  bf = (bg << 4) | fg;
  fb = (fg << 4) | bg;
  ff = (fg << 4) | fg;
  bb |= bb << 8;
  bf |= bf << 8;
  fb |= fb << 8;
  ff |= ff << 8;

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  char_colors[0] = (bb << 16) | bb;
  char_colors[1] = (bb << 16) | bf;
  char_colors[2] = (bb << 16) | fb;
  char_colors[3] = (bb << 16) | ff;
  char_colors[4] = (bf << 16) | bb;
  char_colors[5] = (bf << 16) | bf;
  char_colors[6] = (bf << 16) | fb;
  char_colors[7] = (bf << 16) | ff;
  char_colors[8] = (fb << 16) | bb;
  char_colors[9] = (fb << 16) | bf;
  char_colors[10] = (fb << 16) | fb;
  char_colors[11] = (fb << 16) | ff;
  char_colors[12] = (ff << 16) | bb;
  char_colors[13] = (ff << 16) | bf;
  char_colors[14] = (ff << 16) | fb;
  char_colors[15] = (ff << 16) | ff;
#else
  char_colors[0] = (bb << 16) | bb;
  char_colors[1] = (bf << 16) | bb;
  char_colors[2] = (fb << 16) | bb;
  char_colors[3] = (ff << 16) | bb;
  char_colors[4] = (bb << 16) | bf;
  char_colors[5] = (bf << 16) | bf;
  char_colors[6] = (fb << 16) | bf;
  char_colors[7] = (ff << 16) | bf;
  char_colors[8] = (bb << 16) | fb;
  char_colors[9] = (bf << 16) | fb;
  char_colors[10] = (fb << 16) | fb;
  char_colors[11] = (ff << 16) | fb;
  char_colors[12] = (bb << 16) | ff;
  char_colors[13] = (bf << 16) | ff;
  char_colors[14] = (fb << 16) | ff;
  char_colors[15] = (ff << 16) | ff;
#endif
}

static void set_colors8_smzx3 (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  Uint32 c0, c1, c2, c3;
  c0 = ((bg << 4) | (fg & 0x0F)) & 0xFF;
  c1 = (c0 + 1) & 0xFF;
  c2 = (c0 + 2) & 0xFF;
  c3 = (c0 + 3) & 0xFF;

  c0 |= c0 << 8;
  c1 |= c1 << 8;
  c2 |= c2 << 8;
  c3 |= c3 << 8;

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  char_colors[0] = (c0 << 16) | c0;
  char_colors[1] = (c0 << 16) | c2;
  char_colors[2] = (c0 << 16) | c1;
  char_colors[3] = (c0 << 16) | c3;
  char_colors[4] = (c2 << 16) | c0;
  char_colors[5] = (c2 << 16) | c2;
  char_colors[6] = (c2 << 16) | c1;
  char_colors[7] = (c2 << 16) | c3;
  char_colors[8] = (c1 << 16) | c0;
  char_colors[9] = (c1 << 16) | c2;
  char_colors[10] = (c1 << 16) | c1;
  char_colors[11] = (c1 << 16) | c3;
  char_colors[12] = (c3 << 16) | c0;
  char_colors[13] = (c3 << 16) | c2;
  char_colors[14] = (c3 << 16) | c1;
  char_colors[15] = (c3 << 16) | c3;
#else
  char_colors[0] = (c0 << 16) | c0;
  char_colors[1] = (c2 << 16) | c0;
  char_colors[2] = (c1 << 16) | c0;
  char_colors[3] = (c3 << 16) | c0;
  char_colors[4] = (c0 << 16) | c2;
  char_colors[5] = (c2 << 16) | c2;
  char_colors[6] = (c1 << 16) | c2;
  char_colors[7] = (c3 << 16) | c2;
  char_colors[8] = (c0 << 16) | c1;
  char_colors[9] = (c2 << 16) | c1;
  char_colors[10] = (c1 << 16) | c1;
  char_colors[11] = (c3 << 16) | c1;
  char_colors[12] = (c0 << 16) | c3;
  char_colors[13] = (c2 << 16) | c3;
  char_colors[14] = (c1 << 16) | c3;
  char_colors[15] = (c3 << 16) | c3;
#endif
}

static void set_colors16_mzx (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  Uint32 cb_bg, cb_fg;

  cb_bg = graphics->flat_intensity_palette[bg];
  cb_fg = graphics->flat_intensity_palette[fg];

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
        char_colors[0] = (cb_bg << 16) | cb_bg;
        char_colors[1] = (cb_bg << 16) | cb_fg;
        char_colors[2] = (cb_fg << 16) | cb_bg;
        char_colors[3] = (cb_fg << 16) | cb_fg;
#else
        char_colors[0] = (cb_bg << 16) | cb_bg;
        char_colors[1] = (cb_fg << 16) | cb_bg;
        char_colors[2] = (cb_bg << 16) | cb_fg;
        char_colors[3] = (cb_fg << 16) | cb_fg;
#endif
}

static void set_colors16_smzx (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  bg &= 0x0F;
  fg &= 0x0F;

  char_colors[0] = graphics->flat_intensity_palette[(bg << 4) | bg];
  char_colors[1] = graphics->flat_intensity_palette[(bg << 4) | fg];
  char_colors[2] = graphics->flat_intensity_palette[(fg << 4) | bg];
  char_colors[3] = graphics->flat_intensity_palette[(fg << 4) | fg];

  char_colors[0] = (char_colors[0] << 16) | char_colors[0];
  char_colors[1] = (char_colors[1] << 16) | char_colors[1];
  char_colors[2] = (char_colors[2] << 16) | char_colors[2];
  char_colors[3] = (char_colors[3] << 16) | char_colors[3];
}

static void set_colors16_smzx3 (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  Uint8 base;

  base = (bg << 4) | (fg & 0x0F);

  char_colors[0] = graphics->flat_intensity_palette[base];
  char_colors[1] = graphics->flat_intensity_palette[(base + 2) & 0xFF];
  char_colors[2] = graphics->flat_intensity_palette[(base + 1) & 0xFF];
  char_colors[3] = graphics->flat_intensity_palette[(base + 3) & 0xFF];

  char_colors[0] = (char_colors[0] << 16) | char_colors[0];
  char_colors[1] = (char_colors[1] << 16) | char_colors[1];
  char_colors[2] = (char_colors[2] << 16) | char_colors[2];
  char_colors[3] = (char_colors[3] << 16) | char_colors[3];
}

static void set_colors32_mzx (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  char_colors[0] = graphics->flat_intensity_palette[bg];
  char_colors[1] = graphics->flat_intensity_palette[fg];
}

static void set_colors32_smzx (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  bg &= 0x0F;
  fg &= 0x0F;

  char_colors[0] = graphics->flat_intensity_palette[(bg << 4) | bg];
  char_colors[1] = graphics->flat_intensity_palette[(bg << 4) | fg];
  char_colors[2] = graphics->flat_intensity_palette[(fg << 4) | bg];
  char_colors[3] = graphics->flat_intensity_palette[(fg << 4) | fg];
}

static void set_colors32_smzx3 (struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  Uint8 base;

  base = (bg << 4) | (fg & 0x0F);

  char_colors[0] = graphics->flat_intensity_palette[base];
  char_colors[1] = graphics->flat_intensity_palette[(base + 2) & 0xFF];
  char_colors[2] = graphics->flat_intensity_palette[(base + 1) & 0xFF];
  char_colors[3] = graphics->flat_intensity_palette[(base + 3) & 0xFF];
}

void (*const set_colors8[4])
 (struct graphics_data *, Uint32 *, Uint8, Uint8) =
{
  set_colors8_mzx,
  set_colors8_smzx,
  set_colors8_smzx,
  set_colors8_smzx3
};

void (*const set_colors16[4])
 (struct graphics_data *, Uint32 *, Uint8, Uint8) =
{
  set_colors16_mzx,
  set_colors16_smzx,
  set_colors16_smzx,
  set_colors16_smzx3
};

void (*const set_colors32[4])
 (struct graphics_data *, Uint32 *, Uint8, Uint8) =
{
  set_colors32_mzx,
  set_colors32_smzx,
  set_colors32_smzx,
  set_colors32_smzx3
};

#ifdef CONFIG_RENDER_YUV

#include "render_yuv.h"

static void yuv2_set_colors_mzx(struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 y0mask = render_data->y0mask;
  Uint32 y1mask = render_data->y1mask;
  Uint32 uvmask = render_data->uvmask;
  Uint32 cb_bg, cb_fg, cb_mix;

  cb_bg = graphics->flat_intensity_palette[bg];
  cb_fg = graphics->flat_intensity_palette[fg];
  cb_mix = (((cb_bg & uvmask) >> 1) + ((cb_fg & uvmask) >> 1)) & uvmask;

  char_colors[0] = cb_bg;
  char_colors[1] = cb_mix | (cb_bg & y0mask) | (cb_fg & y1mask);
  char_colors[2] = cb_mix | (cb_fg & y0mask) | (cb_bg & y1mask);
  char_colors[3] = cb_fg;
}

void (*const yuv2_set_colors[4])
 (struct graphics_data *, Uint32 *, Uint8, Uint8) =
{
  yuv2_set_colors_mzx,
  set_colors32_smzx,
  set_colors32_smzx,
  set_colors32_smzx3
};

#endif

// Nominally 8-bit (Character graphics 8 bytes wide)
void render_graph8(Uint8 *pixels, Uint32 pitch, struct graphics_data *graphics,
 void (*set_colors)(struct graphics_data *, Uint32 *, Uint8, Uint8))
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  struct char_element *src = graphics->text_video;
  Uint8 old_bg = 255;
  Uint8 old_fg = 255;
  Uint8 *char_ptr;
  Uint32 char_colors[16];
  Uint32 current_char_byte;
  Uint32 i, i2, i3;
  Uint32 line_advance = pitch / 4;
  Uint32 row_advance = line_advance * 14;

  dest = (Uint32 *)pixels;

  for(i = 0; i < 25; i++)
  {
    ldest2 = dest;
    for(i2 = 0; i2 < 80; i2++)
    {
      ldest = dest;
      if((src->bg_color != old_bg) || (src->fg_color != old_fg))
      {
        set_colors(graphics, char_colors, src->bg_color, src->fg_color);
        old_bg = src->bg_color;
        old_fg = src->fg_color;
      }

      char_ptr = graphics->charset + (src->char_value * 14);
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

// Nominally 16-bit (Character graphics 16 bytes wide)
void render_graph16(Uint16 *pixels, Uint32 pitch,
 struct graphics_data *graphics,
 void (*set_colors)(struct graphics_data *, Uint32 *, Uint8, Uint8))
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  struct char_element *src = graphics->text_video;
  Uint8 old_bg = 255;
  Uint8 old_fg = 255;
  Uint8 *char_ptr;
  Uint32 char_colors[4];
  Uint32 current_char_byte;
  Uint32 i, i2, i3;
  Uint32 line_advance = pitch / 4;
  Uint32 row_advance = line_advance * 14;

  dest = (Uint32 *)pixels;

  for(i = 0; i < 25; i++)
  {
    ldest2 = dest;
    for(i2 = 0; i2 < 80; i2++)
    {
      ldest = dest;
      if((src->bg_color != old_bg) || (src->fg_color != old_fg))
      {
        set_colors(graphics, char_colors, src->bg_color, src->fg_color);
        old_bg = src->bg_color;
        old_fg = src->fg_color;
      }

      char_ptr = graphics->charset + (src->char_value * 14);
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

// Nominally 32-bit (Character graphics 32 bytes wide)
void render_graph32(Uint32 *pixels, Uint32 pitch,
 struct graphics_data *graphics,
 void (*set_colors)(struct graphics_data *, Uint32 *, Uint8, Uint8))
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  struct char_element *src = graphics->text_video;
  Uint8 old_bg = 255;
  Uint8 old_fg = 255;
  Uint8 *char_ptr;
  Uint32 char_colors[2];
  Uint32 current_char_byte;
  Uint32 i, i2, i3;
  Sint32 i4;
  Uint32 line_advance = pitch / 4;
  Uint32 line_advance_sub = line_advance - 8;
  Uint32 row_advance = line_advance * 14;

  dest = pixels;

  for(i = 0; i < 25; i++)
  {
    ldest2 = dest;
    for(i2 = 0; i2 < 80; i2++)
    {
      ldest = dest;
      if((src->bg_color != old_bg) || (src->fg_color != old_fg))
      {
        set_colors(graphics, char_colors, src->bg_color, src->fg_color);
        old_bg = src->bg_color;
        old_fg = src->fg_color;
      }

      char_ptr = graphics->charset + (src->char_value * 14);
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

void render_graph32s(Uint32 *pixels, Uint32 pitch,
 struct graphics_data *graphics,
 void (*set_colors)(struct graphics_data *, Uint32 *, Uint8, Uint8))
{
  Uint32 *dest;
  Uint32 *ldest, *ldest2;
  struct char_element *src = graphics->text_video;
  Uint8 old_bg = 255;
  Uint8 old_fg = 255;
  Uint8 *char_ptr;
  Uint32 char_colors[4];
  Uint32 current_char_byte;
  Uint32 current_color;
  Uint32 i, i2, i3;
  Sint32 i4;
  Uint32 line_advance = pitch / 4;
  Uint32 line_advance_sub = line_advance - 8;
  Uint32 row_advance = line_advance * 14;

  dest = pixels;

  for(i = 0; i < 25; i++)
  {
    ldest2 = dest;
    for(i2 = 0; i2 < 80; i2++)
    {
      ldest = dest;
      if((src->bg_color != old_bg) || (src->fg_color != old_fg))
      {
        set_colors(graphics, char_colors, src->bg_color, src->fg_color);
        old_bg = src->bg_color;
        old_fg = src->fg_color;
      }

      char_ptr = graphics->charset + (src->char_value * 14);
      src++;
      for(i3 = 0; i3 < 14; i3++)
      {
        current_char_byte = *char_ptr;
        char_ptr++;
        for(i4 = 6; i4 >= 0; i4 -= 2, dest += 2)
        {
          current_color = char_colors[(current_char_byte >> i4) & 0x03];
          *dest = current_color;
          *(dest + 1) = current_color;
        }
        dest += line_advance_sub;
      }

      dest = ldest + 8;
    }
    dest = ldest2 + row_advance;
  }
}

void render_cursor(Uint32 *pixels, Uint32 pitch, Uint8 bpp, Uint32 x, Uint32 y,
 Uint32 color, Uint8 lines, Uint8 offset)
{
  int i,j;
  Uint8 size = bpp / 4;
  Uint32 line_advance = pitch / 4;
  Uint32 line_advance_sub = line_advance - size;
  Uint32 row_advance = line_advance * 14;
  Uint32 *dest = pixels + (x * size) + (y * row_advance) +
   (offset * line_advance);
  for(i = 0; i < lines; i++)
  {
    for(j = 0; j < size; j++)
      *(dest++) = color;
    dest += line_advance_sub;
  }
}

void render_mouse(Uint32 *pixels, Uint32 pitch, Uint8 bpp, Uint32 x, Uint32 y,
 Uint32 mask, Uint32 amask, Uint8 w, Uint8 h)
{
  int i,j;
  Uint8 size = w * bpp / 32;
  Uint32 line_advance = pitch / 4;
  Uint32 line_advance_sub = line_advance - size;
  Uint32 *dest = pixels + (x * bpp / 32) + (y * line_advance);
  for(i = 0; i < h; i++)
  {
    for(j = 0; j < size; j++)
    {
      *dest = amask | (*dest ^ mask);
      dest++;
    }
    dest += line_advance_sub;
  }
}

void get_screen_coords_centered(struct graphics_data *graphics, int screen_x,
 int screen_y, int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y)
{
  int w_offset, h_offset;
  int target_width = graphics->window_width;
  int target_height = graphics->window_height;

  if(graphics->fullscreen)
  {
    target_width = graphics->resolution_width;
    target_height = graphics->resolution_height;
  }

  w_offset = (target_width - 640) / 2;
  h_offset = (target_height - 350) / 2;

  *x = screen_x - w_offset;
  *y = screen_y - h_offset;

  *min_x = w_offset;
  *min_y = h_offset;
  *max_x = 639 + w_offset;
  *max_y = 349 + h_offset;
}

void set_screen_coords_centered(struct graphics_data *graphics, int x, int y,
 int *screen_x, int *screen_y)
{
  int target_width = graphics->window_width;
  int target_height = graphics->window_height;

  if(graphics->fullscreen)
  {
    target_width = graphics->resolution_width;
    target_height = graphics->resolution_height;
  }

  *screen_x = x + (target_width - 640) / 2;
  *screen_y = y + (target_height - 350) / 2;
}

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM) \
 || defined(CONFIG_RENDER_YUV)

void get_screen_coords_scaled(struct graphics_data *graphics, int screen_x,
 int screen_y, int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y)
{
  int target_width = graphics->window_width;
  int target_height = graphics->window_height;

  if(graphics->fullscreen)
  {
    target_width = graphics->resolution_width;
    target_height = graphics->resolution_height;
  }

  *x = screen_x * 640 / target_width;
  *y = screen_y * 350 / target_height;
  *min_x = 0;
  *min_y = 0;
  *max_x = target_width - 1;
  *max_y = target_height - 1;
}

void set_screen_coords_scaled(struct graphics_data *graphics, int x, int y,
 int *screen_x, int *screen_y)
{
  int target_width = graphics->window_width;
  int target_height = graphics->window_height;

  if(graphics->fullscreen)
  {
    target_width = graphics->resolution_width;
    target_height = graphics->resolution_height;
  }

  *screen_x = x * target_width / 640;
  *screen_y = y * target_height / 350;
}

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM || CONFIG_RENDER_YUV

// FIXME: Integerize

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM) \
 || defined(CONFIG_RENDER_YUV) || defined(CONFIG_RENDER_GX)

void fix_viewport_ratio(int width, int height, int *v_width, int *v_height,
 enum ratio_type ratio)
{
  int numerator = 0, denominator = 0;

  *v_width = width;
  *v_height = height;

  if((ratio == RATIO_STRETCH))
    return;

  if(ratio == RATIO_CLASSIC_4_3)
  {
    numerator = 4;
    denominator = 3;
  }
  else if(ratio == RATIO_MODERN_64_35)
  {
    numerator = 64;
    denominator = 35;
  }

  if(((float)width / (float)height) < ((float)numerator / (float)denominator))
    *v_height = (width * denominator) / numerator;
  else
    *v_width = (height * numerator) / denominator;
}

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM || CONFIG_RENDER_YUV || CONFIG_RENDER_GX

void resize_screen_standard(struct graphics_data *graphics, int w, int h)
{
  update_screen();
  update_palette();
}
