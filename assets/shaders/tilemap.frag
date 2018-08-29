/* MegaZeux
 *
 * Copyright (C) 2008 Joel Bouchard Lamontagne <logicow@gmail.com>
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#version 110

// Keep these the same as in render_glsl.c
#define CHARSET_COLS      64.0
#define TEX_DATA_WIDTH    512.0
#define TEX_DATA_HEIGHT   1024.0
#define TEX_DATA_PAL_Y    896.0
#define TEX_DATA_LAYER_X  0.0
#define TEX_DATA_LAYER_Y  901.0

// This has to be slightly less than 14.0 to avoid propagating error
// with some very old driver/video card combos.
#define CHAR_H            13.99999

uniform sampler2D baseMap;

varying vec2 vTexcoord;

float fract_(float v)
{
  return clamp(fract(v + 0.001) - 0.001, 0.000, 0.999);
}

float floor_(float v)
{
  return floor(v + 0.001);
}

// FIXME magic
float layer_get_char(vec4 layer_data)
{
  return floor_(layer_data.z * 63.75) + layer_data.w * 255.0 * 64.0;
}

// FIXME magic
float layer_get_fg_color(vec4 layer_data)
{
  return layer_data.x * 0.5 + fract_(layer_data.y * 127.501);
}

// FIXME magic
float layer_get_bg_color(vec4 layer_data)
{
  return floor_(layer_data.y * 127.5) / 512.0 + fract_(layer_data.z * 63.751);
}

void main(void)
{
  /**
   * Get the packed char/color data for this position from the current layer.
   * vTexcoord will be provided in the range of x=[0..layer.w), y=[0..layer.h).
   */
  float layer_x = (vTexcoord.x + TEX_DATA_LAYER_X) / TEX_DATA_WIDTH;
  float layer_y = (vTexcoord.y + TEX_DATA_LAYER_Y) / TEX_DATA_HEIGHT;
  vec4 layer_data = texture2D(baseMap, vec2(layer_x, layer_y));

  /**
   * Get the current char and its base position in the texture charset.
   * The x position will be normalized to the width of the texture,
   * but for the y position it's easier to get the pixel position and
   * normalize afterward.
   */
  float chr_idx = layer_get_char(layer_data);
  float chr_x = fract_(chr_idx / CHARSET_COLS);
  float chr_y = floor_(chr_idx / CHARSET_COLS);

  /**
   * Get the current pixel value of the current char from the texture.
   */
  float char_pix_x = chr_x + fract_(vTexcoord.x) / CHARSET_COLS;
  float char_pix_y = (chr_y + fract_(vTexcoord.y)) * CHAR_H / TEX_DATA_HEIGHT;
  vec4 char_pix = texture2D(baseMap, vec2(char_pix_x, char_pix_y));

  /**
   * Determine whether this is the foreground or background color of the char,
   * get that color from the texture, and output it.
   */
  float color;

  // We could actually check any component here.
  if(char_pix.x > 0.5)
  {
    color = layer_get_fg_color(layer_data);
  }
  else
  {
    color = layer_get_bg_color(layer_data);
  }

  gl_FragColor =
   texture2D(baseMap, vec2(color, TEX_DATA_PAL_Y/TEX_DATA_HEIGHT));
}
