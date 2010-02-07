/* MegaZeux
 *
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

#ifndef __RENDER_H
#define __RENDER_H

#include "compat.h"

__M_BEGIN_DECLS

#include "graphics.h"

void set_colors8_mzx (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);
void set_colors8_smzx (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);
void set_colors8_smzx3 (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);
void set_colors16_mzx (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);
void set_colors16_smzx (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);
void set_colors16_smzx3 (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);
void set_colors32_mzx (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);
void set_colors32_smzx (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);
void set_colors32_smzx3 (graphics_data *graphics, Uint32 *char_colors, Uint8 bg,
 Uint8 fg);

void render_graph8(Uint8 *pixels, Uint32 pitch, graphics_data *graphics,
 void (*set_colors)(graphics_data *, Uint32 *, Uint8, Uint8));
void render_graph16(Uint16 *pixels, Uint32 pitch, graphics_data *graphics,
 void (*set_colors)(graphics_data *, Uint32 *, Uint8, Uint8));
void render_graph32(Uint32 *pixels, Uint32 pitch, graphics_data *graphics,
 void (*set_colors)(graphics_data *, Uint32 *, Uint8, Uint8));
void render_graph32s(Uint32 *pixels, Uint32 pitch, graphics_data *graphics,
 void (*set_colors)(graphics_data *, Uint32 *, Uint8, Uint8));

void render_cursor(Uint32 *pixels, Uint32 pitch, Uint8 bpp, Uint32 x, Uint32 y,
 Uint32 color, Uint8 lines, Uint8 offset);
void render_mouse(Uint32 *pixels, Uint32 pitch, Uint8 bpp, Uint32 x, Uint32 y,
 Uint32 mask, Uint8 w, Uint8 h);

void get_screen_coords_centered(graphics_data *graphics, int screen_x,
 int screen_y, int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y);
void get_screen_coords_scaled(graphics_data *graphics, int screen_x,
 int screen_y, int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y);

void set_screen_coords_centered(graphics_data *graphics, int x, int y,
 int *screen_x, int *screen_y);
void set_screen_coords_scaled(graphics_data *graphics, int x, int y,
 int *screen_x, int *screen_y);

void resize_screen_standard(graphics_data *graphics, int w, int h);

__M_END_DECLS

#endif // __RENDER_H
