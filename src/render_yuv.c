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
#include "yuv.h"

boolean yuv_set_video_mode_size(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize,
 int yuv_width, int yuv_height)
{
  struct yuv_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)

  sdl_destruct_window(graphics);

  render_data->sdl.window = SDL_CreateWindow("MegaZeux",
   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
   sdl_flags(depth, fullscreen, resize));

  if(!render_data->sdl.window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  render_data->sdl.renderer =
   SDL_CreateRenderer(render_data->sdl.window, -1, SDL_RENDERER_ACCELERATED);

  if(!render_data->sdl.renderer)
  {
    render_data->sdl.renderer =
     SDL_CreateRenderer(render_data->sdl.window, -1, SDL_RENDERER_SOFTWARE);

    if(!render_data->sdl.renderer)
    {
      warn("Failed to create renderer: %s\n", SDL_GetError());
      goto err_free;
    }

    warn("Accelerated renderer not available. Overlay will be SLOW!\n");
  }

  render_data->is_yuy2 = true;
  render_data->sdl.texture = SDL_CreateTexture(render_data->sdl.renderer,
   SDL_PIXELFORMAT_YUY2, SDL_TEXTUREACCESS_STREAMING, yuv_width, yuv_height);

  if(!render_data->sdl.texture)
  {
    render_data->is_yuy2 = false;
    render_data->sdl.texture = SDL_CreateTexture(render_data->sdl.renderer,
     SDL_PIXELFORMAT_UYVY, SDL_TEXTUREACCESS_STREAMING, yuv_width, yuv_height);

    if(!render_data->sdl.texture)
    {
      warn("Failed to create YUV texture: %s\n", SDL_GetError());
      goto err_free;
    }
  }

  sdl_window_id = SDL_GetWindowID(render_data->sdl.window);
  render_data->pitch = yuv_width * sizeof(Uint32);

#else // !SDL_VERSION_ATLEAST(2,0,0)

  // the YUV renderer _requires_ 32bit colour
  render_data->sdl.screen = SDL_SetVideoMode(width, height, 32,
   sdl_flags(depth, fullscreen, resize) | SDL_ANYFORMAT);

  if(!render_data->sdl.screen)
    goto err_free;

  // try with a YUY2 pixel format first
  render_data->sdl.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
    SDL_YUY2_OVERLAY, render_data->sdl.screen);

  // didn't work, try with a UYVY pixel format next
  if(!render_data->sdl.overlay)
    render_data->sdl.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
      SDL_UYVY_OVERLAY, render_data->sdl.screen);

  // failed to create an overlay
  if(!render_data->sdl.overlay)
    goto err_free;

  render_data->is_yuy2 = render_data->sdl.overlay->format == SDL_YUY2_OVERLAY;

#endif // !SDL_VERSION_ATLEAST(2,0,0)
  render_data->w = width;
  render_data->h = height;
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

boolean yuv_init_video(struct graphics_data *graphics, struct config_info *conf)
{
  struct yuv_render_data *render_data = cmalloc(sizeof(struct yuv_render_data));
  if(!render_data)
    return false;

  memset(render_data, 0, sizeof(struct yuv_render_data));

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

void yuv_free_video(struct graphics_data *graphics)
{
  sdl_destruct_window(graphics);

  free(graphics->render_data);
  graphics->render_data = NULL;
}

boolean yuv_check_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  return true;
#else
  return SDL_VideoModeOK(width, height, 32,
   sdl_flags(depth, fullscreen, resize) | SDL_ANYFORMAT);
#endif
}

void yuv_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 i;

  // it's a YUY2 overlay
  if(render_data->is_yuy2)
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
       rgb_to_yuy2(palette[i].r, palette[i].g, palette[i].b);
    }
  }

  // it's a UYVY overlay
  else
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
       rgb_to_uyvy(palette[i].r, palette[i].g, palette[i].b);
    }
  }
}

void yuv_sync_screen(struct graphics_data *graphics)
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

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_UpdateTexture(render_data->sdl.texture, NULL, render_data->pixels,
   render_data->pitch);
  SDL_RenderCopy(render_data->sdl.renderer, render_data->sdl.texture, NULL,
   &rect);
  SDL_RenderPresent(render_data->sdl.renderer);
#else
  SDL_DisplayYUVOverlay(render_data->sdl.overlay, &rect);
#endif
}

void yuv_lock_overlay(struct yuv_render_data *render_data)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  /**
   * This presents problems for layer rendering, so it's disabled.
   * First, this is a write-only operation and old data is not guaranteed to
   * exist in the returned buffer, so for transparent chars to work there'd
   * need to be a backup of the drawing area anyway. Second, calculating the
   * rect for this may or may not be intuitive with layer rendering.
   *
   * To enable, change the pixels array in yuv_render_data to a pointer.
   * Also preferably get an SDL_Rect here.
   */

  //SDL_LockTexture(render_data->sdl.texture, NULL,
  // &(render_data->pixels), &(render_data->pitch));
#else
  SDL_LockYUVOverlay(render_data->sdl.overlay);
#endif
}

void yuv_unlock_overlay(struct yuv_render_data *render_data)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  // See note above.
  //SDL_UnlockTexture(render_data->sdl.texture)
#else
  SDL_UnlockYUVOverlay(render_data->sdl.overlay);
#endif
}

Uint32 *yuv_get_pixels_pitch(struct yuv_render_data *render_data, int *pitch)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  *pitch = render_data->pitch;
  return render_data->pixels;
#else
  *pitch = render_data->sdl.overlay->pitches[0];
  return (Uint32 *)render_data->sdl.overlay->pixels[0];
#endif
}
