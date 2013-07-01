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

extern void (*const set_colors8[4])(struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg);
extern void (*const set_colors16[4])(struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg);
extern void (*const set_colors32[4])(struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg);
#ifdef CONFIG_RENDER_YUV
extern void (*const yuv2_set_colors[4])(struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg);
#endif

void render_graph8(Uint8 *pixels, Uint32 pitch,
 struct graphics_data *graphics,
 void (*set_colors)(struct graphics_data *, Uint32 *, Uint8, Uint8));
void render_graph16(Uint16 *pixels, Uint32 pitch,
 struct graphics_data *graphics,
 void (*set_colors)(struct graphics_data *, Uint32 *, Uint8, Uint8));
void render_graph32(Uint32 *pixels, Uint32 pitch,
 struct graphics_data *graphics,
 void (*set_colors)(struct graphics_data *, Uint32 *, Uint8, Uint8));
void render_graph32s(Uint32 *pixels, Uint32 pitch,
 struct graphics_data *graphics,
 void (*set_colors)(struct graphics_data *, Uint32 *, Uint8, Uint8));

void render_cursor(Uint32 *pixels, Uint32 pitch, Uint8 bpp, Uint32 x, Uint32 y,
 Uint32 color, Uint8 lines, Uint8 offset);
void render_mouse(Uint32 *pixels, Uint32 pitch, Uint8 bpp, Uint32 x, Uint32 y,
 Uint32 mask, Uint32 amask, Uint8 w, Uint8 h);

void get_screen_coords_centered(struct graphics_data *graphics, int screen_x,
 int screen_y, int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y);
void set_screen_coords_centered(struct graphics_data *graphics, int x, int y,
 int *screen_x, int *screen_y);

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM) \
 || defined(CONFIG_RENDER_YUV)

void get_screen_coords_scaled(struct graphics_data *graphics, int screen_x,
 int screen_y, int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y);
void set_screen_coords_scaled(struct graphics_data *graphics, int x, int y,
 int *screen_x, int *screen_y);

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM || CONFIG_RENDER_YUV

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM) \
 || defined(CONFIG_RENDER_YUV) || defined(CONFIG_RENDER_GX)

void fix_viewport_ratio(int width, int height, int *v_width, int *v_height,
 enum ratio_type ratio);

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM || CONFIG_RENDER_YUV || CONFIG_RENDER_GX

void resize_screen_standard(struct graphics_data *graphics, int w, int h);

__M_END_DECLS

#endif // __RENDER_H
