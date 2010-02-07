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
#include "render_yuv.h"
#include "renderers.h"

static int yuv1_set_video_mode(graphics_data *graphics, int width, int height,
 int depth, int flags, int fullscreen)
{
  return yuv_set_video_mode_size(graphics, width, height, depth, flags,
   fullscreen, YUV1_OVERLAY_WIDTH, YUV1_OVERLAY_HEIGHT);
}

static void yuv1_render_graph(graphics_data *graphics)
{
  yuv_render_data *render_data = graphics->render_data;
  Uint32 mode = graphics->screen_mode;

  SDL_LockYUVOverlay(render_data->overlay);

  if(!mode)
  {
    render_graph32((Uint32 *)render_data->overlay->pixels[0],
     render_data->overlay->pitches[0], graphics, set_colors32[mode]);
  }
  else
  {
    render_graph32s((Uint32 *)render_data->overlay->pixels[0],
     render_data->overlay->pitches[0], graphics, set_colors32[mode]);
  }

  SDL_UnlockYUVOverlay(render_data->overlay);
}

static void yuv1_render_cursor(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 color, Uint8 lines, Uint8 offset)
{
  yuv_render_data *render_data = graphics->render_data;

  SDL_LockYUVOverlay(render_data->overlay);
  render_cursor((Uint32 *)render_data->overlay->pixels[0],
   render_data->overlay->pitches[0], 32, x, y,
   graphics->flat_intensity_palette[color], lines, offset);
  SDL_UnlockYUVOverlay(render_data->overlay);
}

static void yuv1_render_mouse(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 w, Uint8 h)
{
  yuv_render_data *render_data = graphics->render_data;

  SDL_LockYUVOverlay(render_data->overlay);
  render_mouse((Uint32 *)render_data->overlay->pixels[0],
   render_data->overlay->pitches[0], 32, x, y, 0xFFFFFFFF, w, h);
  SDL_UnlockYUVOverlay(render_data->overlay);
}

void render_yuv1_register(graphics_data *graphics)
{
  graphics->init_video = yuv_init_video;
  graphics->check_video_mode = yuv_check_video_mode;
  graphics->set_video_mode = yuv1_set_video_mode;
  graphics->update_colors = yuv_update_colors;
  graphics->resize_screen = resize_screen_standard;
  graphics->remap_charsets = NULL;
  graphics->remap_char = NULL;
  graphics->remap_charbyte = NULL;
  graphics->get_screen_coords = get_screen_coords_scaled;
  graphics->set_screen_coords = set_screen_coords_scaled;
  graphics->render_graph = yuv1_render_graph;
  graphics->render_cursor = yuv1_render_cursor;
  graphics->render_mouse = yuv1_render_mouse;
  graphics->sync_screen = yuv_sync_screen;
}
