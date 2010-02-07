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

#include "config.h"
#include "graphics.h"
#include "render.h"
#include "render_yuv.h"
#include "renderers.h"

static void yuv2_set_colors_mzx(graphics_data *graphics, Uint32 *char_colors,
 Uint8 bg, Uint8 fg)
{
  yuv_render_data *render_data = graphics->render_data;
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

static void (*yuv2_set_colors[4])(graphics_data *, Uint32 *, Uint8, Uint8) =
{
  yuv2_set_colors_mzx,
  set_colors32_smzx,
  set_colors32_smzx,
  set_colors32_smzx3
};

static int yuv2_set_video_mode(graphics_data *graphics, int width, int height,
 int depth, int flags, int fullscreen)
{
  yuv_render_data *render_data = graphics->render_data;

  if(yuv_set_video_mode_size(graphics, width, height, depth, flags,
   fullscreen, YUV2_OVERLAY_WIDTH, YUV2_OVERLAY_HEIGHT))
  {
    if (render_data->overlay->format == SDL_YUY2_OVERLAY)
    {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
      render_data->y0mask = 0xFF000000;
      render_data->y1mask = 0x0000FF00;
#else
      render_data->y0mask = 0x000000FF;
      render_data->y1mask = 0x00FF0000;
#endif
    }
    else
    {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
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
  else
    return false;
}

static void yuv2_render_graph(graphics_data *graphics)
{
  yuv_render_data *render_data = graphics->render_data;

  SDL_LockYUVOverlay(render_data->overlay);

  render_graph16((Uint16 *)render_data->overlay->pixels[0],
   render_data->overlay->pitches[0], graphics,
   yuv2_set_colors[graphics->screen_mode]);

  SDL_UnlockYUVOverlay(render_data->overlay);
}

static void yuv2_render_cursor(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 color, Uint8 lines, Uint8 offset)
{
  yuv_render_data *render_data = graphics->render_data;

  SDL_LockYUVOverlay(render_data->overlay);
  render_cursor((Uint32 *)render_data->overlay->pixels[0],
   render_data->overlay->pitches[0], 16, x, y,
   graphics->flat_intensity_palette[color], lines, offset);
  SDL_UnlockYUVOverlay(render_data->overlay);
}

static void yuv2_render_mouse(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 w, Uint8 h)
{
  yuv_render_data *render_data = graphics->render_data;

  SDL_LockYUVOverlay(render_data->overlay);
  render_mouse((Uint32 *)render_data->overlay->pixels[0],
   render_data->overlay->pitches[0], 16, x, y, 0xFFFFFFFF, w, h);
  SDL_UnlockYUVOverlay(render_data->overlay);
}

void render_yuv2_register(graphics_data *graphics)
{
  graphics->init_video = yuv_init_video;
  graphics->check_video_mode = yuv_check_video_mode;
  graphics->set_video_mode = yuv2_set_video_mode;
  graphics->update_colors = yuv_update_colors;
  graphics->resize_screen = resize_screen_standard;
  graphics->remap_charsets = NULL;
  graphics->remap_char = NULL;
  graphics->remap_charbyte = NULL;
  graphics->get_screen_coords = get_screen_coords_scaled;
  graphics->set_screen_coords = set_screen_coords_scaled;
  graphics->render_graph = yuv2_render_graph;
  graphics->render_cursor = yuv2_render_cursor;
  graphics->render_mouse = yuv2_render_mouse;
  graphics->sync_screen = yuv_sync_screen;
}
