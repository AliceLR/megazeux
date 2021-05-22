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

#include <stdlib.h>

#include "graphics.h"
#include "render.h"
#include "render_layer.h"
#include "render_sdl.h"
#include "renderers.h"
#include "util.h"
#include "yuv.h"

#if SDL_VERSION_ATLEAST(2,0,0)
#error Overlay renderer no longer supported for SDL 2+. Use softscale!
#endif

#define YUV1_OVERLAY_WIDTH   (SCREEN_PIX_W * 2)
#define YUV1_OVERLAY_HEIGHT  SCREEN_PIX_H

#define YUV2_OVERLAY_WIDTH   SCREEN_PIX_W
#define YUV2_OVERLAY_HEIGHT  SCREEN_PIX_H

struct yuv_render_data
{
  struct sdl_render_data sdl;
  set_colors_function set_colors_mzx;
  Uint32 (*rgb_to_yuv)(Uint8 r, Uint8 g, Uint8 b);
  Uint32 bpp;
  Uint32 w;
  Uint32 h;
};

static boolean yuv_set_video_mode_size(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize,
 int yuv_width, int yuv_height)
{
  struct yuv_render_data *render_data = graphics->render_data;

  if(!sdl_check_video_mode(graphics, width, height, &depth,
   sdl_flags(depth, fullscreen, false, resize) | SDL_ANYFORMAT))
    return false;

  // the YUV renderer _requires_ 32bit colour
  render_data->sdl.screen = SDL_SetVideoMode(width, height, 32,
   sdl_flags(depth, fullscreen, false, resize) | SDL_ANYFORMAT);

  if(!render_data->sdl.screen)
    goto err_free;

  // try with a YUY2 pixel format first
  render_data->rgb_to_yuv = rgb_to_yuy2;
  render_data->set_colors_mzx = yuy2_subsample_set_colors_mzx;
  render_data->sdl.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
   SDL_YUY2_OVERLAY, render_data->sdl.screen);

  // didn't work, try with a UYVY pixel format next
  if(!render_data->sdl.overlay)
  {
    render_data->rgb_to_yuv = rgb_to_uyvy;
    render_data->set_colors_mzx = uyvy_subsample_set_colors_mzx;
    render_data->sdl.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
     SDL_UYVY_OVERLAY, render_data->sdl.screen);
  }

  // Since we support this format now too, might as well try.
  if(!render_data->sdl.overlay)
  {
    render_data->rgb_to_yuv = rgb_to_yvyu;
    render_data->set_colors_mzx = yvyu_subsample_set_colors_mzx;
    render_data->sdl.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
     SDL_YVYU_OVERLAY, render_data->sdl.screen);
  }

  // failed to create an overlay
  if(!render_data->sdl.overlay)
    goto err_free;

  render_data->w = width;
  render_data->h = height;
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

static boolean yuv1_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  struct yuv_render_data *render_data = graphics->render_data;
  render_data->bpp = 32;

  return yuv_set_video_mode_size(graphics, width, height, depth, fullscreen,
   resize, YUV1_OVERLAY_WIDTH, YUV1_OVERLAY_HEIGHT);
}

static boolean yuv2_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
  struct yuv_render_data *render_data = graphics->render_data;
  render_data->bpp = 16;

  return yuv_set_video_mode_size(graphics, width, height, depth, fullscreen,
   resize, YUV2_OVERLAY_WIDTH, YUV2_OVERLAY_HEIGHT);
}

static boolean yuv_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct yuv_render_data *render_data = cmalloc(sizeof(struct yuv_render_data));
  if(!render_data)
    return false;

  memset(render_data, 0, sizeof(struct yuv_render_data));

  graphics->bits_per_pixel = 32;
  graphics->render_data = render_data;
  graphics->allow_resize = conf->allow_resize;
  graphics->ratio = conf->video_ratio;

  if(!set_video_mode())
  {
    free(render_data);
    graphics->render_data = NULL;
    return false;
  }

  return true;
}

static void yuv_free_video(struct graphics_data *graphics)
{
  sdl_destruct_window(graphics);

  free(graphics->render_data);
  graphics->render_data = NULL;
}

static void yuv_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct yuv_render_data *render_data = graphics->render_data;
  unsigned int i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
     render_data->rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b);
  }
}

static void yuv_lock_overlay(struct yuv_render_data *render_data,
 Uint32 **pixels, Uint32 *pitch)
{
  SDL_LockYUVOverlay(render_data->sdl.overlay);

  *pixels = (Uint32 *)render_data->sdl.overlay->pixels[0];
  *pitch = render_data->sdl.overlay->pitches[0];
}

static void yuv_unlock_overlay(struct yuv_render_data *render_data)
{
  SDL_UnlockYUVOverlay(render_data->sdl.overlay);
}

static void yuv_render_graph(struct graphics_data *graphics)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 mode = graphics->screen_mode;
  Uint32 bpp = render_data->bpp;
  Uint32 *pixels;
  Uint32 pitch;

  yuv_lock_overlay(render_data, &pixels, &pitch);

  if(bpp == 16)
  {
    // Subsampled mode
    if(!mode)
      render_graph16((Uint16 *)pixels, pitch, graphics,
       render_data->set_colors_mzx);
    else
      render_graph16((Uint16 *)pixels, pitch, graphics, set_colors32[mode]);
  }
  else
  {
    if(!mode)
      render_graph32(pixels, pitch, graphics);
    else
      render_graph32s(pixels, pitch, graphics);
  }

  yuv_unlock_overlay(render_data);
}

static void yuv1_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 bpp = render_data->bpp;
  Uint32 *pixels;
  Uint32 pitch;

  yuv_lock_overlay(render_data, &pixels, &pitch);

  render_layer(pixels, bpp, pitch, graphics, layer);

  yuv_unlock_overlay(render_data);
}

static void yuv_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 bpp = render_data->bpp;
  Uint32 *pixels;
  Uint32 pitch;

  yuv_lock_overlay(render_data, &pixels, &pitch);

  render_cursor(pixels, pitch, bpp, x, y,
   graphics->flat_intensity_palette[color], lines, offset);

  yuv_unlock_overlay(render_data);
}

static void yuv_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 bpp = render_data->bpp;
  Uint32 *pixels;
  Uint32 pitch;

  yuv_lock_overlay(render_data, &pixels, &pitch);

  render_mouse(pixels, pitch, bpp, x, y, 0xFFFFFFFF, 0x0, w, h);

  yuv_unlock_overlay(render_data);
}

static void yuv_sync_screen(struct graphics_data *graphics)
{
  struct yuv_render_data *render_data = graphics->render_data;
  int width = render_data->w, v_width;
  int height = render_data->h, v_height;
  SDL_Rect rect;

  // FIXME: Putting this here is suboptimal
  fix_viewport_ratio(width, height, &v_width, &v_height, graphics->ratio);

  rect.x = (width - v_width) >> 1;
  rect.y = (height - v_height) >> 1;
  rect.w = v_width;
  rect.h = v_height;

  SDL_DisplayYUVOverlay(render_data->sdl.overlay, &rect);
}

void render_yuv1_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = yuv_init_video;
  renderer->free_video = yuv_free_video;
  renderer->set_video_mode = yuv1_set_video_mode;
  renderer->update_colors = yuv_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_graph = yuv_render_graph;
  renderer->render_layer = yuv1_render_layer;
  renderer->render_cursor = yuv_render_cursor;
  renderer->render_mouse = yuv_render_mouse;
  renderer->sync_screen = yuv_sync_screen;
}

void render_yuv2_register(struct renderer *renderer)
{
  render_yuv1_register(renderer);
  renderer->set_video_mode = yuv2_set_video_mode;
  renderer->render_layer = NULL;
}
