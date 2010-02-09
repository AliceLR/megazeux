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
 int width, int height, int depth, int fullscreen, int resize)
{
  struct yuv_render_data *render_data = graphics->render_data;

  if(yuv_set_video_mode_size(graphics, width, height, depth, fullscreen,
   resize, YUV2_OVERLAY_WIDTH, YUV2_OVERLAY_HEIGHT))
  {
    if(render_data->overlay->format == SDL_YUY2_OVERLAY)
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

  return false;
}

static void yuv2_render_graph(struct graphics_data *graphics)
{
  struct yuv_render_data *render_data = graphics->render_data;

  SDL_LockYUVOverlay(render_data->overlay);

  render_graph16((Uint16 *)render_data->overlay->pixels[0],
   render_data->overlay->pitches[0], graphics,
   yuv2_set_colors[graphics->screen_mode]);

  SDL_UnlockYUVOverlay(render_data->overlay);
}

static void yuv2_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct yuv_render_data *render_data = graphics->render_data;

  SDL_LockYUVOverlay(render_data->overlay);
  render_cursor((Uint32 *)render_data->overlay->pixels[0],
   render_data->overlay->pitches[0], 16, x, y,
   graphics->flat_intensity_palette[color], lines, offset);
  SDL_UnlockYUVOverlay(render_data->overlay);
}

static void yuv2_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct yuv_render_data *render_data = graphics->render_data;

  SDL_LockYUVOverlay(render_data->overlay);
  render_mouse((Uint32 *)render_data->overlay->pixels[0],
   render_data->overlay->pitches[0], 16, x, y, 0xFFFFFFFF, w, h);
  SDL_UnlockYUVOverlay(render_data->overlay);
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
