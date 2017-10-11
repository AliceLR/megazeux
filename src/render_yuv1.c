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

#include "graphics.h"
#include "render.h"
#include "render_layer.h"
#include "render_sdl.h"
#include "render_yuv.h"
#include "renderers.h"

static bool yuv1_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  return yuv_set_video_mode_size(graphics, width, height, depth, fullscreen,
   resize, YUV1_OVERLAY_WIDTH, YUV1_OVERLAY_HEIGHT);
}

static void yuv1_render_graph(struct graphics_data *graphics)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 mode = graphics->screen_mode;
  Uint32 *pixels;
  int pitch;

  yuv_lock_overlay(render_data);

  pixels = yuv_get_pixels_pitch(render_data, &pitch);

  if(!mode)
    render_graph32(pixels, pitch, graphics, set_colors32[mode]);
  else
    render_graph32s(pixels, pitch, graphics, set_colors32[mode]);

  yuv_unlock_overlay(render_data);
}

static void yuv1_render_layer(struct graphics_data *graphics, struct video_layer *layer)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 *pixels;
  int pitch;

  yuv_lock_overlay(render_data);

  pixels = yuv_get_pixels_pitch(render_data, &pitch);

  render_layer(pixels, 32, pitch, graphics, layer);

  yuv_unlock_overlay(render_data);
}

static void yuv1_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 *pixels;
  int pitch;

  yuv_lock_overlay(render_data);

  pixels = yuv_get_pixels_pitch(render_data, &pitch);
  render_cursor(pixels, pitch, 32, x, y,
   graphics->flat_intensity_palette[color], lines, offset);

  yuv_unlock_overlay(render_data);
}

static void yuv1_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 *pixels;
  int pitch;

  yuv_lock_overlay(render_data);

  pixels = yuv_get_pixels_pitch(render_data, &pitch);
  render_mouse(pixels, pitch, 32, x, y, 0xFFFFFFFF, 0x0, w, h);

  yuv_unlock_overlay(render_data);
}

void render_yuv1_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = yuv_init_video;
  renderer->free_video = yuv_free_video;
  renderer->check_video_mode = yuv_check_video_mode;
  renderer->set_video_mode = yuv1_set_video_mode;
  renderer->update_colors = yuv_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_graph = yuv1_render_graph;
  renderer->render_layer = yuv1_render_layer;
  renderer->render_cursor = yuv1_render_cursor;
  renderer->render_mouse = yuv1_render_mouse;
  renderer->sync_screen = yuv_sync_screen;
}
