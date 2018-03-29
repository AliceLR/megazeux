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

#include "../../src/graphics.h"
#include "../../src/pngops.h"
#include "../../src/render.h"
#include "../../src/renderers.h"
#include "../../src/render_layer.h"
#include "../../src/util.h"

#include "event.h"
#include "platform.h"
#include "render.h"

static bool nx_init_video(struct graphics_data *graphics, struct config_info *conf)
{
  static struct nx_render_data render_data;

  memset(&render_data, 0, sizeof(struct nx_render_data));
  graphics->render_data = &render_data;

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 32;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  return set_video_mode();
}

static void nx_free_video(struct graphics_data *graphics)
{
}

static bool nx_check_video_mode(struct graphics_data *graphics, int width,
  int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static bool nx_set_video_mode(struct graphics_data *graphics, int width,
  int height, int depth, bool fullscreen, bool resize)
{
  return true;
}

static void nx_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  Uint32 i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
      (palette[i].r) | (palette[i].g << 8) | (palette[i].b << 16) | 0xFF000000;
  }
}

static void nx_render_graph(struct graphics_data *graphics)
{
  u32 width, height;
  u32* pixels = (u32*) gfxGetFramebuffer((u32*)&width, (u32*)&height);
  Uint32 mode = graphics->screen_mode;

  pixels += width * 5; // centering

  if(!mode)
    render_graph32(pixels, width * 4, graphics, set_colors32[mode]);
  else
    render_graph32s(pixels, width * 4, graphics, set_colors32[mode]);
}

static void nx_render_layer(struct graphics_data *graphics, struct video_layer *vlayer)
{
  u32 width, height;
  u32* pixels = (u32*) gfxGetFramebuffer((u32*)&width, (u32*)&height);
  pixels += width * 5; // centering

  render_layer(pixels, 32, width * 4, graphics, vlayer);
}

static void nx_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint16 color, Uint8 lines, Uint8 offset)
{
  u32 width, height;
  u32* pixels = (u32*) gfxGetFramebuffer((u32*)&width, (u32*)&height);
  pixels += width * 5; // centering

  render_cursor(pixels, width * 4, 32, x, y, graphics->flat_intensity_palette[color], lines, offset);
}

static void nx_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  u32 width, height;
  u32* pixels = (u32*) gfxGetFramebuffer((u32*)&width, (u32*)&height);
  pixels += width * 5; // centering

  render_mouse(pixels, width * 4, 32, x, y, 0xFFFFFFFF, 0xFF000000, w, h);
}

static void nx_sync_screen(struct graphics_data *graphics)
{
  gfxFlushBuffers();
  gfxSwapBuffers();
}

void render_nx_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = nx_init_video;
  renderer->free_video = nx_free_video;
  renderer->check_video_mode = nx_check_video_mode;
  renderer->set_video_mode = nx_set_video_mode;
  renderer->update_colors = nx_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = nx_render_graph;
  renderer->render_layer = nx_render_layer;
  renderer->render_cursor = nx_render_cursor;
  renderer->render_mouse = nx_render_mouse;
  renderer->sync_screen = nx_sync_screen;
}

