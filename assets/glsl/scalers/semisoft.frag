/* MegaZeux
 *
 * Copyright (C) 2017 GreaseMonkey
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

uniform sampler2D baseMap;

varying vec2 vTexcoord;

/* Larger values result in cleaner edges but will cause stretching
 * artifacts to become more apparent. */
#define SHARPNESS         3.0

#define TEX_SCREEN_WIDTH  1024.0
#define TEX_SCREEN_HEIGHT 512.0
#define HALF_PIXEL_X      0.5 / TEX_SCREEN_WIDTH
#define HALF_PIXEL_Y      0.5 / TEX_SCREEN_HEIGHT

void main(void)
{
  const vec2 resolution = vec2(TEX_SCREEN_WIDTH, TEX_SCREEN_HEIGHT);

  /* Select a source texture pixel to shift the current fragment towards.
   *
   * This is NOT the nearest pixel: to get consistent results, every
   * fragment needs to be smoothed in the same direction, so the right and
   * bottom quadrants of a source pixel mix toward the next pixel in their
   * respective directions. Unfortunately, this means parts of the top row
   * and left column of source pixels get clipped.
   */
  vec2 pos = vTexcoord * resolution;
  vec2 mix_point = floor(pos + 0.5) + 0.5;
  vec2 dist = pos - mix_point;

  dist = sign(dist) * pow(abs(dist), vec2(SHARPNESS));
  gl_FragColor = texture2D(baseMap, (mix_point + dist) / resolution);
}
