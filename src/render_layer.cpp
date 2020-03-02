/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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
#include <assert.h>

#include "platform.h"
#include "graphics.h"
#include "render_layer.h"

// Skip unused variants to reduce compile time on these platforms.
#if defined(CONFIG_WII) || defined(ANDROID) || defined(__EMSCRIPTEN__)
#define SKIP_8BPP
#define SKIP_16BPP
#endif

// Not exactly clear how much Emscripten benefits from these and they're
// doubling the number of renderers.
#if defined(__EMSCRIPTEN__)
#define SKIP_64_ALIGN
#endif

#include "render_layer_code.hpp"

#if 0
// This layer renderer is very slow, but it should work properly.
// The renderers in render_layer_code.h should generally be used instead.

static inline void reference_renderer(Uint32 *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer)
{
  Uint32 mode = layer->mode;

  Uint32 ch_x, ch_y;
  Uint16 c;

  struct char_element *src = layer->data;
  int row, col;
  int tcol = layer->transparent_col;

  Uint8 *char_ptr;
  Uint8 current_char_byte;

  Uint32 *drawPtr;
  Uint32 pix;
  int pix_pos;
  //Uint32 advance = (pitch / 4) - 8;

  int x, y, i;

  Uint32 char_colors[4];
  int char_idx[4];
  int idx;

  Uint32 protected_pal_position = graphics->protected_pal_position;

  for(ch_y = 0; ch_y < layer->h; ch_y++)
  {
    for(ch_x = 0; ch_x < layer->w; ch_x++)
    {
      c = src->char_value;
      if(c != INVISIBLE_CHAR)
      {
        // Char values of 256+, prior to offsetting, are from the protected set
        if(c > 0xFF)
        {
          c = (c & 0xFF) + PROTECTED_CHARSET_POSITION;
        }
        else
        {
          c += layer->offset;
          c %= PROTECTED_CHARSET_POSITION;
        }

        if(mode)
        {
          for(i = 0; i < 4; i++)
          {
            idx = ((src->bg_color & 0xF) << 4 | (src->fg_color & 0xF)) * 4 + i;
            char_idx[i] = graphics->smzx_indices[idx];
            char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
          }
        }
        else
        {
          if(src->bg_color >= 16)
            char_idx[0] = (src->bg_color - 16) % 16 + protected_pal_position;
          else
            char_idx[0] = src->bg_color;

          if(src->fg_color >= 16)
            char_idx[1] = (src->fg_color - 16) % 16 + protected_pal_position;
          else
            char_idx[1] = src->fg_color;

          for(i = 0; i < 2; i++)
            char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
        }

        char_ptr = graphics->charset + (c * CHAR_H);
        for(row = 0; row < CHAR_H; row++)
        {
          current_char_byte = *char_ptr;
          if(mode == 0)
          {
            for(col = 0; col < CHAR_W; col++)
            {
              y = layer->y + ch_y * 14 + row;
              x = layer->x + ch_x * 8 + col;

              if(x >= 0 && x < SCREEN_PIX_W && y >= 0 && y < SCREEN_PIX_H)
              {
                drawPtr = pixels + (pitch / sizeof(Uint32)) * y + x;

                pix_pos = (current_char_byte & (0x80 >> col)) << col >> 7;

                if(char_idx[pix_pos] != tcol)
                  *drawPtr = char_colors[pix_pos];
              }
            }
          }
          else
          {
            for(col = 0; col < CHAR_W; col+=2)
            {
              y = layer->y + ch_y * 14 + row;
              x = layer->x + ch_x * 8 + col;

              if(x >= 0 && x < SCREEN_PIX_W && y >= 0 && y < SCREEN_PIX_H)
              {
                drawPtr = pixels + (pitch / sizeof(Uint32)) * y + x;

                pix_pos = (current_char_byte & (0xC0 >> col)) << col >> 6;

                if(char_idx[pix_pos] != tcol)
                {
                  pix = char_colors[pix_pos];
                  *drawPtr = pix;
                  if(x < SCREEN_PIX_W - 1)
                    *(++drawPtr) = pix;
                }
              }
            }
          }
          char_ptr++;
        }
      }
      src++;
    }
  }
}
#endif

void render_layer_32bpp(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer)
{
  render_layer(pixels, 32, pitch, graphics, layer);
}

void render_layer_16bpp(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer)
{
  render_layer(pixels, 16, pitch, graphics, layer);
}

void render_layer_8bpp(void *pixels, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer)
{
  render_layer(pixels, 8, pitch, graphics, layer);
}

void render_layer(void *pixels, int force_bpp, Uint32 pitch,
 struct graphics_data *graphics, struct video_layer *layer)
{
  //reference_renderer(pixels, pitch, graphics, layer); return;

  int smzx = layer->mode;
  int trans = layer->transparent_col != -1;
  size_t drawStart;
  int align = 8;
  int clip = 0;
  int ppal = graphics->protected_pal_position;

  if(layer->x < 0 || layer->y < 0 ||
   (layer->x + layer->w * CHAR_W) > SCREEN_PIX_W ||
   (layer->y + layer->h * CHAR_H) > SCREEN_PIX_H)
    clip = 1;

  if(force_bpp == -1)
    force_bpp = graphics->bits_per_pixel;

  drawStart =
   (size_t)((char *)pixels + layer->y * pitch + (layer->x * force_bpp / 8));

  /**
   * Select the highest pixel align the current platform is capable of.
   * Additionally, to simplify the renderer code, the align must also be
   * capable of addressing the first pixel of the draw (e.g. a 64-bit platform
   * will use a 32-bit align if the layer is on an odd horizontal pixel).
   */
#ifndef SKIP_64_ALIGN
  if((sizeof(size_t) >= sizeof(Uint64)) && ((drawStart % sizeof(Uint64)) == 0))
  {
    align = 64;
  }
  else
#endif

  if((sizeof(size_t) >= sizeof(Uint32)) && ((drawStart % sizeof(Uint32)) == 0))
  {
    align = 32;
  }
  else

  if((sizeof(size_t) >= sizeof(Uint16)) && ((drawStart % sizeof(Uint16)) == 0))
  {
    align = 16;
  }

  render_layer_func(pixels, pitch, graphics, layer,
   force_bpp, align, smzx, ppal, trans, clip);
}
