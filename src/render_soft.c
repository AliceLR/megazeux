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

#include <stdlib.h>

#include "graphics.h"
#include "render.h"
#include "render_layer.h"
#include "renderers.h"
#include "util.h"

#ifdef CONFIG_SDL
#include <SDL.h>
#include "render_sdl.h"
#endif

struct soft_render_data
{
#ifdef CONFIG_SDL
  struct sdl_render_data sdl;
  SDL_Color sdlpal[SMZX_PAL_SIZE];
#else
  unsigned pitch;
  unsigned bpp;
  uint8_t buffer[SCREEN_PIX_W * SCREEN_PIX_H];
#endif
};

#ifdef CONFIG_SDL

static SDL_Surface *soft_get_screen_surface(struct soft_render_data *_render_data)
{
  struct sdl_render_data *render_data = &(_render_data->sdl);
  return render_data->shadow ? render_data->shadow : render_data->screen;
}

static void soft_lock_buffer(struct soft_render_data *render_data,
 uint32_t **pixels, unsigned *pitch, unsigned *bpp, uint32_t *amask)
{
  SDL_Surface *screen = soft_get_screen_surface(render_data);

  *pixels = (uint32_t *)screen->pixels;
  *pitch = screen->pitch;
  *bpp = screen->format->BytesPerPixel * 8;

  *pixels += *pitch * ((screen->h - 350) / 8);
  *pixels += (screen->w - 640) * *bpp / 64;

  if(amask)
    *amask = screen->format->Amask;

  SDL_LockSurface(screen);
}

static void soft_unlock_buffer(struct soft_render_data *render_data)
{
  SDL_Surface *screen = soft_get_screen_surface(render_data);
  SDL_UnlockSurface(screen);
}

#else /* !CONFIG_SDL */

static void soft_lock_buffer(struct soft_render_data *render_data,
 uint32_t **pixels, unsigned *pitch, unsigned *bpp, uint32_t *amask)
{
  *pixels = (uint32_t *)render_data->buffer;
  *pitch = render_data->pitch;
  *bpp = render_data->bpp;
  if(amask)
    *amask = 0;
}

static void soft_unlock_buffer(struct soft_render_data *render_data)
{
  // do nothing
}

#endif /* !CONFIG_SDL */

static boolean soft_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct soft_render_data *render_data =
   (struct soft_render_data *)ccalloc(1, sizeof(struct soft_render_data));
  graphics->render_data = render_data;

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 32;

  // Screens smaller than 640x350 do weird things with the software renderer
  if(graphics->resolution_width < 640)
    graphics->resolution_width = 640;
  if(graphics->resolution_height < 350)
    graphics->resolution_height = 350;
  if(graphics->window_width < 640)
    graphics->window_width = 640;
  if(graphics->window_height < 350)
    graphics->window_height = 350;

  // We have 8-bit, 16-bit, and 32-bit software renderers
  if(conf->force_bpp == BPP_AUTO || conf->force_bpp == 8 ||
   conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  return set_video_mode();
}

static void soft_free_video(struct graphics_data *graphics)
{
#ifdef CONFIG_SDL
  sdl_destruct_window(graphics);
#endif

  free(graphics->render_data);
  graphics->render_data = NULL;
}

static boolean soft_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, boolean fullscreen, boolean resize)
{
#ifdef CONFIG_SDL
  return sdl_set_video_mode(graphics, width, height, depth, fullscreen, resize);
#else

  struct soft_render_data *render_data = graphics->render_data;

  graphics->bits_per_pixel = 8;
  render_data->pitch = (graphics->bits_per_pixel / 8) * SCREEN_PIX_W;
  render_data->bpp = graphics->bits_per_pixel;

  graphics->renderer_is_headless = true;
  return true;

#endif /* !CONFIG_SDL */
}

static void soft_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
#ifdef CONFIG_SDL
  struct soft_render_data *render_data = graphics->render_data;
  SDL_Surface *screen = soft_get_screen_surface(render_data);

  unsigned int i;

  if(graphics->bits_per_pixel != 8)
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
       SDL_MapRGBA(screen->format, palette[i].r, palette[i].g, palette[i].b,
       255);
    }
  }
  else
  {
    if(count > 256)
      count = 256;

    for(i = 0; i < count; i++)
    {
      render_data->sdlpal[i].r = palette[i].r;
      render_data->sdlpal[i].g = palette[i].g;
      render_data->sdlpal[i].b = palette[i].b;
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    SDL_SetPaletteColors(render_data->sdl.palette, render_data->sdlpal, 0, count);
#else
    SDL_SetColors(render_data->sdl.screen, render_data->sdlpal, 0, count);
#endif
  }
#endif /* CONFIG_SDL */
}

static void soft_render_graph(struct graphics_data *graphics)
{
  struct soft_render_data *render_data = graphics->render_data;

  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;
  unsigned int mode = graphics->screen_mode;

  soft_lock_buffer(render_data, &pixels, &pitch, &bpp, NULL);

  if(bpp == 8)
  {
    render_graph8((uint8_t *)pixels, pitch, graphics, set_colors8[mode]);
  }
  else

  if(bpp == 16)
  {
    render_graph16((uint16_t *)pixels, pitch, graphics, set_colors16[mode]);
  }
  else

  if(bpp == 32)
  {
    if(!mode)
      render_graph32(pixels, pitch, graphics);
    else
      render_graph32s(pixels, pitch, graphics);

    /* This just adds a 3x3 red box to the top left of the screen
       It's useful for debugging because it indicates when the
       fallback renderer is used

    *(pixels + 0) = 0xFFFF0000;
    *(pixels + 1) = 0xFFFF0000;
    *(pixels + 2) = 0xFFFF0000;
    *(pixels + (pitch/4)) = 0xFFFF0000;
    *(pixels + (pitch/4) + 1) = 0xFFFF0000;
    *(pixels + (pitch/4) + 2) = 0xFFFF0000;
    *(pixels + (pitch/2)) = 0xFFFF0000;
    *(pixels + (pitch/2) + 1) = 0xFFFF0000;
    *(pixels + (pitch/2) + 2) = 0xFFFF0000;
    */
  }
  soft_unlock_buffer(render_data);
}

static void soft_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct soft_render_data *render_data = graphics->render_data;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  soft_lock_buffer(render_data, &pixels, &pitch, &bpp, NULL);
  render_layer(pixels, bpp, pitch, graphics, layer);
  soft_unlock_buffer(render_data);
}

static void soft_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct soft_render_data *render_data = graphics->render_data;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;
  uint32_t flatcolor;

  soft_lock_buffer(render_data, &pixels, &pitch, &bpp, NULL);

  if(bpp == 8)
  {
    color &= 0xFF;
    flatcolor = (color << 24) | (color << 16) | (color << 8) | color;
  }
  else
  {
    flatcolor = graphics->flat_intensity_palette[color];
  }

  if(bpp == 16)
    flatcolor |= flatcolor << 16;

  render_cursor(pixels, pitch, bpp, x, y, flatcolor, lines, offset);
  soft_unlock_buffer(render_data);
}

static void soft_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct soft_render_data *render_data = graphics->render_data;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;
  uint32_t mask, amask;

  soft_lock_buffer(render_data, &pixels, &pitch, &bpp, &amask);

  if((bpp == 8) && !graphics->screen_mode)
    mask = 0x0F0F0F0F;
  else
    mask = 0xFFFFFFFF;

  render_mouse(pixels, pitch, bpp, x, y, mask, amask, w, h);
  soft_unlock_buffer(render_data);
}

static void soft_sync_screen(struct graphics_data *graphics)
{
#ifdef CONFIG_SDL
  struct sdl_render_data *render_data = graphics->render_data;

  if(render_data->shadow)
  {
    SDL_BlitSurface(render_data->shadow,
     &render_data->shadow->clip_rect, render_data->screen,
     &render_data->screen->clip_rect);
  }

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_UpdateWindowSurface(render_data->window);
#else
  SDL_Flip(render_data->screen);
#endif
#endif /* CONFIG_SDL */
}

void render_soft_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = soft_init_video;
  renderer->free_video = soft_free_video;
  renderer->set_video_mode = soft_set_video_mode;
  renderer->update_colors = soft_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->get_screen_coords = get_screen_coords_centered;
  renderer->set_screen_coords = set_screen_coords_centered;
  renderer->render_graph = soft_render_graph;
  renderer->render_layer = soft_render_layer;
  renderer->render_cursor = soft_render_cursor;
  renderer->render_mouse = soft_render_mouse;
  renderer->sync_screen = soft_sync_screen;
}
