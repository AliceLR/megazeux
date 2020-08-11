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

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#endif

// Keep these the same as in render_glsl.c
#define CHARSET_COLS      64.0
#define CHARSET_ROWS_EACH 4.0
#define TEX_DATA_WIDTH    512.0
#define TEX_DATA_HEIGHT   1024.0
#define TEX_DATA_PAL_Y    896.0
#define TEX_DATA_LAYER_X  0.0
#define TEX_DATA_LAYER_Y  901.0

// This has to be slightly less than 14.0 to avoid propagating error
// with some very old driver/video card combos.
#define CHAR_H            13.99999

// Relative position of the protected palette seen by this shader.
#define PROTECTED_PALETTE 16.0

uniform sampler2D baseMap;
uniform float protected_pal_position;

varying vec2 vTexcoord;

float fract_(float v)
{
  return clamp(fract(v + 0.001) - 0.001, 0.000, 0.999);
}

float floor_(float v)
{
  return floor(v + 0.001);
}

int int_(float v)
{
  return int(v + 0.01);
}

// NOTE: Layer data packing scheme
// This is intentionally wasteful so the components don't interfere with each
// other on old and embedded cards with poor float precision. The colors are
// encoded to skip SMZX exclusive colors so they only require one byte each.
// C = character
// B = background color (0-15 normal; >=16 protected)
// F = foreground color (0-15 normal; >=16 protected)
// w        z        y        x
// 00000000 00000000 00000000 00000000
// CCCCCCCC CCCCCCCC BBBBBBBB FFFFFFFF
#define PACK_COLOR_FG  x
#define PACK_COLOR_BG  y
#define PACK_CHAR      z
#define PACK_CHARSET   w

// Some older cards/drivers tend to be slightly off; slight variations
// in values here are intentional.

float layer_unpack(float layer_data)
{
  return layer_data * 255.001;
}

float layer_unpack_color(float color_data)
{
  if(color_data * 255.001 >= (PROTECTED_PALETTE - 0.001))
    return (protected_pal_position - PROTECTED_PALETTE) + color_data * 255.001;

  return color_data * 255.001;
}

void main(void)
{
  /**
   * Get the packed char/color data for this position from the current layer.
   * vTexcoord will be provided in the range of x=[0..layer.w), y=[0..layer.h).
   * Note that floor() is not required on vTexcoord since the texture filtering
   * is set to GL_NEAREST (floor() also causes bugs here on some old drivers).
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
  float char_num = layer_unpack(layer_data.PACK_CHAR);
  float char_set = layer_unpack(layer_data.PACK_CHARSET);
  float char_x = fract_(char_num / CHARSET_COLS);
  float char_y = floor_(char_num / CHARSET_COLS) + char_set * CHARSET_ROWS_EACH;

  /**
   * Get the current pixel value of the current char from the texture.
   */
  float char_tex_x = char_x + fract_(vTexcoord.x) / CHARSET_COLS;
  float char_tex_y = (char_y + fract_(vTexcoord.y)) * CHAR_H / TEX_DATA_HEIGHT;
  vec4 char_pix = texture2D(baseMap, vec2(char_tex_x, char_tex_y));

  /**
   * Determine whether this is the foreground or background color of the char,
   * get that color from the texture, and output it.
   */
  float color;

  // We could actually check any component here.
  if(char_pix.x > 0.5)
  {
    color = layer_unpack_color(layer_data.PACK_COLOR_FG);
  }
  else
  {
    color = layer_unpack_color(layer_data.PACK_COLOR_BG);
  }

  gl_FragColor = texture2D(baseMap,
   vec2(color / TEX_DATA_WIDTH, TEX_DATA_PAL_Y / TEX_DATA_HEIGHT));
}
