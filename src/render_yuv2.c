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

#include "platform.h"
#include "graphics.h"
#include "render.h"
#include "render_yuv.h"
#include "renderers.h"

static bool yuv2_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  struct yuv_render_data *render_data = graphics->render_data;

  if(!yuv_set_video_mode_size(graphics, width, height, depth, fullscreen,
   resize, YUV2_OVERLAY_WIDTH, YUV2_OVERLAY_HEIGHT))
    return false;

  if(render_data->is_yuy2)
  {
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
    render_data->y0mask = 0xFF000000;
    render_data->y1mask = 0x0000FF00;
#else
    render_data->y0mask = 0x000000FF;
    render_data->y1mask = 0x00FF0000;
#endif
  }
  else
  {
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
    render_data->y0mask = 0x00FF0000;
    render_data->y1mask = 0x000000FF;
#else
    render_data->y0mask = 0x0000FF00;
    render_data->y1mask = 0xFF000000;
#endif
  }

  render_data->uvmask = ~(render_data->y0mask | render_data->y1mask);
  return true;
}

static void yuv2_set_colors_mzx(struct graphics_data *graphics,
 Uint32 *char_colors, Uint8 bg, Uint8 fg)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 y0mask = render_data->y0mask;
  Uint32 y1mask = render_data->y1mask;
  Uint32 uvmask = render_data->uvmask;
  Uint32 cb_bg, cb_fg, cb_mix;

  cb_bg = graphics->flat_intensity_palette[bg];
  cb_fg = graphics->flat_intensity_palette[fg];
  cb_mix = (((cb_bg & uvmask) >> 1) + ((cb_fg & uvmask) >> 1)) & uvmask;

  char_colors[0] = cb_bg;
  char_colors[1] = cb_mix | (cb_bg & y0mask) | (cb_fg & y1mask);
  char_colors[2] = cb_mix | (cb_fg & y0mask) | (cb_bg & y1mask);
  char_colors[3] = cb_fg;
}

static void yuv2_render_graph(struct graphics_data *graphics)
{
  struct yuv_render_data *render_data = graphics->render_data;
  int mode = graphics->screen_mode;
  Uint32 *pixels;
  int pitch;

  yuv_lock_overlay(render_data);

  pixels = yuv_get_pixels_pitch(render_data, &pitch);

  if(!mode)
    render_graph16((Uint16 *)pixels, pitch, graphics, yuv2_set_colors_mzx);
  else
    render_graph16((Uint16 *)pixels, pitch, graphics, set_colors32[mode]);

  yuv_unlock_overlay(render_data);
}

static void yuv2_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 *pixels;
  int pitch;

  // FIXME no layer support
  color &= 0xFF;
  // end FIXME

  yuv_lock_overlay(render_data);

  pixels = yuv_get_pixels_pitch(render_data, &pitch);
  render_cursor(pixels, pitch, 16, x, y,
   graphics->flat_intensity_palette[color], lines, offset);

  yuv_unlock_overlay(render_data);
}

static void yuv2_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 *pixels;
  int pitch;

  yuv_lock_overlay(render_data);

  pixels = yuv_get_pixels_pitch(render_data, &pitch);
  render_mouse(pixels, pitch, 16, x, y, 0xFFFFFFFF, 0x0, w, h);

  yuv_unlock_overlay(render_data);
}

void render_yuv2_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = yuv_init_video;
  renderer->free_video = yuv_free_video;
  renderer->check_video_mode = yuv_check_video_mode;
  renderer->set_video_mode = yuv2_set_video_mode;
  renderer->update_colors = yuv_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_graph = yuv2_render_graph;
  renderer->render_cursor = yuv2_render_cursor;
  renderer->render_mouse = yuv2_render_mouse;
  renderer->sync_screen = yuv_sync_screen;
}
