/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
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
#include "renderers.h"
#include "util.h"

#include <3ds.h>
#include <sf2d.h>

struct ctr_render_data
{
  sf2d_texture *texture;
  Uint32* buffer;
};

static bool ctr_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct ctr_render_data render_data;

  memset(&render_data, 0, sizeof(struct ctr_render_data));
  graphics->render_data = &render_data;

  render_data.texture = sf2d_create_texture(1024, 512, TEXFMT_RGBA8, SF2D_PLACE_RAM);
  render_data.buffer = linearAlloc((render_data.texture->pow2_w * render_data.texture->pow2_h) * 4);

  render_data.texture->params =
		GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)
		| GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)
		| GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_BORDER)
		| GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_BORDER);

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 32;

  if(graphics->resolution_width < 640)
    graphics->resolution_width = 640;
  if(graphics->resolution_height < 350)
    graphics->resolution_height = 350;
  if(graphics->window_width < 640)
    graphics->window_width = 640;
  if(graphics->window_height < 350)
    graphics->window_height = 350;

  return set_video_mode();
}

static bool ctr_check_video_mode(struct graphics_data *graphics, int width,
  int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static bool ctr_set_video_mode(struct graphics_data *graphics, int width,
  int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static void ctr_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  Uint32 i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
      (palette[i].r << 24) | (palette[i].g << 16) | (palette[i].b << 8) | 0xFF;
  }
}

static void ctr_render_graph(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data = graphics->render_data;

  Uint32 *pixels = render_data->buffer;
  Uint32 pitch = 1024 * 4;
  Uint32 mode = graphics->screen_mode;

  if(!mode)
    render_graph32(pixels, pitch, graphics, set_colors32[mode]);
  else
    render_graph32s(pixels, pitch, graphics, set_colors32[mode]);
}

static void ctr_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct ctr_render_data *render_data = graphics->render_data;

  Uint32 *pixels = render_data->buffer;
  Uint32 pitch = 1024 * 4;
  Uint32 bpp = 32;
  Uint32 flatcolor = graphics->flat_intensity_palette[color];

  render_cursor(pixels, pitch, bpp, x, y, flatcolor, lines, offset);
}

static void ctr_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct ctr_render_data *render_data = graphics->render_data;

  Uint32 *pixels = render_data->buffer;
  Uint32 pitch = 1024 * 4;
  Uint32 bpp = 32;
  Uint32 mask = 0xFFFFFFFF;
  Uint32 amask = 0x000000FF;

  render_mouse(pixels, pitch, bpp, x, y, mask, amask, w, h);
}

extern void sf2d_texture_tile32_hardware(sf2d_texture *texture, const void *data, int w, int h);

static void ctr_sync_screen(struct graphics_data *graphics)
{
  struct ctr_render_data *render_data = graphics->render_data;
  float xs, ys;

  sf2d_start_frame(GFX_TOP, GFX_LEFT);
  render_data->texture->tiled = 0;
  sf2d_texture_tile32_hardware(render_data->texture, render_data->buffer, render_data->texture->pow2_w, render_data->texture->pow2_h);

  xs = 400.0 / 640.0;
  ys = 220.0 / 350.0;
  sf2d_draw_texture_scale(render_data->texture, 0, 10, xs, ys);
  sf2d_end_frame();

  sf2d_swapbuffers();
}

void render_ctr_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = ctr_init_video;
  renderer->check_video_mode = ctr_check_video_mode;
  renderer->set_video_mode = ctr_set_video_mode;
  renderer->update_colors = ctr_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = ctr_render_graph;
  renderer->render_cursor = ctr_render_cursor;
  renderer->render_mouse = ctr_render_mouse;
  renderer->sync_screen = ctr_sync_screen;
}
