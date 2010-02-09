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

#ifndef __RENDER_YUV_H
#define __RENDER_YUV_H

#define SOFT_SOURCE_WIDTH    640
#define SOFT_SOURCE_HEIGHT   350

#define YUV1_OVERLAY_WIDTH   (SOFT_SOURCE_WIDTH * 2)
#define YUV1_OVERLAY_HEIGHT  SOFT_SOURCE_HEIGHT

#define YUV2_OVERLAY_WIDTH   SOFT_SOURCE_WIDTH
#define YUV2_OVERLAY_HEIGHT  SOFT_SOURCE_HEIGHT

#include "compat.h"

#include "graphics.h"

#include "SDL.h"

__M_BEGIN_DECLS

typedef struct
{
  SDL_Surface *screen;
  SDL_Overlay *overlay;
  Uint32 y0mask;
  Uint32 y1mask;
  Uint32 uvmask;
} yuv_render_data;

bool yuv_set_video_mode_size(graphics_data *graphics, int width, int height,
 int depth, int fullscreen, int resize, int yuv_width, int yuv_height);
bool yuv_init_video(graphics_data *graphics, config_info *conf);
void yuv_free_video(graphics_data *graphics);
bool yuv_check_video_mode(graphics_data *graphics,  int width, int height,
 int depth, int fullscreen, int resize);
void yuv_update_colors(graphics_data *graphics, rgb_color *palette,
 Uint32 count);
void yuv_sync_screen (graphics_data *graphics);

__M_END_DECLS

#endif // __RENDER_YUV_H
