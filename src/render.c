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

#include "graphics.h"
#include "platform_endian.h"
#include "render.h"
#include "render_layer.h"
#include "util.h"
#include "yuv.h"

static void set_colors8_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
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

static void set_colors8_smzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  const uint8_t *indices = graphics->smzx_indices;
  uint32_t bb, bf, fb, ff;
  bg &= 0x0F;
  fg &= 0x0F;
  indices += ((bg << 4) | fg) << 2;

  bb = indices[0];
  bf = indices[1];
  fb = indices[2];
  ff = indices[3];

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

static void set_colors16_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  uint32_t cb_bg, cb_fg;

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

static void set_colors16_smzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  const uint8_t *indices = graphics->smzx_indices;
  bg &= 0x0F;
  fg &= 0x0F;
  indices += ((bg << 4) | fg) << 2;

  char_colors[0] = graphics->flat_intensity_palette[indices[0]];
  char_colors[1] = graphics->flat_intensity_palette[indices[1]];
  char_colors[2] = graphics->flat_intensity_palette[indices[2]];
  char_colors[3] = graphics->flat_intensity_palette[indices[3]];

  char_colors[0] = (char_colors[0] << 16) | char_colors[0];
  char_colors[1] = (char_colors[1] << 16) | char_colors[1];
  char_colors[2] = (char_colors[2] << 16) | char_colors[2];
  char_colors[3] = (char_colors[3] << 16) | char_colors[3];
}

static void set_colors32_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  char_colors[0] = graphics->flat_intensity_palette[bg];
  char_colors[1] = graphics->flat_intensity_palette[fg];
}

static void set_colors32_smzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  const uint8_t *indices = graphics->smzx_indices;
  bg &= 0x0F;
  fg &= 0x0F;
  indices += ((bg << 4) | fg) << 2;

  char_colors[0] = graphics->flat_intensity_palette[indices[0]];
  char_colors[1] = graphics->flat_intensity_palette[indices[1]];
  char_colors[2] = graphics->flat_intensity_palette[indices[2]];
  char_colors[3] = graphics->flat_intensity_palette[indices[3]];
}

const set_colors_function set_colors8[4] =
{
  set_colors8_mzx,
  set_colors8_smzx,
  set_colors8_smzx,
  set_colors8_smzx
};

const set_colors_function set_colors16[4] =
{
  set_colors16_mzx,
  set_colors16_smzx,
  set_colors16_smzx,
  set_colors16_smzx
};

/* The 32-bit set colors functions should be inlined in render_graph32 and
 * render_graph32s, but they may be needed by non-32 bit render_graph
 * implementations (e.g. SMZX with chroma subsampling in render_graph16), so
 * also provide them as an array.
 */
const set_colors_function set_colors32[4] =
{
  set_colors32_mzx,
  set_colors32_smzx,
  set_colors32_smzx,
  set_colors32_smzx
};

/* YUY2 chroma subsampling set_colors function for use with render_graph16. */
void yuy2_subsample_set_colors_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  uint32_t yuv_bg = graphics->flat_intensity_palette[bg];
  uint32_t yuv_fg = graphics->flat_intensity_palette[fg];

  char_colors[0] = yuv_bg;
  char_colors[1] = yuy2_subsample(yuv_bg, yuv_fg);
  char_colors[2] = yuy2_subsample(yuv_fg, yuv_bg);
  char_colors[3] = yuv_fg;
}

/* UYVY chroma subsampling set_colors function for use with render_graph16. */
void uyvy_subsample_set_colors_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  uint32_t yuv_bg = graphics->flat_intensity_palette[bg];
  uint32_t yuv_fg = graphics->flat_intensity_palette[fg];

  char_colors[0] = yuv_bg;
  char_colors[1] = uyvy_subsample(yuv_bg, yuv_fg);
  char_colors[2] = uyvy_subsample(yuv_fg, yuv_bg);
  char_colors[3] = yuv_fg;
}

/* YVYU chroma subsampling set_colors function for use with render_graph16. */
void yvyu_subsample_set_colors_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  uint32_t yuv_bg = graphics->flat_intensity_palette[bg];
  uint32_t yuv_fg = graphics->flat_intensity_palette[fg];

  char_colors[0] = yuv_bg;
  char_colors[1] = yvyu_subsample(yuv_bg, yuv_fg);
  char_colors[2] = yvyu_subsample(yuv_fg, yuv_bg);
  char_colors[3] = yuv_fg;
}

// Nominally 8-bit (Character graphics 8 bytes wide)
void render_graph8(uint8_t * RESTRICT pixels, size_t pitch,
 const struct graphics_data *graphics, set_colors_function set_colors)
{
  uint32_t *dest;
  uint32_t *ldest, *ldest2;
  const struct char_element *src = graphics->text_video;
  const uint8_t *char_ptr;
  uint32_t char_colors[16];
  unsigned int old_bg = 255;
  unsigned int old_fg = 255;
  unsigned int current_char_byte;
  unsigned int i, i2, i3;
  size_t line_advance = pitch / 4;
  size_t row_advance = line_advance * 14;

  dest = (uint32_t *)pixels;

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
void render_graph16(uint16_t * RESTRICT pixels, size_t pitch,
 const struct graphics_data *graphics, set_colors_function set_colors)
{
  uint32_t *dest;
  uint32_t *ldest, *ldest2;
  const struct char_element *src = graphics->text_video;
  const uint8_t *char_ptr;
  uint32_t char_colors[4];
  unsigned int old_bg = 255;
  unsigned int old_fg = 255;
  unsigned int current_char_byte;
  unsigned int i, i2, i3;
  size_t line_advance = pitch / 4;
  size_t row_advance = line_advance * 14;

  dest = (uint32_t *)pixels;

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

/* Nominally 32-bit (Character graphics 32 bytes wide)
 * Because the render_graph32 functions are guaranteed to map a single pixel to
 * a single 32-bit color they can used fixed set_colors functions, hopefully
 * saving time on platforms where that would actually matter.
 */
void render_graph32(uint32_t * RESTRICT pixels, size_t pitch,
 const struct graphics_data *graphics)
{
  uint32_t *dest;
  uint32_t *ldest, *ldest2;
  const struct char_element *src = graphics->text_video;
  const uint8_t *char_ptr;
  uint32_t char_colors[2];
  unsigned int old_bg = 255;
  unsigned int old_fg = 255;
  unsigned int current_char_byte;
  unsigned int i, i2, i3;
  int i4;
  size_t line_advance = pitch / 4;
  size_t line_advance_sub = line_advance - 8;
  size_t row_advance = line_advance * 14;

  dest = pixels;

  for(i = 0; i < 25; i++)
  {
    ldest2 = dest;
    for(i2 = 0; i2 < 80; i2++)
    {
      ldest = dest;
      if((src->bg_color != old_bg) || (src->fg_color != old_fg))
      {
        set_colors32_mzx(graphics, char_colors, src->bg_color, src->fg_color);
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

void render_graph32s(uint32_t * RESTRICT pixels, size_t pitch,
 const struct graphics_data *graphics)
{
  uint32_t *dest;
  uint32_t *ldest, *ldest2;
  const struct char_element *src = graphics->text_video;
  const uint8_t *char_ptr;
  uint32_t char_colors[4];
  uint32_t current_color;
  unsigned int old_bg = 255;
  unsigned int old_fg = 255;
  unsigned int current_char_byte;
  unsigned int i, i2, i3;
  int i4;
  size_t line_advance = pitch / 4;
  size_t line_advance_sub = line_advance - 8;
  size_t row_advance = line_advance * 14;

  dest = pixels;

  for(i = 0; i < 25; i++)
  {
    ldest2 = dest;
    for(i2 = 0; i2 < 80; i2++)
    {
      ldest = dest;
      if((src->bg_color != old_bg) || (src->fg_color != old_fg))
      {
        set_colors32_smzx(graphics, char_colors, src->bg_color, src->fg_color);
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

void render_cursor(uint32_t *pixels, size_t pitch, uint8_t bpp, unsigned int x,
 unsigned int y, uint32_t flatcolor, uint8_t lines, uint8_t offset)
{
  unsigned int i, j;
  unsigned int size = bpp / 4;
  size_t line_advance = pitch / 4;
  size_t line_advance_sub = line_advance - size;
  size_t row_advance = line_advance * 14;

  uint32_t *dest = pixels + (x * size) + (y * row_advance) + (offset * line_advance);

  for(i = 0; i < lines; i++)
  {
    for(j = 0; j < size; j++)
      *(dest++) = flatcolor;

    dest += line_advance_sub;
  }
}

void render_mouse(uint32_t *pixels, size_t pitch, uint8_t bpp, unsigned int x,
 unsigned int y, uint32_t mask, uint32_t amask, uint8_t w, uint8_t h)
{
  unsigned int i, j;
  unsigned int size = w * bpp / 32;
  size_t line_advance = pitch / 4;
  size_t line_advance_sub = line_advance - size;

  uint32_t *dest = pixels + (x * bpp / 32) + (y * line_advance);

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
 || defined(CONFIG_RENDER_SOFTSCALE) || defined(CONFIG_RENDER_YUV)

void get_screen_coords_scaled(struct graphics_data *graphics, int screen_x,
 int screen_y, int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y)
{
  int window_width = graphics->window_width;
  int window_height = graphics->window_height;
  int target_width;
  int target_height;
  int offset_x;
  int offset_y;

  if(graphics->fullscreen)
  {
    window_width = graphics->resolution_width;
    window_height = graphics->resolution_height;
  }

  target_width = window_width;
  target_height = window_height;

  fix_viewport_ratio(window_width, window_height, &target_width, &target_height,
   graphics->ratio);

  offset_x = (window_width - target_width)/2;
  offset_y = (window_height - target_height)/2;

  *x = CLAMP( (screen_x - offset_x) * 640 / target_width, 0, 639 );
  *y = CLAMP( (screen_y - offset_y) * 350 / target_height, 0, 349 );
  *min_x = 0;
  *min_y = 0;
  *max_x = target_width - 1;
  *max_y = target_height - 1;
}

void set_screen_coords_scaled(struct graphics_data *graphics, int x, int y,
 int *screen_x, int *screen_y)
{
  int window_width = graphics->window_width;
  int window_height = graphics->window_height;
  int target_width;
  int target_height;
  int offset_x;
  int offset_y;

  if(graphics->fullscreen)
  {
    window_width = graphics->resolution_width;
    window_height = graphics->resolution_height;
  }

  fix_viewport_ratio(window_width, window_height, &target_width, &target_height,
   graphics->ratio);

  offset_x = (window_width - target_width)/2;
  offset_y = (window_height - target_height)/2;

  *screen_x = x * target_width / 640 + offset_x;
  *screen_y = y * target_height / 350 + offset_y;
}

#endif /* CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM ||
 CONFIG_RENDER_YUV */

// FIXME: Integerize

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM) \
 || defined(CONFIG_RENDER_SOFTSCALE) || defined(CONFIG_RENDER_YUV) \
 || defined(CONFIG_RENDER_GX)

void fix_viewport_ratio(int width, int height, int *v_width, int *v_height,
 enum ratio_type ratio)
{
  int numerator = 0, denominator = 0;

  *v_width = MAX(width, 1);
  *v_height = MAX(height, 1);

  if(ratio == RATIO_STRETCH)
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
  {
    height = (width * denominator) / numerator;
    *v_height = MAX(height, 1);
  }
  else
  {
    width = (height * numerator) / denominator;
    *v_width = MAX(width, 1);
  }
}

#endif /* CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM ||
 CONFIG_RENDER_YUV || CONFIG_RENDER_GX */

void resize_screen_standard(struct graphics_data *graphics, int w, int h)
{
  graphics->palette_dirty = true;
  update_screen();
}
