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

uniform sampler2D baseMap;

varying vec2 vTexcoord;

// Keep these the same as in render_glsl.c
#define CHARSET_COLS      64.0
#define CHARSET_ROWS_EACH 4.0
#define TEX_DATA_WIDTH    512.0
#define TEX_DATA_HEIGHT   1024.0
#define TEX_DATA_PAL_Y    896.0
#define TEX_DATA_IDX_Y    897.0
#define TEX_DATA_LAYER_X  0.0
#define TEX_DATA_LAYER_Y  901.0

#define COLOR_TRANSPARENT (272.0 - 0.001)
#define PIXEL_X           1.0 / TEX_DATA_WIDTH

// This has to be slightly less than 14.0 to avoid propagating error
// with some very old driver/video card combos.
#define CHAR_W            8.0
#define CHAR_H            13.99999
#define CHAR_W_I          8
#define CHAR_H_I          14

float fract_(float v)
{
  return clamp(fract(v + 0.001) - 0.001, 0.000, 0.999);
}

float floor_(float v)
{
  return floor(v + 0.001);
}

float mod_(float v, float r)
{
  return clamp(mod(v + 0.01, r) - 0.01, 0.0, r);
}

int int_(float v)
{
  return int(v + 0.01);
}

// NOTE: Layer data packing scheme.
// This is intentionally wasteful so the components don't interfere with each
// other on old and embedded cards with poor float precision.
// C = char.
// P = subpalette (or a fully transparent char if any high bits are set).
// w        z        y        x
// 00000000 00000000 00000000 00000000
// CCCCCCCC CCCCCCCC PPPPPPPP PPPPPPPP
#define PACK_SUBPAL_LO x
#define PACK_SUBPAL_HI y
#define PACK_CHAR      z
#define PACK_CHARSET   w

// Some older cards/drivers tend to be slightly off; slight variations
// in values here are intentional.

float layer_unpack(float layer_data)
{
  return layer_data * 255.001;
}

void main( void )
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

  // If any of the high subpalette bits are set, display a transparent pixel...
  if(layer_unpack(layer_data.PACK_SUBPAL_HI) >= 0.999)
  {
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
  }
  else
  {
    /**
     * The math in this shader is more complex than the math in the regular
     * shader, so it's handled partly in integers instead of floats. Older cards
     * seem less bad at calculating ints than floats (albeit slower too).
     *
     * Get the current char and:
     *
     * 1) determine the pixel position in the char we should be looking at.
     * We're considering pairs of two pixels horizontally adjacent, so get
     * the position of the left pixel in the pair specifically.
     *
     * 2) Calculate the position on the texture in pixel scale where the
     * current char is and add the offset from step 1.
     */
    float char_num = layer_unpack(layer_data.PACK_CHAR);
    float char_set = layer_unpack(layer_data.PACK_CHARSET);

    int px = int_(fract_(vTexcoord.x) * CHAR_W) / 2 * 2;

    int char_x = int_(mod_(char_num, CHARSET_COLS) * CHAR_W) + px;
    float char_y = floor_(char_num / CHARSET_COLS) + (char_set * CHARSET_ROWS_EACH);

    /**
     * Get the current pixels of the current char from the texture.
     * Together, these determine which color of the current subpalette to use.
     */
    float char_tex_x = float(char_x) / TEX_DATA_WIDTH;
    float char_tex_y = (char_y + fract_(vTexcoord.y)) * CHAR_H / TEX_DATA_HEIGHT;

    vec4 char_pix_l = texture2D(baseMap, vec2(char_tex_x, char_tex_y));
    vec4 char_pix_r = texture2D(baseMap, vec2(char_tex_x + PIXEL_X, char_tex_y));

    /**
     * Determine the SMZX subpalette and SMZX index of the current pixel,
     * get that color from the texture, and output it.
     */

    int smzx_col;

    // We could actually check any component here.
    if(char_pix_l.x < 0.5)
    {
      if(char_pix_r.x < 0.5)
      {
        smzx_col = 0;
      }
      else
      {
        smzx_col = 1;
      }
    }
    else
    {
      if(char_pix_r.x < 0.5)
      {
        smzx_col = 2;
      }
      else
      {
        smzx_col = 3;
      }
    }

    float subpalette = layer_unpack(layer_data.PACK_SUBPAL_LO);
    float smzx_tex_x = subpalette / TEX_DATA_WIDTH;
    float smzx_tex_y = (float(smzx_col) + TEX_DATA_IDX_Y) / TEX_DATA_HEIGHT;

    // NOTE: This must use the x component.
    float real_col = texture2D(baseMap, vec2(smzx_tex_x, smzx_tex_y)).x * 255.001;

    gl_FragColor = texture2D(baseMap,
     vec2(real_col / TEX_DATA_WIDTH, TEX_DATA_PAL_Y / TEX_DATA_HEIGHT));
  }
}
