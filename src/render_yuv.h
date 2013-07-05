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

struct yuv_render_data
{
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  SDL_Window *window;
#else
  SDL_Overlay *overlay;
#endif
  SDL_Surface *screen;
  Uint32 y0mask;
  Uint32 y1mask;
  Uint32 uvmask;
  enum ratio_type ratio;
  bool is_yuy2;
};

bool yuv_set_video_mode_size(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize,
 int yuv_width, int yuv_height);
bool yuv_init_video(struct graphics_data *graphics, struct config_info *conf);
void yuv_free_video(struct graphics_data *graphics);
bool yuv_check_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize);
void yuv_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count);
void yuv_sync_screen(struct graphics_data *graphics);
void yuv_lock_overlay(struct yuv_render_data *render_data);
void yuv_unlock_overlay(struct yuv_render_data *render_data);
Uint32 *yuv_get_pixels_pitch(struct yuv_render_data *render_data, int *pitch);

__M_END_DECLS

#endif // __RENDER_YUV_H
