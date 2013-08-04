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

#include "platform.h"
#include "graphics.h"
#include "render.h"
#include "render_sdl.h"
#include "render_yuv.h"

bool yuv_set_video_mode_size(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize,
 int yuv_width, int yuv_height)
{
  struct yuv_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  if(render_data->renderer)
    SDL_DestroyRenderer(render_data->renderer);

  if(render_data->window)
    SDL_DestroyWindow(render_data->window);

  render_data->window = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   sdl_flags(depth, fullscreen, resize));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  render_data->renderer =
   SDL_CreateRenderer(render_data->window, -1, SDL_RENDERER_ACCELERATED);

  if(!render_data->renderer)
  {
    render_data->renderer =
     SDL_CreateRenderer(render_data->window, -1, SDL_RENDERER_SOFTWARE);

    if(!render_data->renderer)
    {
      warn("Failed to create renderer: %s\n", SDL_GetError());
      goto err_destroy_window;
    }

    warn("Accelerated renderer not available. Overlay will be SLOW!\n");
  }

  render_data->screen = SDL_GetWindowSurface(render_data->window);

  if(!render_data->screen)
  {
    /* Sometimes the accelerated renderer will be created, but doesn't
     * allow the surface to be returned. This seems like an SDL bug, but
     * we'll work around it here.
     */
    SDL_DestroyRenderer(render_data->renderer);

    render_data->renderer =
     SDL_CreateRenderer(render_data->window, -1, SDL_RENDERER_SOFTWARE);

    if(!render_data->renderer)
    {
      warn("Failed to create renderer: %s\n", SDL_GetError());
      goto err_destroy_window;
    }

    render_data->screen = SDL_GetWindowSurface(render_data->window);

    if(!render_data->screen)
    {
      warn("Failed to get window surface: %s\n", SDL_GetError());
      goto err_destroy_renderer;
    }

    warn("Accelerated renderer not available. Overlay will be SLOW!\n");
  }

  if(render_data->screen->format->BitsPerPixel != 32)
  {
    warn("Failed to get 32-bit window surface\n");
    goto err_destroy_renderer;
  }

  render_data->is_yuy2 = true;
  render_data->texture = SDL_CreateTexture(render_data->renderer,
   SDL_PIXELFORMAT_YUY2, SDL_TEXTUREACCESS_STREAMING, yuv_width, yuv_height);

  if(!render_data->texture)
  {
    render_data->is_yuy2 = false;
    render_data->texture = SDL_CreateTexture(render_data->renderer,
     SDL_PIXELFORMAT_UYVY, SDL_TEXTUREACCESS_STREAMING, yuv_width, yuv_height);

    if(!render_data->texture)
    {
      warn("Failed to create YUV texture: %s\n", SDL_GetError());
      goto err_destroy_renderer;
    }
  }
#else // !SDL_VERSION_ATLEAST(2,0,0)
  // the YUV renderer _requires_ 32bit colour
  render_data->screen = SDL_SetVideoMode(width, height, 32,
   sdl_flags(depth, fullscreen, resize) | SDL_ANYFORMAT);

  if(!render_data->screen)
    goto err_free;

  // try with a YUY2 pixel format first
  render_data->overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
    SDL_YUY2_OVERLAY, render_data->screen);

  // didn't work, try with a UYVY pixel format next
  if(!render_data->overlay)
    render_data->overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
      SDL_UYVY_OVERLAY, render_data->screen);

  // failed to create an overlay
  if(!render_data->overlay)
    goto err_free;

  render_data->is_yuy2 = render_data->overlay->format == SDL_YUY2_OVERLAY;
#endif // !SDL_VERSION_ATLEAST(2,0,0)
  return true;

#if SDL_VERSION_ATLEAST(2,0,0)
err_destroy_renderer:
  SDL_DestroyRenderer(render_data->renderer);
  render_data->renderer = NULL;
err_destroy_window:
  SDL_DestroyWindow(render_data->window);
  render_data->window = NULL;
#endif // SDL_VERSION_ATLEAST(2,0,0)
err_free:
  free(render_data);
  return false;
}

bool yuv_init_video(struct graphics_data *graphics, struct config_info *conf)
{
  struct yuv_render_data *render_data = cmalloc(sizeof(struct yuv_render_data));
  if(!render_data)
    return false;

  memset(render_data, 0, sizeof(struct yuv_render_data));

  graphics->render_data = render_data;
  graphics->allow_resize = conf->allow_resize;
  render_data->ratio = conf->video_ratio;

  if(!set_video_mode())
    return false;

  return true;
}

void yuv_free_video(struct graphics_data *graphics)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  struct yuv_render_data *render_data = graphics->render_data;
  SDL_DestroyTexture(render_data->texture);
  SDL_DestroyRenderer(render_data->renderer);
  SDL_DestroyWindow(render_data->window);
#endif
  free(graphics->render_data);
  graphics->render_data = NULL;
}

bool yuv_check_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  return true;
#else
  return SDL_VideoModeOK(width, height, 32,
   sdl_flags(depth, fullscreen, resize) | SDL_ANYFORMAT);
#endif
}

static inline void rgb_to_yuv(Uint8 r, Uint8 g, Uint8 b,
                              Uint8 *y, Uint8 *u, Uint8 *v)
{
  *y = (9797 * r + 19237 * g + 3734 * b) >> 15;
  *u = ((18492 * (b - *y)) >> 15) + 128;
  *v = ((23372 * (r - *y)) >> 15) + 128;
}

void yuv_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint8 y, u, v;
  Uint32 i;

  // it's a YUY2 overlay
  if(render_data->is_yuy2)
  {
    for(i = 0; i < count; i++)
    {
      rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b, &y, &u, &v);

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
      graphics->flat_intensity_palette[i] =
        (v << 0) | (y << 8) | (u << 16) | (y << 24);
#else
      graphics->flat_intensity_palette[i] =
        (y << 0) | (u << 8) | (y << 16) | (v << 24);
#endif
    }
  }

  // it's a UYVY overlay
  else
  {
    for(i = 0; i < count; i++)
    {
      rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b, &y, &u, &v);

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
      graphics->flat_intensity_palette[i] =
        (y << 0) | (v << 8) | (y << 16) | (u << 24);
#else
      graphics->flat_intensity_palette[i] =
        (u << 0) | (y << 8) | (v << 16) | (y << 24);
#endif
    }
  }
}

void yuv_sync_screen(struct graphics_data *graphics)
{
  struct yuv_render_data *render_data = graphics->render_data;
  int width = render_data->screen->w, v_width;
  int height = render_data->screen->h, v_height;
  SDL_Rect rect;

  // FIXME: Putting this here is suboptimal
  fix_viewport_ratio(width, height, &v_width, &v_height, render_data->ratio);

  rect.x = (width - v_width) >> 1;
  rect.y = (height - v_height) >> 1;
  rect.w = v_width;
  rect.h = v_height;

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_UpdateTexture(render_data->texture, NULL, render_data->screen->pixels,
   render_data->screen->pitch);
  SDL_RenderCopy(render_data->renderer, render_data->texture, NULL, &rect);
  SDL_RenderPresent(render_data->renderer);
#else
  SDL_DisplayYUVOverlay(render_data->overlay, &rect);
#endif
}

void yuv_lock_overlay(struct yuv_render_data *render_data)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_LockSurface(render_data->screen);
#else
  SDL_LockYUVOverlay(render_data->overlay);
#endif
}

void yuv_unlock_overlay(struct yuv_render_data *render_data)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_UnlockSurface(render_data->screen);
#else
  SDL_UnlockYUVOverlay(render_data->overlay);
#endif
}

Uint32 *yuv_get_pixels_pitch(struct yuv_render_data *render_data, int *pitch)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  *pitch = render_data->screen->pitch;
  return (Uint32 *)render_data->screen->pixels;
#else
  *pitch = render_data->overlay->pitches[0];
  return (Uint32 *)render_data->overlay->pixels[0];
#endif
}
