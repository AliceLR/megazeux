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

#include "config.h"
#include "graphics.h"
#include "render.h"
#include "renderers.h"

static void (*set_colors8[4])(graphics_data *, Uint32 *, Uint8, Uint8) =
{
  set_colors8_mzx,
  set_colors8_smzx,
  set_colors8_smzx,
  set_colors8_smzx3
};

static void (*set_colors16[4])(graphics_data *, Uint32 *, Uint8, Uint8) =
{
  set_colors16_mzx,
  set_colors16_smzx,
  set_colors16_smzx,
  set_colors16_smzx3
};

static void (*set_colors32[4])(graphics_data *, Uint32 *, Uint8, Uint8) =
{
  set_colors32_mzx,
  set_colors32_smzx,
  set_colors32_smzx,
  set_colors32_smzx3
};

static int soft_init_video(graphics_data *graphics, config_info *conf)
{
  graphics->allow_resize = 0;

  graphics->bits_per_pixel = 32;

  // Screens smaller than 640x350 do weird things with the software renderer
  if (graphics->resolution_width < 640)
    graphics->resolution_width = 640;
  if (graphics->resolution_height < 350)
    graphics->resolution_height = 350;
  if (graphics->window_width < 640)
    graphics->window_width = 640;
  if (graphics->window_height < 350)
    graphics->window_height = 350;

  // We have 8-bit, 16-bit, and 32-bit software renderers
  if (conf->force_bpp == 8 || conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  return set_video_mode();
}

static int soft_check_video_mode(graphics_data *graphics, int width, int height,
 int depth, int flags)
{
  return SDL_VideoModeOK(width, height, depth, flags);
}

static int soft_set_video_mode(graphics_data *graphics, int width, int height,
 int depth, int flags, int fullscreen)
{
  graphics->render_data = SDL_SetVideoMode(width, height, depth, flags);
  return graphics->render_data != NULL;
}

static void soft_update_colors(graphics_data *graphics, SDL_Color *palette,
 Uint32 count)
{
  SDL_Surface *screen = graphics->render_data;
  Uint32 i;

  if (graphics->bits_per_pixel != 8)
  {
    for (i = 0; i < count; i++)
      graphics->flat_intensity_palette[i] =
       SDL_MapRGBA(screen->format, palette[i].r, palette[i].g, palette[i].b,
       255);
  }
  else
  {
    SDL_SetColors(screen, palette, 0, count);
  }
}

static void soft_render_graph(graphics_data *graphics)
{
  SDL_Surface *screen = graphics->render_data;
  Uint32 *pixels = (Uint32 *)screen->pixels;
  Uint32 pitch = screen->pitch;
  Uint32 bpp = graphics->bits_per_pixel;
  Uint32 mode = graphics->screen_mode;

  pixels += pitch * ((screen->h - 350) / 8);
  pixels += (screen->w - 640) / bpp;

  SDL_LockSurface(screen);
  if (bpp == 8)
    render_graph8((Uint8 *)pixels, pitch, graphics, set_colors8[mode]);
  else

  if (bpp == 16)
    render_graph16((Uint16 *)pixels, pitch, graphics, set_colors16[mode]);
  else

  if (bpp == 32)
  {
    if(!mode)
        render_graph32(pixels, pitch, graphics, set_colors32[mode]);
      else
        render_graph32s(pixels, pitch, graphics, set_colors32[mode]);
  }
  SDL_UnlockSurface(screen);
}

static void soft_render_cursor(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 color, Uint8 lines, Uint8 offset)
{
  SDL_Surface *screen = graphics->render_data;
  Uint32 *pixels = (Uint32 *)screen->pixels;
  Uint32 pitch = screen->pitch;
  Uint32 bpp = graphics->bits_per_pixel;
  Uint32 flatcolor;

  pixels += pitch * ((screen->h - 350) / 8);
  pixels += (screen->w - 640) / bpp;

  if (bpp == 8)
    flatcolor = (color << 24) | (color << 16) | (color << 8) | color;
  else
    flatcolor = graphics->flat_intensity_palette[color];

  if (bpp == 16)
    flatcolor |= flatcolor << 16;

  SDL_LockSurface(screen);
  render_cursor(pixels, pitch, bpp, x, y, flatcolor, lines, offset);
  SDL_UnlockSurface(screen);
}

static void soft_render_mouse(graphics_data *graphics, Uint32 x, Uint32 y,
 Uint8 w, Uint8 h)
{
  SDL_Surface *screen = graphics->render_data;
  Uint32 *pixels = (Uint32 *)screen->pixels;
  Uint32 pitch = screen->pitch;
  Uint32 bpp = graphics->bits_per_pixel;
  Uint32 mask;

  pixels += pitch * ((screen->h - 350) / 8);
  pixels += (screen->w - 640) / bpp;

  if ((bpp == 8) && !graphics->screen_mode)
    mask = 0x0F0F0F0F;
  else
    mask = 0xFFFFFFFF;

  SDL_LockSurface(screen);
  render_mouse(pixels, pitch, bpp, x, y, mask, w, h);
  SDL_UnlockSurface(screen);
}

static void soft_sync_screen(graphics_data *graphics)
{
  SDL_Flip(graphics->render_data);
}

void render_soft_register(graphics_data *graphics)
{
  graphics->init_video = soft_init_video;
  graphics->check_video_mode = soft_check_video_mode;
  graphics->set_video_mode = soft_set_video_mode;
  graphics->update_colors = soft_update_colors;
  graphics->resize_screen = resize_screen_standard;
  graphics->remap_charsets = NULL;
  graphics->remap_char = NULL;
  graphics->remap_charbyte = NULL;
  graphics->get_screen_coords = get_screen_coords_centered;
  graphics->set_screen_coords = set_screen_coords_centered;
  graphics->render_graph = soft_render_graph;
  graphics->render_cursor = soft_render_cursor;
  graphics->render_mouse = soft_render_mouse;
  graphics->sync_screen = soft_sync_screen;
}
